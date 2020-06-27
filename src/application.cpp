#include "application.h"
#include "utils.h"
#include "mesh.h"
#include "texture.h"

#include "fbo.h"
#include "shader.h"
#include "input.h"
#include "includes.h"
#include "prefab.h"
#include "gltf_loader.h"
#include "renderer.h"
#include "scene.h"
#include "sphericalharmonics.h"

#include <cmath>
#include <string>
#include <cstdio>

Application* Application::instance = nullptr;
float cam_speed = 10;

Application::Application(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	instance = this;
	must_exit = false;
	render_debug = true;
	render_gui = true;
	render_grid = false;
	render_wireframe = false;

	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;
	mouse_locked = false;

	prefab_selected = NULL;
	light_selected = NULL;

	current_pipeline = DEFERRED;
	current_illumination = PBR;

	Vector3 offset = Vector3(0, -1000, 0);

	//loads and compiles several shaders from one single file
    //change to "data/shader_atlas_osx.txt" if you are in XCODE
	if(!Shader::LoadAtlas("data/shader_atlas.txt"))
        exit(1);
    checkGLErrors();

	// Create camera
	camera = new Camera();
	camera->lookAt(Vector3(-150.f, 150.0f, 250.f) + offset, Vector3(0.f, 0.1f, 0.f) + offset, Vector3(0.f, 1.f, 0.f));
	camera->setPerspective( 45.f, window_width/(float)window_height, 1.0f, 10000.f);
	
	//This class will be the one in charge of rendering all 
	renderer = new GTR::Renderer(); //here so we have opengl ready in constructor
	renderer->initFlags();

	//initialize GBuffers for deferred (create FBO)
	renderer->gbuffers_fbo = new FBO();

	//create 3 textures of 4 components
	renderer->gbuffers_fbo->create(window_width, window_height,
		3, 			//three textures
		GL_RGBA, 		//four channels
		GL_UNSIGNED_BYTE, //1 byte
		true);		//add depth_texture

	//create and FBO
	renderer->illumination_fbo = new FBO();
	renderer->volumetrics_fbo = new FBO();

	//create 3 textures of 4 components
	renderer->illumination_fbo->create(window_width, window_height, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);
	renderer->volumetrics_fbo->create(window_width/4, window_height/4, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);

	renderer->irr_fbo = new FBO();
	renderer->irr_fbo->create(64, 64, 1, GL_RGB, GL_FLOAT);

	renderer->reflections_fbo = new FBO();
	renderer->reflections_fbo->create(64, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE);

	renderer->reflections_component = new FBO();
	renderer->reflections_component->create(window_width, window_height, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);

	//let's create an FBO to render the AO inside
	renderer->ssao_fbo = new FBO();
	renderer->ssao_fbo->create(window_width, window_height);

	//maybe we want to create also one for the blur, in this case just create a texture
	renderer->ssao_blur = new Texture();
	renderer->ssao_blur->create(renderer->ssao_fbo->width, renderer->ssao_fbo->height);

	//Create Scene
	GTR::Scene* scene = new GTR::Scene();

	GTR::Prefab* scene_prefab = GTR::Prefab::Get("data/prefabs/brutalism/scene.gltf");
	scene_prefab->root.model.rotate(PI/2.0, Vector3(0,1,0));
	scene_prefab->root.model.translateGlobal(0, 0, -200);
	scene_prefab->root.model.scale(100, 100, 100);
	scene->AddEntity(new GTR::PrefabEntity(scene_prefab, offset));

	Mesh* plane_mesh = new Mesh();
	plane_mesh->createPlane(2048.0f);
	GTR::Node plane_node = GTR::Node();
	plane_node.mesh = plane_mesh;

	GTR::Material* plane_mat = new GTR::Material(Texture::Get("data/textures/grass.png"));
	plane_mat->tiles_number = 50;
	plane_node.material = plane_mat;
	GTR::Prefab* floor = new GTR::Prefab();
	floor->root = plane_node;
	floor->name = "Floor_Node";
	scene->AddEntity(new GTR::PrefabEntity(floor, Vector3(0,-27,0) + offset, Vector3(0,0,0), "Floor"));

	GTR::Prefab* car_prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");
	scene->AddEntity(new GTR::PrefabEntity(car_prefab, Vector3(450, -28, 0) + offset, Vector3(0,0,0),"Car")); //last prefab is the car, due to the blend materials

	GTR::Light* sun = new GTR::Light(Color::WHITE, Vector3(0, 0, 0) + offset, Vector3(0.8, -0.45, -0.4), "Sun", GTR::DIRECTIONAL, 5);
	sun->ortho_cam_size = 1000;
	sun->initializeLightCamera();

	//pick the texture used for the skybox
	scene->environment = GTR::CubemapFromHDRE("data/textures/panorama.hdre");

	GTR::Light* spot1 = new GTR::Light(Color::RED, Vector3(-60, 100, 0) + offset, Vector3(0.45, -0.8, -0.35), "Spot1", GTR::SPOT, 20);
	spot1->spot_cutoff_in_deg = 70;
	spot1->initializeLightCamera();

	GTR::Light* spot2 = new GTR::Light(Color::YELLOW, Vector3(0, 125, 0) + offset, Vector3(0.5, -0.15, -0.8), "Spot2", GTR::SPOT, 20);
	spot2->spot_cutoff_in_deg = 40;
	spot2->spot_exponent = 40;
	spot2->initializeLightCamera();
	scene->AddEntity(sun);
	scene->AddEntity(spot1);
	scene->AddEntity(spot2);

	renderer->random_points = GTR::generateSpherePoints(100, renderer->sphere_radius, true);

	if (current_pipeline == DEFERRED) //probes only working on deferred for the moment
	{
		scene->defineIrradianceGrid(offset);
		scene->defineReflectionGrid(offset);
	}

	//hide the cursor
	SDL_ShowCursor(!mouse_locked); //hide or show the mouse
}


//what to do when the image has to be draw
void Application::render(void)
{
	GTR::Scene* scene = GTR::Scene::instance;

	if (current_pipeline == DEFERRED) 
	{
		// STORE RENDER INTO BUFFERS
		renderer->shadow_caster_lights = renderer->renderSceneShadowmaps(scene);
		renderer->renderGBuffers(scene, camera);
		if (renderer->use_ssao) renderer->renderSSAO(camera);
		renderer->renderIlluminationToBuffer(camera);
		renderer->forward_for_blends = true;
		renderer->renderSceneForward(scene, camera);
		renderer->forward_for_blends = false;

		renderer->illumination_fbo->unbind();

		// SHOW BUFFERS TO VIEWPORT
		//set default flags
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		
		renderer->renderToViewport(camera, scene);
	}
	else {
		renderer->shadow_caster_lights = renderer->renderSceneShadowmaps(scene);
		renderer->renderSceneForward(scene, camera);
		renderer->showSceneShadowmaps();
	}
	
	//the swap buffers is done in the main loop after this function
}


//called to render the GUI from
void Application::renderDebugGUI(void)
{
#ifndef SKIP_IMGUI //to block this code from compiling if we want

	GTR::Scene* scene = GTR::Scene::instance;

	//System stats
	ImGui::Text(getGPUStats().c_str());					   // Display some text (you can use a format strings too)

	const char* current_element_name = (current_pipeline >= 0 && current_pipeline < Element_COUNT) ? element_names[current_pipeline] : "Unknown";
	ImGui::SliderInt("Rendering Pipeline", &current_pipeline, 0, Element_COUNT - 1, current_element_name);

	const char* current_illumination_name = (current_illumination >= 0 && current_illumination < Illuminations_COUNT) ? illuminations_names[current_illumination] : "Unknown";
	if (ImGui::SliderInt("Illumination Technique", &current_illumination, 0, Illuminations_COUNT - 1, current_illumination_name)) {
		float compensation_factor;
		if (current_illumination == PBR)
			compensation_factor = 10.0;
		else
			compensation_factor = 1 / 10.0;

		for (GTR::Light* light : GTR::Scene::instance->lights)
			light->intensity *= compensation_factor;
	}
	
	//add info to the debug panel about the camera
	if (ImGui::TreeNode(camera, "Camera")) {
		camera->renderInMenu();
		ImGui::TreePop();
	}
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Renderer")) {
		renderer->renderInMenu(scene);
	}

	ImGui::Separator();

	//add info to the debug panel about the scene
	if (ImGui::CollapsingHeader("Scene"))
		scene->renderInMenu();

	#endif
}

void Application::update(double seconds_elapsed)
{
	float speed = seconds_elapsed * cam_speed * 3; //the speed is defined by the seconds_elapsed so it goes constant
	float orbit_speed = seconds_elapsed * 0.5;
	
	//async input to move the camera around
	if (Input::isKeyPressed(SDL_SCANCODE_LSHIFT)) speed *= 10; //move faster with left shift
	if (Input::isKeyPressed(SDL_SCANCODE_W) || Input::isKeyPressed(SDL_SCANCODE_UP)) camera->move(Vector3(0.0f, 0.0f, 1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_S) || Input::isKeyPressed(SDL_SCANCODE_DOWN)) camera->move(Vector3(0.0f, 0.0f,-1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_A) || Input::isKeyPressed(SDL_SCANCODE_LEFT)) camera->move(Vector3(1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_D) || Input::isKeyPressed(SDL_SCANCODE_RIGHT)) camera->move(Vector3(-1.0f, 0.0f, 0.0f) * speed);

	//mouse input to rotate the cam
	#ifndef SKIP_IMGUI
	if (!ImGuizmo::IsUsing())
	#endif
	{
		if (mouse_locked || Input::mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) //move in first person view
		{
			camera->rotate(-Input::mouse_delta.x * orbit_speed * 0.5, Vector3(0, 1, 0));
			Vector3 right = camera->getLocalVector(Vector3(1, 0, 0));
			camera->rotate(-Input::mouse_delta.y * orbit_speed * 0.5, right);
		}
		else //orbit around center
		{
			bool mouse_blocked = false;
			#ifndef SKIP_IMGUI
						mouse_blocked = ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
			#endif
			if (Input::mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT) && !mouse_blocked) //is left button pressed?
			{
				camera->orbit(-Input::mouse_delta.x * orbit_speed, Input::mouse_delta.y * orbit_speed);
			}
		}
	}
	
	//move up or down the camera using Q and E
	if (Input::isKeyPressed(SDL_SCANCODE_Q)) camera->moveGlobal(Vector3(0.0f, -1.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_E)) camera->moveGlobal(Vector3(0.0f, 1.0f, 0.0f) * speed);

	//to navigate with the mouse fixed in the middle
	SDL_ShowCursor(!mouse_locked);
	#ifndef SKIP_IMGUI
		ImGui::SetMouseCursor(mouse_locked ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow);
	#endif
	if (mouse_locked)
	{
		Input::centerMouse();
		//ImGui::SetCursorPos(ImVec2(Input::mouse_position.x, Input::mouse_position.y));
	}
}

void Application::renderDebugGizmo()
{
	GTR::BaseEntity* prefab = NULL;
	//this configuration will priorize the prefabs than the lights
	if (prefab_selected != NULL)
		prefab = prefab_selected;
	else if (light_selected != NULL) {
		if (light_selected->light_type != GTR::DIRECTIONAL)
			prefab = light_selected;
	}

	if (prefab == NULL) return;
	//example of matrix we want to edit, change this to the matrix of your entity
	Matrix44& matrix = prefab->model;

	#ifndef SKIP_IMGUI
	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(matrix.m, matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	static Vector3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGuizmo::Manipulate(camera->view_matrix.m, camera->projection_matrix.m, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m, NULL, useSnap ? &snap.x : NULL);
	
	if (light_selected != NULL && light_selected->light_type != GTR::DIRECTIONAL) {
		light_selected->light_node->model = light_selected->model;
		light_selected->light_node->model.scale(3,3,3);

		// if want the light to be static (i.e. shadows won't be computed at every frame), don't update the light information. Otherwise, shadowmap will react to current model.
		if (light_selected->update_shadowmap)
			light_selected->updateLightCamera();
	}
	#endif
}



//Keyboard event handler (sync input)
void Application::onKeyDown( SDL_KeyboardEvent event )
{
	switch(event.keysym.sym)
	{
		case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
		case SDLK_F1: render_debug = !render_debug; break;
		case SDLK_F2: render_grid = !render_grid; break;
		case SDLK_f: camera->center.set(0, 0, 0); camera->updateViewMatrix(); break;
		case SDLK_F5: Shader::ReloadAll(); break;
	}
}

void Application::onKeyUp(SDL_KeyboardEvent event)
{
}

void Application::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Application::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

float down_time;
void Application::onMouseButtonDown( SDL_MouseButtonEvent event )
{
	if (event.button == SDL_BUTTON_MIDDLE) //middle mouse
	{
		//Input::centerMouse();
		mouse_locked = !mouse_locked;
		SDL_ShowCursor(!mouse_locked);
	}

	if (event.button == SDL_BUTTON_LEFT)
		down_time = time;
}

void Application::onMouseButtonUp(SDL_MouseButtonEvent event)
{

}

void Application::onMouseWheel(SDL_MouseWheelEvent event)
{
	bool mouse_blocked = false;

	#ifndef SKIP_IMGUI
		ImGuiIO& io = ImGui::GetIO();
		if(!mouse_locked)
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
			{
				if (event.x > 0) io.MouseWheelH += 1;
				if (event.x < 0) io.MouseWheelH -= 1;
				if (event.y > 0) io.MouseWheel += 1;
				if (event.y < 0) io.MouseWheel -= 1;
			}
		}
		mouse_blocked = ImGui::IsAnyWindowHovered();
	#endif

	if (!mouse_blocked && event.y)
	{
		if (mouse_locked)
			cam_speed *= 1 + (event.y * 0.1);
		else
			camera->changeDistance(event.y * 0.5);
	}
}

void Application::onResize(int width, int height)
{
    std::cout << "window resized: " << width << "," << height << std::endl;
	glViewport( 0,0, width, height );
	camera->aspect =  width / (float)height;
	window_width = width;
	window_height = height;

	//Update FBO's
	renderer->gbuffers_fbo->~FBO();
	renderer->gbuffers_fbo->create(window_width, window_height,
		3, 	
		GL_RGBA, 	
		GL_UNSIGNED_BYTE, 
		true);

	renderer->illumination_fbo->~FBO();
	renderer->illumination_fbo->create(window_width, window_height,
		1, 	
		GL_RGB, 	
		GL_UNSIGNED_BYTE,
		false);	

	/*volumetrics_fbo->~FBO();
	volumetrics_fbo->create(window_width, window_height,
		1,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		false);*/

	/*reflections_component->~Texture();
	reflections_component->create(window_width, window_height);*/

}

