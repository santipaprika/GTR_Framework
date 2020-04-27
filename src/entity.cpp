
#include "entity.h"
#include <cassert>

using namespace GTR;

BaseEntity::BaseEntity(Vector3 position, Vector3 eulerAngles, eType type, bool visible)
{
	Matrix44 auxModel = Matrix44::IDENTITY;
	Vector3 eulerAnglesRad = eulerAngles * (PI / 180.0);

	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(auxModel.m, matrixTranslation, matrixRotation, matrixScale);
	matrixTranslation[0] = position.x;	matrixTranslation[1] = position.y;	matrixTranslation[2] = position.z;
	matrixRotation[0] = eulerAngles.x;	matrixRotation[1] = eulerAngles.y;	matrixRotation[2] = eulerAngles.z;
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, auxModel.m);

	/*auxModel.rotate(eulerAnglesRad.x, auxModel.frontVector());
	auxModel.rotate(eulerAnglesRad.y, auxModel.topVector());
	auxModel.rotate(eulerAnglesRad.z, auxModel.rightVector());
	auxModel.translateGlobal(position.x, position.y, position.z);*/
	this->model = auxModel;
	this->type = type;
	this->visible = visible;
}

PrefabEntity::PrefabEntity(Prefab* prefab, Vector3 position, Vector3 eulerAngles, bool visible) : BaseEntity(position, eulerAngles, PREFAB, visible)
{
	this->prefab = prefab;
}

void PrefabEntity::renderInMenu()
{
	if (this->prefab && ImGui::TreeNode(this, this->prefab->name.c_str())) {
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

Light::Light(Color color, Vector3 position, Vector3 eulerAngles, eLightType light_type, float intensity, bool visible) : BaseEntity(position, eulerAngles, LIGHT, visible)
{
	this->color = color.toVector3();
	this->intensity = intensity;
	this->light_type = light_type;
}

void Light::setUniforms(Shader* shader)
{
	shader->setUniform("u_light_color", color);
	shader->setUniform("u_light_direction", model.frontVector());
	shader->setUniform("u_light_type", light_type);
	shader->setUniform("u_light_position", model.getTranslation());
	shader->setUniform("u_light_maxdist", max_distance);
	shader->setUniform("u_light_intensity", intensity);
	shader->setUniform("u_light_spotCosineCutoff", spotCosineCutoff);
	shader->setUniform("u_light_spotExponent", spotExponent);
}

void Light::renderInMenu()
{
	ImGui::ColorEdit3("Light Color", (float*)&color);
	ImGui::Combo("Light Type", (int*)&light_type, "DIRECTIONAL\0POINT\0SPOT", 3);
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(model.m, matrixTranslation, matrixRotation, matrixScale);
	switch (light_type)
	{
	case GTR::DIRECTIONAL:
		ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 100);
		ImGui::DragFloat3("Direction", matrixRotation, 0.1f);
		break;
	case GTR::POINT:
		ImGui::SliderFloat("Max Distance", (float*)&max_distance, 0, 1000);
		ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 100);
		ImGui::DragFloat3("Position", matrixTranslation, 0.1f);
		break;
	case GTR::SPOT:
		ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 100);
		ImGui::DragFloat3("Position", matrixTranslation, 0.1f);
		ImGui::DragFloat3("Direction", matrixRotation, 0.1f);
		ImGui::SliderFloat("Cosine Cutoff", (float*)&spotCosineCutoff, 0, 1);
		ImGui::SliderFloat("Spot Exponent", (float*)&spotExponent, 0, 100);
	}
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, model.m);
	ImGui::TreePop();
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
			const char* node_name = "Light";
			if (ImGui::TreeNode(light, node_name)) {
				light->renderInMenu();
			}
		}
	}
	ImGui::TreePop();
#endif
}