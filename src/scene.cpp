#include "scene.h"
#include "mesh.h"
#include "application.h"
#include "renderer.h"

using namespace GTR;

Scene* Scene::instance = nullptr;


Scene::Scene()
{
	//para que no creemos dos instancias ponemos que si no es NULL salta un error
	assert(instance == NULL);

	//guardamos la instancia
	instance = this;

	ambient_light = Vector3(1.0, 1.0, 1.0);
	bg_color = Vector4(0.7, 0.8, 1.0, 1.0);
	ambient_power = 0.6;

	//flags
	reverse_shadowmap = true;
	AA_shadows = false;
	show_gbuffers = false;
	use_geometry_on_deferred = true;
	show_deferred_light_geometry = false;
	forward_for_blends = false;
	show_ssao = false;
	show_probes = false;
}

void Scene::defineGrid(Vector3 offset)
{
	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	Vector3 start_pos(-55, 10, -240);
	Vector3 end_pos(240, 230, 80);

	start_pos += offset;
	end_pos += offset;

	//define how many probes you want per dimension
	Vector3 dim(4, 6, 6);

	//compute the vector from one corner to the other
	Vector3 delta = (end_pos - start_pos);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta.x /= (dim.x - 1);
	delta.y /= (dim.y - 1);
	delta.z /= (dim.z - 1);

	//now delta give us the distance between probes in every axis
	//lets compute the centers
	//pay attention at the order at which we add them
	for (int z = 0; z < dim.z; ++z)
		for (int y = 0; y < dim.y; ++y)
			for (int x = 0; x < dim.x; ++x)
			{
				sProbe p;
				p.local.set(x, y, z);

				//index in the linear array
				p.index = x + y * dim.x + z * dim.x * dim.y;

				//and its position
				p.pos = start_pos + delta * Vector3(x, y, z);
				probes.push_back(p);
			}

	int num = dim.x * dim.y * dim.z;
	//now compute the coeffs for every probe
	for (int iP = 0; iP < num; ++iP)
	{
		int probe_index = iP;
		Application::instance->renderer->computeIrradianceCoefficients(probes[probe_index], Scene::instance);
	}

}

void Scene::computeAllIrradianceCoefficients()
{
	for (int iP = 0; iP < probes.size(); ++iP)
	{
		int probe_index = iP;
		Application::instance->renderer->computeIrradianceCoefficients(probes[probe_index], Scene::instance);
	}
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
	ImGui::Checkbox("Show probes", &show_probes);
	if (ImGui::Button("Re-compute irradiance")) {
		computeAllIrradianceCoefficients();
	}

	if (Application::instance->current_pipeline == Application::DEFERRED)
	{
		ImGui::Checkbox("Show GBuffers", &show_gbuffers);
		ImGui::Checkbox("Show SSAO", &show_ssao);
		ImGui::Checkbox("Use Geometry", &use_geometry_on_deferred);
		if (use_geometry_on_deferred)
			ImGui::Checkbox("Show Light Geometry", &show_deferred_light_geometry);
	}

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
			bool is_selected = ImGui::Selectable(prefabs[i]->name, selected == i);
			if (is_selected) {
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