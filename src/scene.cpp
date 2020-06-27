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
	ambient_power = 0.2;
}

void Scene::defineIrradianceGrid(Vector3 offset)
{
	Renderer* renderer = Application::instance->renderer;

	if (renderer->loadProbesFromDisk(instance))
		return;

	probes.clear();

	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	renderer->start_pos_grid.set(-120, -25, -320);
	renderer->end_pos_grid.set(350, 250, 160);

	renderer->start_pos_grid += offset;
	renderer->end_pos_grid += offset;

	//define how many probes you want per dimension
	renderer->dim_grid.set(14, 12, 12);

	//compute the vector from one corner to the other
	renderer->delta_grid = (renderer->end_pos_grid - renderer->start_pos_grid);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	renderer->delta_grid.x /= (renderer->dim_grid.x - 1);
	renderer->delta_grid.y /= (renderer->dim_grid.y - 1);
	renderer->delta_grid.z /= (renderer->dim_grid.z - 1);

	for (int z = 0; z < renderer->dim_grid.z; ++z)
		for (int y = 0; y < renderer->dim_grid.y; ++y)
			for (int x = 0; x < renderer->dim_grid.x; ++x)
			{
				sProbe p;
				p.local.set(x, y, z);

				//index in the linear array
				p.index = x + y * renderer->dim_grid.x + z * renderer->dim_grid.x * renderer->dim_grid.y;

				//and its position
				p.pos = renderer->start_pos_grid + renderer->delta_grid * Vector3(x, y, z);
				probes.push_back(p);
			}

	renderer->computeIrradiance(instance);
}

void Scene::defineReflectionGrid(Vector3 offset)
{
	Vector3 start_refl_grid;
	start_refl_grid.set(-20, 70, 0);

	Vector3 end_refl_grid;
	end_refl_grid.set(200, 90, 80);

	start_refl_grid += offset;
	end_refl_grid += offset;

	//define how many probes you want per dimension
	Vector3 dim_refl_grid;
	dim_refl_grid.set(2, 1, 1);

	//compute the vector from one corner to the other
	Vector3 delta_refl_grid;
	delta_refl_grid = (end_refl_grid - start_refl_grid);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta_refl_grid.x /= (dim_refl_grid.x - 1);
	//delta_refl_grid.y /= (dim_refl_grid.y - 1);
	//delta_refl_grid.z /= (dim_refl_grid.z - 1);

	for (int z = 0; z < dim_refl_grid.z; ++z)
		for (int y = 0; y < dim_refl_grid.y; ++y)
			for (int x = 0; x < dim_refl_grid.x; ++x)
			{
				sReflectionProbe* rProbe = new sReflectionProbe();

				//and its position
				rProbe->pos = start_refl_grid + delta_refl_grid * Vector3(x, y, z);
				rProbe->cubemap = new Texture();
				rProbe->cubemap->createCubemap(
					512, 512,
					NULL,
					GL_RGB, GL_UNSIGNED_INT, false);

				reflection_probes.push_back(rProbe);

				rProbe->cubemap->bind();
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

	placeReflectionProbe(Vector3(0, 270, 0), offset);	// top house
	placeReflectionProbe(Vector3(-20, 40, -180), offset);	// inside house
	placeReflectionProbe(Vector3(0, 270, -200), offset);	// top behind house
	placeReflectionProbe(Vector3(-160, 70, -100), offset);	// left house
	placeReflectionProbe(Vector3(380, 110, 0), offset);	// right house
	placeReflectionProbe(Vector3(100, 70, 120), offset);	// front house
	placeReflectionProbe(Vector3(-20, 40, -340), offset); //behind left house
	placeReflectionProbe(Vector3(200, 40, -250), offset); //behind right house

	Application::instance->renderer->computeReflection(instance);
}

void Scene::placeReflectionProbe(Vector3 pos, Vector3 offset)
{
	sReflectionProbe* rProbe = new sReflectionProbe();

	//top probe
	rProbe->pos = pos + offset;
	rProbe->cubemap = new Texture();
	rProbe->cubemap->createCubemap(
		512, 512,
		NULL,
		GL_RGB, GL_UNSIGNED_INT, false);

	reflection_probes.push_back(rProbe);

	rProbe->cubemap->bind();
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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