#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "BaseEntity.h"
#include "scene.h"
#include "application.h"
#include "extra/hdre.h"

using namespace GTR;


//DEFERRED
void Renderer::renderGBuffers(Scene* scene, Camera* camera)
{
	FBO* gbuffers_fbo = Application::instance->gbuffers_fbo;

	gbuffers_fbo->bind();
	//we clear in several passes so we can control the clear color independently for every gbuffer

	//disable all but the GB0 (and the depth)
	gbuffers_fbo->enableSingleBuffer(0);

	//clear GB0 with the color (and depth)
	glClearColor(scene->bg_color.x, scene->bg_color.y, scene->bg_color.z, scene->bg_color.w);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//and now enable the second GB to clear it to black
	gbuffers_fbo->enableSingleBuffer(1);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	gbuffers_fbo->enableSingleBuffer(2);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	//enable all buffers back
	gbuffers_fbo->enableAllBuffers();

	//first of all, render the skybox
	renderSkybox(camera, scene->environment);

	//render everything 
	renderScene(scene, camera);	// this will call renderToGBuffers for every mesh in the camera frustrum (scene)

	//stop rendering to the gbuffers
	gbuffers_fbo->unbind();
}

void Renderer::renderToGBuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	Shader* shader = Shader::Get("gbuffers");
	glEnable(GL_DEPTH_TEST);
	assert(glGetError() == GL_NO_ERROR);

	if (material->alpha_mode == GTR::BLEND)
		return;

	shader->enable();

	shader->setUniform("u_camera_pos", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

	shader->setUniform("u_use_gamma_correction", Application::instance->use_gamma_correction);

	material->setUniforms(shader, true);

	mesh->render(GL_TRIANGLES);

	shader->disable();
}

void Renderer::renderSSAO(Camera* camera)
{
	Application* application = Application::instance;

	Shader* shader = Shader::Get("ssao");
	shader->enable();

	////bind the texture we want to change
	application->gbuffers_fbo->bind();

	//disable using mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//enable bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	application->gbuffers_fbo->unbind();

	////bind the texture we want to change
	application->gbuffers_fbo->color_textures[1]->bind();

	//disable using mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//enable bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	application->gbuffers_fbo->color_textures[1]->unbind();

	//start rendering inside the ssao texture
	application->ssao_fbo->bind();

	application->ssao_fbo->enableSingleBuffer(0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	application->ssao_fbo->enableAllBuffers();

	//get the shader for SSAO (remember to create it using the atlas)
	

	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();

	//send info to reconstruct the world position
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	//we need the pixel size so we can center the samples 
	shader->setUniform("u_iRes", Vector2(1.0 / (float)application->gbuffers_fbo->depth_texture->width, 1.0 / (float)application->gbuffers_fbo->depth_texture->height));
	//we will need the viewprojection to obtain the uv in the depthtexture of any random position of our world
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

	shader->setTexture("u_depth_texture", application->gbuffers_fbo->depth_texture, 0);
	shader->setTexture("u_normal_texture", application->gbuffers_fbo->color_textures[1], 1);

	//send random points so we can fetch around
	shader->setUniform3Array("u_points", (float*)&application->random_points[0], application->random_points.size());
	shader->setUniform("u_radius", application->sphere_radius);

	//render fullscreen quad
	Mesh* quad = Mesh::getQuad();
	quad->render(GL_TRIANGLES);

	//stop rendering to the texture
	application->ssao_fbo->unbind();

	shader->disable();

	// Blur
	//get the shader for SSAO (remember to create it using the atlas)
	Shader* blur_shader = Shader::Get("blur");
	blur_shader->enable();

	blur_shader->setTexture("u_texture", application->ssao_fbo->color_textures[0], 0);
	blur_shader->setUniform("u_kernel_size", application->kernel_size);
	blur_shader->setUniform("u_offset", Vector2(1.0/application->ssao_fbo->color_textures[0]->width, 1.0/application->ssao_fbo->color_textures[0]->height));

	application->ssao_fbo->color_textures[0]->copyTo(application->ssao_blur, blur_shader);

	blur_shader->disable();

}

void Renderer::renderIlluminationToBuffer(Camera* camera)
{
	FBO* illumination_fbo = Application::instance->illumination_fbo;
	FBO* gbuffers_fbo = Application::instance->gbuffers_fbo;
	std::vector<Light*> scene_lights = Scene::instance->lights;
	Scene* scene = Scene::instance;

	//start rendering to the illumination fbo
	illumination_fbo->bind();

	//FIRST PASS DEFERRED RENDER
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	//clear to bg color (not working?)
	gbuffers_fbo->enableSingleBuffer(0);
	Vector4 bg_color = Scene::instance->bg_color;
	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	gbuffers_fbo->enableAllBuffers();

	//we need a fullscreen quad
	Mesh* quad = Mesh::getQuad();

	Shader* sh = Shader::Get("deferred");
	sh->enable();

	//pass the gbuffers to the shader
	sh->setTexture("u_color_texture", gbuffers_fbo->color_textures[0], 0);
	sh->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
	sh->setTexture("u_emissive_texture", gbuffers_fbo->color_textures[2], 2);
	sh->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);

	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();
	
	//pass the inverse projection of the camera to reconstruct world pos.
	sh->setUniform("u_inverse_viewprojection", inv_vp);
	//pass the inverse window resolution, this may be useful
	sh->setUniform("u_iRes", Vector2(1.0 / (float)Application::instance->window_width, 1.0 / (float)Application::instance->window_height));

	//pass all the information about the light and ambient�
	if (Application::instance->use_gamma_correction)
		sh->setUniform("u_ambient_light", gamma(scene->ambient_light) * scene->ambient_power);
	else
		sh->setUniform("u_ambient_light", scene->ambient_light * scene->ambient_power);

	sh->setUniform("u_use_ssao", Application::instance->use_ssao);
	if (Application::instance->use_ssao)
		sh->setTexture("u_ssao_texture", Application::instance->ssao_blur, 5);

	// irradiance uniforms
	scene->SetIrradianceUniforms(sh);

	quad->render(GL_TRIANGLES);
	sh->disable();

	// RENDER MULTI PASS
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	bool is_first_pass = true;
	for (auto light : scene_lights)
	{
		Shader* shader;
		if (light->light_type == GTR::DIRECTIONAL && Scene::instance->use_geometry_on_deferred) {
			Scene::instance->use_geometry_on_deferred = false;
			shader = chooseShader(light);
			Scene::instance->use_geometry_on_deferred = true;
		}
		else
			shader = chooseShader(light);	//this sets light uniforms too

		//pass the gbuffers to the shader
		shader->setTexture("u_color_texture", gbuffers_fbo->color_textures[0], 0);
		shader->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
		shader->setTexture("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
		shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);

		Matrix44 inv_vp_mp = camera->viewprojection_matrix;
		inv_vp_mp.inverse();

		//pass the inverse projection of the camera to reconstruct world pos.
		shader->setUniform("u_inverse_viewprojection", inv_vp_mp);
		//pass the inverse window resolution, this may be useful
		shader->setUniform("u_iRes", Vector2(1.0 / (float)Application::instance->window_width, 1.0 / (float)Application::instance->window_height));

		if (Scene::instance->use_geometry_on_deferred && light->light_type != GTR::DIRECTIONAL)
		{
			Mesh* light_geometry;

			switch (light->light_type)
			{
			case GTR::POINT:
				light_geometry = Mesh::Get("data/meshes/sphere.obj");
				break;
			case GTR::SPOT:
				light_geometry = Mesh::Get("data/meshes/sphere.obj");
				break;
			}

			//basic.vs will need the model and the viewproj of the camera
			shader->setUniform("u_camera_pos", camera->eye);
			shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

			//we must translate the model to the center of the light
			Matrix44 m;
			Vector3 light_pos = light->model.getTranslation();
			m.setTranslation(light_pos.x, light_pos.y, light_pos.z);
			//and scale it according to the max_distance of the light
			m.scale(light->max_distance, light->max_distance, light->max_distance);

			//pass the model to the shader to render the sphere
			shader->setUniform("u_model", m);

			if (Application::instance->current_pipeline == Application::DEFERRED)
			{
				sReflectionProbe* closest_probe;
				float min_dist = NULL;
				for (auto probe : scene->reflection_probes) //compute the nearest reflection probe
				{
					float curr_dist = Vector3(probe->pos - camera->eye).length();
					if (curr_dist < min_dist || min_dist == NULL)
					{
						min_dist = curr_dist;
						closest_probe = probe;
					}
				}
				shader->setTexture("u_cubemap_texture", closest_probe->cubemap, 13);
			}

			//render only the backfacing triangles of the sphere
			glEnable(GL_CULL_FACE);
			glFrontFace(GL_CW);

			if (scene->show_deferred_light_geometry) {
				glDisable(GL_DEPTH_TEST);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				if (Application::instance->use_gamma_correction)
					shader->setUniform("u_color", Vector4(gamma(light->color), 0.5));
				else
					shader->setUniform("u_color", Vector4(light->color, 0.5));

			}

			//and render the sphere
			light_geometry->render(GL_TRIANGLES);

			glFrontFace(GL_CCW);
		}
		else
			quad->render(GL_TRIANGLES);

		shader->disable();
	}

	// RENDER VOLUME SCATTERING
	if (scene->use_volumetric)
	{
		glDisable(GL_BLEND);

		Texture* noise = Texture::Get("data/textures/noise.png");
		noise->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		noise->unbind();

		Application::instance->volumetrics_fbo->bind();
		Shader* sh = Shader::Get("volumetric");
		sh->enable();
		
		Matrix44 m;
		sh->setUniform("u_model", m);
		sh->setUniform("u_camera_position", camera->eye);
		sh->setUniform("u_viewprojection", camera->viewprojection_matrix);

		sh->setUniform("u_quality", 64);
		sh->setTexture("u_noise_tex", noise, 2);
		sh->setUniform("u_random", vec3(random(), random(), random()));

		//pass the inverse projection of the camera to reconstruct world pos.
		sh->setUniform("u_inverse_viewprojection", inv_vp);
		sh->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 1);
		sh->setUniform("u_iRes", Vector2(1.0 / gbuffers_fbo->width, 1.0 / gbuffers_fbo->height));

		Light* sun = scene_lights[0];
		sh->setUniform("u_light_color", gamma(sun->color));
		sh->setUniform("u_light_type", sun->light_type);
		sh->setUniform("u_shadow_viewproj", sun->camera->viewprojection_matrix);
		sh->setTexture("u_shadowmap", sun->shadow_fbo->depth_texture, 8);
		sh->setUniform("u_shadow_bias", (float)sun->shadow_bias);

		quad->render(GL_TRIANGLES);
		sh->disable();
		Application::instance->volumetrics_fbo->unbind();

		illumination_fbo->bind();
		glDisable(GL_BLEND);
	}
}

void Renderer::showGBuffers()
{
	Application* application = Application::instance;
	float window_width = application->window_width;
	float window_height = application->window_height;

	glDisable(GL_DEPTH_TEST);

	glViewport(0, window_height * 0.5, window_width * 0.5, window_height * 0.5);
	if (application->use_gamma_correction)
		application->gbuffers_fbo->color_textures[0]->toViewport(Shader::Get("degammaDeferred"));
	else
		application->gbuffers_fbo->color_textures[0]->toViewport();

	glViewport(window_width * 0.5, window_height * 0.5, window_width * 0.5, window_height * 0.5);
	application->gbuffers_fbo->color_textures[1]->toViewport();

	glViewport(0, 0, window_width * 0.5, window_height * 0.5);
	application->gbuffers_fbo->color_textures[2]->toViewport();

	Shader* shader = Shader::Get("depth");
	shader->enable();
	shader->setUniform("u_camera_nearfar", Vector2(application->camera->near_plane, application->camera->far_plane));
	glViewport(window_width * 0.5, 0, window_width * 0.5, window_height * 0.5);
	application->gbuffers_fbo->depth_texture->toViewport(shader);
}

void Renderer::showSSAO()
{
	Application* application = Application::instance;
	glViewport(0, 0, application->window_width, application->window_height);
	application->ssao_blur->toViewport();
}

//FORWARD
//render all the scene to viewport
void Renderer::renderSceneForward(GTR::Scene* scene, Camera* camera)
{
	Application* application = Application::instance;
	application->rendering_shadowmap = false;

	//be sure no errors present in opengl before start
	checkGLErrors();

	//set the clear color 
	Vector4 bg_color = scene->bg_color;
	glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w);

	setDefaultGLFlags();

	//render skybox
	if (!Scene::instance->forward_for_blends) renderSkybox(camera, scene->environment);

	//set the camera as default (used by some functions in the framework)
	camera->enable();

	if (application->render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	renderScene(scene, camera);

	if (application->render_grid)
		drawGrid();
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

	if (Scene::instance->forward_for_blends)
	{
		glEnable(GL_BLEND);
		//if (is_first_pass)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		//else
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glDepthFunc(GL_LEQUAL);
	}
}

std::vector<Light*> Renderer::renderSceneShadowmaps(GTR::Scene* scene)
{
	Application::instance->rendering_shadowmap = true;

	std::vector<Light*> shadow_casting_lights;

	setDefaultGLFlags();
	if (scene->instance->reverse_shadowmap)
		glFrontFace(GL_CW); //instead of GL_CCW

	//find lights that cast shadow (forced to spot atm)
	for (Light* light : GTR::Scene::instance->lights) {
		if (!light->cast_shadows) continue;
		if (!light->update_shadowmap) {
			shadow_casting_lights.push_back(light);
			continue;
		}

		light->shadow_fbo->bind();
		glColorMask(false, false, false, false);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//be sure no errors present in opengl before start
		checkGLErrors();

		Camera* light_cam = light->camera;
		light_cam->enable();

		if (light->light_type == GTR::POINT)
			renderPointShadowmap(light);
		else
			renderScene(scene, light_cam);
		
		light->shadow_fbo->unbind();
		glColorMask(true, true, true, true);

		if (light->show_shadowmap)
			shadow_casting_lights.push_back(light);
	}

	glDisable(GL_CULL_FACE);
	Application::instance->rendering_shadowmap = false;

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

//render all the scene
void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	for (auto prefabEnt : scene->prefabs)
	{
		if (prefabEnt->visible) {

			//If we are far enough, we can save time by lowering the number of trinagles of the mesh
			Vector3 aux = (prefabEnt->model.getTranslation() - Application::instance->camera->eye);
			float dist = aux.length();

			if (dist > 1000 && prefabEnt->lowres_prefab)
				renderPrefab(prefabEnt->model, prefabEnt->lowres_prefab, camera);
			else
				renderPrefab(prefabEnt->model, prefabEnt->prefab, camera);
		}
	}

	if (Application::instance->rendering_shadowmap) return;

	for (auto light : scene->lights)
	{
		//if (light->visible && light->light_type != DIRECTIONAL)
			//renderSimple(light->light_node->model, light->light_node->mesh, light->light_node->material, camera);
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
    assert(glGetError() == GL_NO_ERROR);


	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	std::vector<Light*> scene_lights = Scene::instance->lights;

	if (Application::instance->rendering_shadowmap)
		renderSimple(model, mesh, material, camera);
	else if (Application::instance->current_pipeline == Application::DEFERRED && !Scene::instance->forward_for_blends)
		renderToGBuffers(model, mesh, material, camera);
	else {
		if (scene_lights.empty())
			renderWithoutLights(model, mesh, material, camera);
		else
			renderMultiPass(model, mesh, material, camera);
	}

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); //as default
	glEnable(GL_DEPTH_TEST);
}

void Renderer::renderWithoutLights(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{

	manageBlendingAndCulling(material, false);

	Shader* shader = Shader::Get("noLights");
	
	//no shader? then nothing to render
	enableShader(shader);

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);

	if (Application::instance->use_gamma_correction)
		shader->setUniform("u_ambient_light", gamma(GTR::Scene::instance->ambient_light) * GTR::Scene::instance->ambient_power);
	else
		shader->setUniform("u_ambient_light", GTR::Scene::instance->ambient_light * GTR::Scene::instance->ambient_power);

	material->setUniforms(shader, true);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();
}

void Renderer::renderMultiPass(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	if (material->alpha_mode != BLEND && Scene::instance->forward_for_blends) return;

	//define locals to simplify coding
	Shader* shader = NULL;
	BaseEntity* entity = NULL;

	std::vector<Light*> scene_lights = Scene::instance->lights;

	bool is_first_pass = true;
	for (auto light : scene_lights)		// MULTI PASS
	{
		// skip iteration if light is far from mesh && light is point light && is not first pass (bc first pass must be done to paint the mesh with ambient light))
		if (!mesh->testSphereCollision(model, light->model.getTranslation(), light->max_distance, Vector3(0, 0, 0), Vector3(0, 0, 0)) && (light->light_type == GTR::POINT || light->light_type == GTR::SPOT) && !is_first_pass)
			continue;

		manageBlendingAndCulling(material, true, is_first_pass);

		Shader* shader = chooseShader(light);

		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_pos", camera->eye);
		shader->setUniform("u_model", model);

		if (Application::instance->use_gamma_correction)
			shader->setUniform("u_ambient_light", gamma(Scene::instance->ambient_light) * GTR::Scene::instance->ambient_power * is_first_pass);
		else
			shader->setUniform("u_ambient_light", Scene::instance->ambient_light * GTR::Scene::instance->ambient_power * is_first_pass);

		if (Scene::instance->forward_for_blends)
		{

			glDisable(GL_DEPTH_TEST);

			material->setUniforms(shader, true);

			FBO* gbuffers = Application::instance->gbuffers_fbo;
			shader->setTexture("u_depth_texture", gbuffers->depth_texture, 9);

			Matrix44 inv_vp = camera->viewprojection_matrix;
			inv_vp.inverse();

			//pass the inverse projection of the camera to reconstruct world pos.
			shader->setUniform("u_inverse_viewprojection", inv_vp);
			//pass the inverse window resolution, this may be useful
			shader->setUniform("u_iRes", Vector2(1.0 / (float)Application::instance->window_width, 1.0 / (float)Application::instance->window_height));
		}
		else
			material->setUniforms(shader, is_first_pass);

		mesh->render(GL_TRIANGLES);

		shader->disable();
		is_first_pass = false;
	}
}

Shader* Renderer::chooseShader(Light* light)
{
	Shader* shader;
	bool use_deferred = Application::instance->current_pipeline == Application::DEFERRED;
	Scene* scene = Scene::instance;

	if (light->cast_shadows)
	{
		if (scene->AA_shadows)
		{
			if (use_deferred) // the second condition take in consideration blend materials
			{
				if (scene->forward_for_blends)
					shader = Shader::Get("deferredBlendAAShadows");
				else if (scene->use_geometry_on_deferred)
				{
					if (scene->show_deferred_light_geometry)
						shader = Shader::Get("flat");
					else
						shader = Shader::Get("deferredLightAAShadowsGeometry");
				}
				else
					shader = Shader::Get("deferredLightAAShadows");
			}
			else
				shader = Shader::Get("lightAAShadows");
			enableShader(shader);
		}
		else //the shader without AA is so much simple
		{
			if (use_deferred)
			{
				if (scene->forward_for_blends)
					shader = Shader::Get("deferredBlendShadows");
				else if (scene->use_geometry_on_deferred)
				{
					if (scene->show_deferred_light_geometry)
						shader = Shader::Get("flat");
					else
						shader = Shader::Get("deferredLightShadowsGeometry");
				}
				else
					shader = Shader::Get("deferredLightShadows");
			}
			else
				shader = Shader::Get("lightShadows");
			enableShader(shader);
		}

		light->setLightUniforms(shader);
		light->setShadowUniforms(shader);
	}
	else
	{
		if (use_deferred)
		{
			if (scene->forward_for_blends)
				shader = Shader::Get("deferredBlend");
			else if (scene->use_geometry_on_deferred)
			{
				if (scene->show_deferred_light_geometry)
					shader = Shader::Get("flat");
				else
					shader = Shader::Get("deferredLightGeometry");
			}
			else
				shader = Shader::Get("deferredLight");
		}
		else
			shader = Shader::Get("light");
		//no shader? then nothing to render
		enableShader(shader);
		light->setLightUniforms(shader);
	}

	shader->setUniform("illumination_technique", Application::instance->current_illumination);
	return shader;
}

void Renderer::enableShader(Shader* shader)
{
	if (!shader)
		return;
	shader->enable();
}

void Renderer::renderSimple(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	Shader* shader = Shader::Get("flat");
	assert(glGetError() == GL_NO_ERROR);

	if (material->alpha_mode == GTR::BLEND && Application::instance->rendering_shadowmap)
		return;

	shader->enable();

	shader->setUniform("u_camera_pos", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

	shader->setUniform("u_color", material->color);

	mesh->render(GL_TRIANGLES);

	shader->disable();
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
	if (!Scene::instance->forward_for_blends)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the color and the depth buffer

	//set default flags
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW); //instead of GL_CCW
}

void Renderer::computeIrradianceCoefficients(sProbe &probe, Scene* scene)
{
	FloatImage images[6]; //here we will store the six views

	Camera cam;
	//set the fov to 90 and the aspect to 1
	cam.setPerspective(90, 1, 0.1, 1000);

	FBO* irr_fbo = Application::instance->irr_fbo;

	for (int i = 0; i < 6; ++i) //for every cubemap face
	{
		//compute camera orientation using defined vectors
		Vector3 eye = probe.pos;
		Vector3 front = cubemapFaceNormals[i][2];
		Vector3 center = probe.pos + front;
		Vector3 up = cubemapFaceNormals[i][1];
		cam.lookAt(eye, center, up);
		cam.enable();

		//render the scene from this point of view

		Application::instance->current_pipeline = Application::FORWARD;
		std::vector<GTR::Light*> shadow_caster_lights = renderSceneShadowmaps(scene);
		irr_fbo->bind();
		renderSceneForward(scene, &cam);
		Application::instance->current_pipeline = Application::DEFERRED;

		irr_fbo->unbind();

		//read the pixels back and store in a FloatImage
		images[i].fromTexture(irr_fbo->color_textures[0]);
	}
	
	Application::instance->camera->enable();

	//compute the coefficients given the six images
	probe.sh = computeSH(images);

}

void Renderer::renderIrradianceProbe(Vector3 pos, float size, float* coeffs)
{
	Camera* camera = Application::instance->camera;
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");
	Shader* shader = Shader::Get("probe");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	Application* application = Application::instance;
	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setTexture("u_depth_texture", application->gbuffers_fbo->depth_texture, 0);
	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	//we need the pixel size so we can center the samples 
	shader->setUniform("u_iRes", Vector2(1.0 / (float)application->gbuffers_fbo->depth_texture->width, 1.0 / (float)application->gbuffers_fbo->depth_texture->height));

	shader->setUniform3Array("u_coeffs", coeffs, 9);

	mesh->render(GL_TRIANGLES);
}

void Renderer::renderReflectionProbe(Vector3 pos, float size, Texture* cubemap)
{
	Camera* camera = Application::instance->camera;
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");
	Shader* shader = Shader::Get("reflection");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	Application* application = Application::instance;
	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);

	shader->setTexture("u_texture", cubemap, 11);

	mesh->render(GL_TRIANGLES);
}

void Renderer::renderSkybox(Camera* camera, Texture* environment)
{
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");
	Shader* shader = Shader::Get("skybox");
	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10,10,10);

	//flags
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	shader->enable();

	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", m);

	shader->setUniform("u_texture", environment, 0);

	mesh->render(GL_TRIANGLES);

	shader->disable();

	//restore default flags
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW); //instead of GL_CCW
}

Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT);
	for (int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
}