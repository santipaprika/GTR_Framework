#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "entity.h"
#include "application.h"

using namespace GTR;

//render all the scene to viewport
void Renderer::renderSceneToScreen(GTR::Scene* scene, Camera* camera, Vector4 bg_color)
{
	Application* application = Application::instance;
	application->rendering_shadowmap = false;

	//be sure no errors present in opengl before start
	checkGLErrors();

	//set the clear color 
	glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w);

	//set the camera as default (used by some functions in the framework)
	camera->enable();

	setDefaultGLFlags();

	if (application->render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	renderScene(scene, camera);

	if (application->render_grid)
		drawGrid();
}

std::vector<Light*> Renderer::renderSceneShadowmaps(GTR::Scene* scene)
{
	Application::instance->rendering_shadowmap = true;

	std::vector<Light*> shadow_casting_lights;

	//find lights that cast shadow (forced to spot atm)

	for (auto light : GTR::Scene::instance->lights) {
		if (!light->cast_shadows) continue;

		light->shadow_fbo->bind();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//be sure no errors present in opengl before start
		checkGLErrors();
		setDefaultGLFlags();

		Camera* light_cam = light->camera;
		Vector3 light_center = light->model.getTranslation();
		light_cam->enable();

		if (light->light_type == GTR::POINT)
			renderPointShadowmap(light);
		else
			renderScene(scene, light_cam);
		
		light->shadow_fbo->unbind();
		shadow_casting_lights.push_back(light);
	}

	return shadow_casting_lights;
}

void Renderer::renderPointShadowmap(Light* light)
{
	Camera* light_cam = light->camera;
	Vector3 light_center = light->model.getTranslation();

	int width = light->shadow_fbo->width / 6.0;
	int height = light->shadow_fbo->height;
	Vector3 directions[6] = { Vector3(-1,0,0), Vector3(1,0,0), Vector3(0,-1,0), Vector3(0,1,0), Vector3(0,0,-1), Vector3(0,0,1) };
	for (int i = 0; i < 6; i++) {
		Vector3 topVec = Vector3(0, 1, 0);
		if (directions[i].y != 0)
			topVec.set(0, 0, 1);
		light_cam->lookAt(light_center, light_center + directions[i], topVec);
		light->shadow_viewprojs[i] = light_cam->viewprojection_matrix;

		glViewport(width * i, 0, width, height);
		light_cam->enable();
		renderScene(GTR::Scene::instance, light_cam);
	}
	glViewport(0, 0, Application::instance->window_width, Application::instance->window_height);
}

void Renderer::showSceneShadowmaps(std::vector<Light*> shadow_caster_lights)
{
	int num_points = 0;
	for (int i = 0; i < shadow_caster_lights.size(); i++)
	{
		GTR::Light* light = shadow_caster_lights[i];
		glDisable(GL_DEPTH_TEST);
		Shader* shader = Shader::Get("depth");
		shader->enable();
		shader->setUniform("u_camera_nearfar", Vector2(light->camera->near_plane, light->camera->far_plane));
		if (light->light_type == GTR::POINT) {
			glViewport(0, 200 + 150 * num_points, 150 * 6, 150);
			num_points++;
		}
		else
			glViewport(i * 200, 0, 200, 200);

		if (light->light_type == GTR::DIRECTIONAL)	// ortographic
			light->shadow_fbo->depth_texture->toViewport();
		else
			light->shadow_fbo->depth_texture->toViewport(shader);

		glViewport(0, 0, Application::instance->window_width, Application::instance->window_height);
	}
}

void Renderer::setDefaultGLFlags() 
{
	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set default flags
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

//render all the scene
void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	for (auto prefabEnt : scene->prefabs)
	{
		renderPrefab(prefabEnt->model, prefabEnt->prefab, camera);
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

void Renderer::manageBlendingAndCulling(GTR::Material* material, bool rendering_light, bool is_first_pass)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (material->alpha_mode != GTR::BLEND && (is_first_pass || !rendering_light))
		glDisable(GL_BLEND);

	//select the blending
	if (!is_first_pass)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
		glDepthFunc(GL_LEQUAL);
	}

	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);
}

bool Renderer::renderShadowMap(Shader* &shader, Material* material, Camera* camera, Matrix44 model, Mesh* mesh)
{
	shader = Shader::Get("flat");
	assert(glGetError() == GL_NO_ERROR);

	if (material->alpha_mode == GTR::BLEND)
		return false;

	shader->enable();

	shader->setUniform("u_camera_pos", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	mesh->render(GL_TRIANGLES);

	return true;
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);


	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	std::vector<Light*> scene_lights = Scene::instance->lights;

	if (Application::instance->rendering_shadowmap) {
		if (!renderShadowMap(shader, material, camera, model, mesh)) return;
	}
	else {
		texture = material->color_texture;

		if (!texture) texture = Texture::getWhiteTexture();

		shader = scene_lights.empty() ? Shader::Get("texture") : Shader::Get("phong");
		assert(glGetError() == GL_NO_ERROR);

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();

		//upload uniforms
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);

		if (!scene_lights.empty()) {

			bool is_first_pass = true;
			for (auto light : scene_lights)		// MULTI PASS
			{
				// skip iteration if light is far from mesh && light is point light && is not first pass (bc first pass must be done to paint the mesh with ambient light))
				if (!mesh->testSphereCollision(model, light->model.getTranslation(), light->max_distance, Vector3(0, 0, 0), Vector3(0, 0, 0)) && light->light_type == GTR::POINT && !is_first_pass)
					continue;

				if (!light->camera->testBoxInFrustum(mesh->box.center, mesh->box.halfsize) && light->light_type == GTR::SPOT && !is_first_pass)
					continue;

				manageBlendingAndCulling(material, true, is_first_pass);

				light->setUniforms(shader);	//DO BEFORE MATERIAL SET UNIFORMS
				material->setUniforms(shader);

				shader->setUniform("u_ambient_light", Scene::instance->ambientLight * is_first_pass);
				shader->setUniform("u_is_first_pass", is_first_pass);
				shader->setUniform("u_model", model);
				
				//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
				shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

				//do the draw call that renders the mesh into the screen
				mesh->render(GL_TRIANGLES);
				is_first_pass = false;
			}
		}
		else {	//NO LIGHTS
			manageBlendingAndCulling(material, false);

			shader->setUniform("u_model", model);
			shader->setUniform("u_color", material->color);
			if (texture)
				shader->setUniform("u_texture", texture, 0);

			//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
			shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

			//do the draw call that renders the mesh into the screen
			mesh->render(GL_TRIANGLES);

		}
	}

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); //as default

}