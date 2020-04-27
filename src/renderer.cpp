#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "entity.h"

using namespace GTR;

//render all the scene
void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	for (auto prefab : scene->prefabs)
	{
		renderPrefab(prefab->model, prefab->prefab, camera);
	}
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			//render node mesh
			renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	texture = material->color_texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;

	//select the blending
	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	//chose a shader
	if (texture) {
		std::vector<Light*> scene_lights = Scene::instance->lights;
		if (!scene_lights.empty())	shader = Shader::Get("phong");
		else shader = Shader::Get("texture");
	} else
		shader = Shader::Get("flat");

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	std::vector<Light*> scene_lights = Scene::instance->lights;

	if (!scene_lights.empty()) {
		
		bool is_first_pass = true;
		for (auto light : scene_lights)		// MULTI PASS
		{
			// skip iteration if light is far from mesh && light is point light && is not first pass (bc first pass must be done to paint the mesh with ambient light))
			if (!mesh->testSphereCollision(model, light->model.getTranslation(), light->max_distance, Vector3(0, 0, 0), Vector3(0, 0, 0)) && light->light_type == GTR::POINT && !is_first_pass)
				continue;
			
			glEnable(GL_BLEND);
			if (material->alpha_mode == GTR::AlphaMode::BLEND)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else
			{
				if (is_first_pass)
					glDisable(GL_BLEND);
				else {
					glDepthFunc(GL_LEQUAL);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				}
			}

			light->setUniforms(shader);

			shader->setUniform("u_has_occlusion_texture", false);
			shader->setUniform("u_has_emissive_texture", false);

			if (texture)
				shader->setUniform("u_texture", texture, 0);

			if (material->emissive_texture) {
				shader->setUniform("u_emissive_texture", material->emissive_texture, 1);
				shader->setUniform("u_has_emissive_texture", true);
			}

			if (material->occlusion_texture) {
				shader->setUniform("u_occlusion_texture", material->occlusion_texture, 2);
				shader->setUniform("u_has_occlusion_texture", true);
			}

			shader->setUniform("u_ambient_light", Scene::instance->ambientLight * is_first_pass);
			shader->setUniform("u_is_first_pass", is_first_pass);
			shader->setUniform("u_model", model);

			shader->setUniform("u_color", material->color);
			shader->setUniform("u_tiles_number", material->tiles_number);

			//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
			shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

			//do the draw call that renders the mesh into the screen
			mesh->render(GL_TRIANGLES);
			is_first_pass = false;
		}
	}
	else {
		shader->setUniform("u_model", model);
		shader->setUniform("u_color", material->color);
		if (texture)
			shader->setUniform("u_texture", texture, 0);

		//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);

	}

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); //as default

}