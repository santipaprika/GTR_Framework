
#include "entity.h"
#include "application.h"
#include "input.h"

#include <cassert>

using namespace GTR;

BaseEntity::BaseEntity(Vector3 position, Vector3 rotation, eType type, bool visible)
{
	model = Matrix44::IDENTITY;
	Vector3 eulerAnglesRad = rotation * (PI / 180.0);

	model.rotate(eulerAnglesRad.x, model.frontVector());
	model.rotate(eulerAnglesRad.y, model.topVector());
	model.rotate(eulerAnglesRad.z, model.rightVector());
	model.translateGlobal(position.x, position.y, position.z);
	
	this->type = type;
	this->visible = visible;
}

PrefabEntity::PrefabEntity(Prefab* prefab, Vector3 position, Vector3 rotation, bool visible) : BaseEntity(position, rotation, PREFAB, visible)
{
	this->prefab = prefab;
}

void PrefabEntity::renderInMenu()
{
	if (this->prefab && ImGui::TreeNode(this, this->prefab->name.c_str())) {
		Vector3 frontVector = model.frontVector();
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(model.m, matrixTranslation, matrixRotation, matrixScale);
		ImGui::DragFloat3("Position", matrixTranslation, 0.1f);
		ImGui::DragFloat3("Rotation", matrixRotation, 0.1f);
		ImGui::DragFloat3("Scale", matrixScale, 0.1f);
		ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, model.m);
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}
}

bool PrefabEntity::clickOnIt(Camera* camera, Node* node)
{

	if (!node) node = &prefab->root;

	if (node->mesh)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			Vector3 cam_dir = camera->getRayDirection(Input::mouse_position.x, Input::mouse_position.y, Application::instance->window_width, Application::instance->window_height);
			Vector3 colision;
			if (RayBoundingBoxCollision(world_bounding, camera->eye, cam_dir, colision))
				return true;
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		if (clickOnIt(camera, node->children[i])) return true;
	
	return false;
}

Light::Light(Color color, Vector3 position, Vector3 frontVector, eLightType light_type, float intensity, bool visible) : BaseEntity(position, Vector3(0,0,1), LIGHT, visible)
{
	if (frontVector.length() == 0) frontVector = Vector3(0.1,-0.9,0.0);
	model.setFrontAndOrthonormalize(frontVector);

	this->color = color.toVector3();
	this->intensity = intensity;
	this->light_type = light_type;

	setVisiblePrefab();

	lightSpherePrefab->prefab->root.material->color = color.toVector4();

	camera = new Camera();
	if (cast_shadows) {
		shadow_fbo = new FBO();

		if (light_type == POINT)
			shadow_fbo->create(6 * 1024, 1024);
		else
			shadow_fbo->create(4096, 4096);

		initializeLightCamera();
	}
}

void Light::initializeLightCamera() {
	
	Vector3 cam_position = model.getTranslation();
	switch (light_type)
	{
	case GTR::SPOT:
		camera->lookAt(model.getTranslation(), cam_position + model.frontVector(), model.topVector());
		camera->setPerspective((float)(2 * spot_cutoff_in_deg), 1.0f, 0.1f, 5000.f);
		break;
	case GTR::DIRECTIONAL:
		max_distance = 500;
		cam_position = model.getTranslation() * Vector3(1, 0, 1) - model.frontVector() * max_distance;
		camera->lookAt(cam_position, cam_position + model.frontVector(), model.topVector());
		camera->setOrthographic(-ortho_cam_size, ortho_cam_size, -ortho_cam_size, ortho_cam_size, 0.1f, 5000.f);
		break;
	case GTR::POINT:
		camera->lookAt(model.getTranslation(), cam_position + Vector3(0,0,1), Vector3(0,1,0));
		camera->setPerspective(90.0f, 1.0f, 0.1f, max_distance);
		break;
	}
}

void Light::setCutoffAngle(float angle_in_deg)
{
	spot_cutoff_in_deg = angle_in_deg;
	camera->setPerspective(2 * spot_cutoff_in_deg, 1, camera->near_plane, camera->far_plane);
}

void Light::setVisiblePrefab()
{
	//Create sphere
	GTR::Node sphere = GTR::Node();
	sphere.mesh = Mesh::Get("data/meshes/sphere.obj");
	sphere.material = new GTR::Material();
	sphere.material->color_texture = Texture::getWhiteTexture();
	sphere.model.scale(5, 5, 5);

	GTR::Prefab* sphere_prefab = new GTR::Prefab();
	sphere_prefab->name = "Sphere (light)";
	sphere_prefab->root = sphere;

	Vector3 light_pos = model.getTranslation();
	lightSpherePrefab = new PrefabEntity(sphere_prefab);
	lightSpherePrefab->model.setTranslation(light_pos.x, light_pos.y, light_pos.z);

	Scene::instance->AddEntity(lightSpherePrefab);
}

void Light::setUniforms(Shader* shader)
{
	shader->setUniform("u_light_color", color);
	shader->setUniform("u_light_direction", model.frontVector());
	shader->setUniform("u_light_type", light_type);
	shader->setUniform("u_light_position", model.getTranslation());
	shader->setUniform("u_light_maxdist", max_distance);
	shader->setUniform("u_light_intensity", intensity);
	shader->setUniform("u_light_spotCosineCutoff", cosf(spot_cutoff_in_deg * DEG2RAD));
	shader->setUniform("u_light_spotExponent", spot_exponent);

	if (!shadow_fbo) return;

	if (light_type == GTR::POINT) {
		shader->setUniform("u_shadowmap_res", shadow_fbo->width);
		shader->setMatrix44Array("u_shadowmap_viewprojs", &shadow_viewprojs[0], 6);
	}

	//get the depth texture from the FBO
	Texture* shadowmap = shadow_fbo->depth_texture;
	shader->setUniform("u_shadowmap", shadowmap, 0);

	//also get the viewprojection from the light
	Matrix44 shadow_proj = camera->viewprojection_matrix;

	//pass it to the shader
	shader->setUniform("u_shadow_viewproj", shadow_proj);

	//we will also need the shadow bias
	shader->setUniform("u_shadow_bias", shadow_bias);

	shader->setUniform("u_cast_shadows", cast_shadows);
}

void Light::renderInMenu()
{
	bool light_type_changed = false;
	ImGui::ColorEdit3("Light Color", (float*)&color);
	if (ImGui::Combo("Light Type", (int*)&light_type, "DIRECTIONAL\0POINT\0SPOT", 3))
		updateLightCamera(true);
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	Vector3 frontVector = model.frontVector();
	ImGuizmo::DecomposeMatrixToComponents(model.m, matrixTranslation, matrixRotation, matrixScale);
	ImGui::DragFloat3("Position", matrixTranslation, 0.1f);
	ImGui::Text("Light Camera:");
	ImGui::SliderFloat("Near", (float*)&camera->near_plane, 0.1f, camera->far_plane);
	ImGui::SliderFloat("Far", (float*)&camera->far_plane, camera->near_plane, 10000.0f);
	ImGui::DragFloat("Shadow Bias", (float*)&shadow_bias, 0.001f);
	ImGui::Checkbox("Cast Shadows", &cast_shadows);
	ImGui::SliderFloat("Max Distance", (float*)&max_distance, 0, 2500);
	ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 100);

	switch (light_type)
	{
	case GTR::DIRECTIONAL:
		ImGui::SliderFloat("Camera Size", (float*)&ortho_cam_size, 0.1, 2000);
		ImGui::SliderFloat3("Direction", (float*)&frontVector[0], -1, 1);
		break;
	case GTR::SPOT:
		ImGui::SliderFloat3("Direction", (float*)&frontVector[0], -1, 1);
		ImGui::SliderFloat("Cutoff Angle", (float*)&spot_cutoff_in_deg, 0, 90);
		ImGui::SliderFloat("Spot Exponent", (float*)&spot_exponent, 0, 100);
		ImGui::SliderFloat("FOV", (float*)&camera->fov, 0.0f, 180.0f);
		break;
	}

	if (frontVector.length() == 0) frontVector = Vector3(0.1, -0.9, 0.0);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, model.m);
	model.setFrontAndOrthonormalize(frontVector);
	lightSpherePrefab->prefab->root.material->color = Vector4(color, 1);
	updateLightCamera();

}

void Light::updateLightCamera(bool type_changed)
{
	switch (light_type) {
	case GTR::DIRECTIONAL:
	{
		camera->setOrthographic(-ortho_cam_size, ortho_cam_size, -ortho_cam_size, ortho_cam_size, camera->near_plane, camera->far_plane);
		Vector3 cam_position = Vector3(model.getTranslation().x, 0, model.getTranslation().z) - model.frontVector() * max_distance;
		camera->lookAt(cam_position, cam_position + model.frontVector(), model.topVector());
		break;
	}
	case GTR::SPOT:
		if (ImGui::Button("Update FOV") || type_changed)
			camera->setPerspective(2 * spot_cutoff_in_deg, 1, camera->near_plane, camera->far_plane);
		camera->lookAt(model.getTranslation(), model.getTranslation() + model.frontVector(), model.topVector());
		break;
	case GTR::POINT:
		camera->lookAt(model.getTranslation(), model.getTranslation() + Vector3(0, 0, 1), Vector3(0, 1, 0));
		camera->setPerspective(90.0f, 1.0f, 0.1f, max_distance);
		camera->far_plane = max_distance;
		break;
	}
}

Scene* Scene::instance = nullptr;
Scene::Scene() 
{
	assert(instance == NULL);
	this->ambientLight = Vector3(0.2, 0.2, 0.35);
	instance = this;
}

void Scene::AddEntity(BaseEntity* entity)
{
	switch (entity->type) 
	{
	case LIGHT:
		lights.push_back((Light*)entity);
		//std::cout << ((Light*)entity)->light_type;

		break;
	case PREFAB:
		prefabs.push_back((PrefabEntity*)entity);
		break;
	}
}

void Scene::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::ColorEdit3("Ambient Light", (float*)&instance->ambientLight);
	if (ImGui::TreeNode("Lights")) {
		int curr_light_idx = 0;
		for (auto light : lights) {
			const char* node_name;
			switch (light->light_type)
			{
			case POINT: node_name = "Point light"; break;
			case SPOT: node_name = "Spot light"; break;
			case DIRECTIONAL: node_name = "Directional light"; break;
			}
			if (ImGui::TreeNode(light, node_name)) {
				light->renderInMenu();
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif
}