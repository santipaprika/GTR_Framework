#include "scene.h"
#include "mesh.h"
#include "application.h"

using namespace GTR;

Scene* Scene::instance = nullptr;


Scene::Scene()
{
	//para que no creemos dos instancias ponemos que si no es NULL salta un error
	assert(instance == NULL);

	//guardamos la instancia
	instance = this;

	ambient_light = Vector3(0.2, 0.2, 0.35);
	bg_color = Vector4(0.5, 0.5, 0.5, 1.0);
	ambient_power = 0.2;

	//flags
	reverse_shadowmap = false;
	AA_shadows = true;
}

void Scene::AddEntity(BaseEntity* entity)
{
	switch (entity->type)
	{
	case LIGHT:
		lights.push_back((Light*)entity);
		break;
	case PREFAB:
		prefabs.push_back((PrefabEntity*)entity);
		break;
	}
}

void Scene::renderInMenu()
{
	ImGui::ColorEdit4("BG color", bg_color.v);
	ImGui::Text("Ambient Light", IMGUI_VERSION);
	ImGui::SliderFloat("Power", &ambient_power, 0.0, 1.0);
	ImGui::ColorEdit3("Color", &ambient_light.x);
	ImGui::Text("Flags:");
	ImGui::Checkbox("Reverse Shadowmap", &reverse_shadowmap);
	ImGui::Checkbox("Apply AntiAliasing to Shadows", &AA_shadows);

	// info to the debug panel about the lights
	bool node_open = ImGui::TreeNode("Lights");
	if (node_open)
	{
		static int selected = -1;
		Light* light_selected = Application::instance->light_selected;
		for (int i = 0; i < lights.size(); i++)
		{
			if (ImGui::Selectable(lights[i]->name, selected == i)) {
				if (Application::instance->light_selected == lights[i])
					Application::instance->light_selected = nullptr;
				else
				{
					Application::instance->light_selected = lights[i];
					selected = i;
				}
			}
			if ((i + 1) == instance->lights.size())
				if (Application::instance->light_selected)
				{
					ImGui::Separator();
					Application::instance->light_selected->renderInMenu();
					ImGui::Separator();
				}
		}
		ImGui::TreePop();
	}

	// info to the debug panel about the prefabs
	node_open = ImGui::TreeNode("Prefabs");
	if (node_open)
	{
		int selected = -1;
		PrefabEntity* prefab_selected = Application::instance->prefab_selected;
		for (int i = 0; i < prefabs.size(); i++)
		{
			bool hola = ImGui::Selectable(prefabs[i]->name, selected == i);
			if (hola) {
				if (Application::instance->prefab_selected == prefabs[i])
					Application::instance->prefab_selected = nullptr;
				else
				{
					Application::instance->prefab_selected = prefabs[i];
					selected = i;
				}
			}
			if ((i + 1) == instance->prefabs.size())
				if (Application::instance->prefab_selected)
				{
					ImGui::Separator();
					Application::instance->prefab_selected->renderInMenu();
					ImGui::Separator();
				}
		}
		ImGui::TreePop();
	}
}