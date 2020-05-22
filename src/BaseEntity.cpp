
#include "BaseEntity.h"
#include "input.h"
#include "scene.h"
#include "application.h"
#include "utils.h"

#include <cassert>

using namespace GTR;

BaseEntity::BaseEntity(Vector3 position, Vector3 rotation, const char* name, eType type, bool visible)
{
	model = Matrix44::IDENTITY;
	Vector3 eulerAnglesRad = rotation * (PI / 180.0);

	model.rotate(eulerAnglesRad.x, model.frontVector());
	model.rotate(eulerAnglesRad.y, model.topVector());
	model.rotate(eulerAnglesRad.z, model.rightVector());
	model.translateGlobal(position.x, position.y, position.z);
	
	this->name = name;
	this->type = type;
	this->visible = visible;
}

PrefabEntity::PrefabEntity(Prefab* prefab, Vector3 position, Vector3 rotation, const char* name, Prefab* lowres_prefab, bool visible) : BaseEntity(position, rotation, name, PREFAB, visible)
{
	this->prefab = prefab;
	this->lowres_prefab = lowres_prefab;
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

Light::Light(Color color, Vector3 position, Vector3 frontVector, const char* name, eLightType light_type, float intensity, bool visible) : BaseEntity(position, Vector3(0,0,1), name, LIGHT, visible)
{
	if (frontVector.length() == 0) frontVector = Vector3(0.1,-0.9,0.0);
	model.setFrontAndOrthonormalize(frontVector);

	this->color = color.toVector3();
	this->intensity = intensity;
	this->light_type = light_type;

	spot_cutoff_in_deg = 45;
	spot_exponent = 10;

	//flags
	cast_shadows = true;
	show_shadowmap = false;
	update_shadowmap = true;

	setVisiblePrefab();

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
	light_node = new GTR::Node();

	switch (light_type)
	{
	case GTR::DIRECTIONAL:
		light_node = NULL;
		return;
	case GTR::POINT:
		light_node->mesh = Mesh::Get("data/meshes/lowPoli_sphere.obj");
		break;
	case GTR::SPOT:
		light_node->mesh = Mesh::Get("data/meshes/cone.obj");
		break;
	}

	light_node->material = new GTR::Material();
	light_node->model = model;
	light_node->model.scale(3, 3, 3);
	light_node->material->color = Vector4(color * (intensity/2),1.0);
}

void Light::setLightUniforms(Shader* shader)
{
	shader->setUniform("u_light_color", gamma(color));
	shader->setUniform("u_light_direction", model.frontVector());
	shader->setUniform("u_light_type", light_type);
	shader->setUniform("u_light_position", model.getTranslation());
	shader->setUniform("u_light_maxdist", max_distance);
	shader->setUniform("u_light_intensity", intensity);
	shader->setUniform("u_light_spotCosineCutoff", cosf(spot_cutoff_in_deg * DEG2RAD));
	shader->setUniform("u_light_spotExponent", spot_exponent);
}

void Light::setShadowUniforms(Shader* shader)
{
	if (!shadow_fbo) return;

	shader->setTexture("u_shadowmap_AA", shadow_fbo->depth_texture, 7);
	shader->setUniform("u_shadowmap", shadow_fbo->depth_texture, 8);
	shader->setUniform("u_shadow_viewproj", camera->viewprojection_matrix);
	shader->setUniform("u_shadow_bias", (float)shadow_bias);
	if (light_type == GTR::POINT)
		shader->setUniform("u_shadowmap_width", (float)shadow_fbo->depth_texture->width / 6.0f);
	else
		shader->setUniform("u_shadowmap_width", (float)shadow_fbo->depth_texture->width);
	shader->setUniform("u_shadowmap_height", (float)shadow_fbo->depth_texture->height);

	if (light_type == GTR::POINT) {
		shader->setMatrix44Array("u_shadowmap_viewprojs", &shadow_viewprojs[0], 6);
	}
}

void Light::renderInMenu()
{
	bool changed = false;
	ImGui::ColorEdit3("Light Color", (float*)&color);
	if (ImGui::Combo("Light Type", (int*)&light_type, "DIRECTIONAL\0POINT\0SPOT", 3))
	{
		updateLightCamera(true);
		setVisiblePrefab();
	}
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	Vector3 frontVector = model.frontVector();
	ImGuizmo::DecomposeMatrixToComponents(model.m, matrixTranslation, matrixRotation, matrixScale);

	ImGui::Text("Basic Properties:");
	changed |= ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 25);
	changed |= ImGui::DragFloat3("Position", matrixTranslation, 0.1f);
	switch (light_type)
	{
	case GTR::DIRECTIONAL:
		changed |= ImGui::SliderFloat3("Direction", (float*)&frontVector[0], -1, 1);
		break;
	case GTR::SPOT:
		changed |= ImGui::SliderFloat3("Direction", (float*)&frontVector[0], -1, 1);
		changed |= ImGui::SliderFloat("Cutoff Angle", (float*)&spot_cutoff_in_deg, 5, 90);
		changed |= ImGui::SliderFloat("Spot Exponent", (float*)&spot_exponent, 0, 100);
		changed |= ImGui::SliderFloat("FOV", (float*)&camera->fov, 0.0f, 180.0f);
		break;
	}
	changed |= ImGui::SliderFloat("Max Distance", (float*)&max_distance, 0, 2500);

	ImGui::Text("Shadows:");
	ImGui::Checkbox("Cast Shadows", &cast_shadows);
	if (cast_shadows)
	{
		ImGui::Checkbox("Show Shadowmap", &show_shadowmap);
		ImGui::Checkbox("Update Shadowmap", &update_shadowmap);
		changed |= ImGui::DragFloat("Shadow Bias", (float*)&shadow_bias, 0.001f);

		ImGui::Text("Light Camera:");
		changed |= ImGui::SliderFloat("Camera Size", (float*)&ortho_cam_size, 0.1, 2000);
		changed |= ImGui::SliderFloat("Near", (float*)&camera->near_plane, 0.1f, camera->far_plane);
		changed |= ImGui::SliderFloat("Far", (float*)&camera->far_plane, camera->near_plane, 10000.0f);

		if (frontVector.length() == 0) frontVector = Vector3(0.1, -0.9, 0.0);
		if (light_type != DIRECTIONAL)
			light_node->material->color = Vector4(color * (intensity / 2), 1);
	}

	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, model.m);
	model.setFrontAndOrthonormalize(frontVector);
	if (cast_shadows) {
		bool updateFOV = ImGui::Button("Update FOV");
		if (changed || updateFOV) updateLightCamera(updateFOV);
	}
}

void Light::updateLightCamera(bool type_changed)
{
	switch (light_type) {
	case GTR::DIRECTIONAL:
	{
		camera->setOrthographic(-ortho_cam_size, ortho_cam_size, -ortho_cam_size, ortho_cam_size, camera->near_plane, camera->far_plane);
		Vector3 cam_position = Vector3(model.getTranslation().x, 0, model.getTranslation().z) - model.frontVector() * max_distance;
		camera->lookAt(cam_position, cam_position + model.frontVector(), Vector3(0,1,0));
		break;
	}
	case GTR::SPOT:
		if (type_changed)
			camera->setPerspective(2 * spot_cutoff_in_deg, 1, camera->near_plane, camera->far_plane);
		camera->lookAt(model.getTranslation(), model.getTranslation() + model.frontVector(), Vector3(0, 1, 0));
		break;
	case GTR::POINT:
		camera->lookAt(model.getTranslation(), model.getTranslation() + Vector3(0, 0, 1), Vector3(0, 1, 0));
		camera->setPerspective(90.0f, 1.0f, 0.1f, max_distance);
		break;
	}
}

