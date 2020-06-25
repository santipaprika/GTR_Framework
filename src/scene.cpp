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

	//flags
	reverse_shadowmap = false;
	AA_shadows = false;
	show_gbuffers = false;
	use_geometry_on_deferred = true;
	show_deferred_light_geometry = false;
	forward_for_blends = false;
	show_ssao = false;
	show_probes = false;
	show_rProbes = false;
	use_irradiance = true;
	show_coefficients = false;
	interpolate_probes = true;

	irr_normal_distance = 1.0f;

	probes_filename = "irradiance.bin";
	use_volumetric = true;
}

void Scene::defineIrradianceGrid(Vector3 offset)
{

	if (loadProbesFromDisk())
		return;

	probes.clear();

	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	start_pos_grid.set(-120, -25, -320);
	end_pos_grid.set(350, 250, 160);

	start_pos_grid += offset;
	end_pos_grid += offset;

	//define how many probes you want per dimension
	dim_grid.set(14, 12, 12);

	//compute the vector from one corner to the other
	delta_grid = (end_pos_grid - start_pos_grid);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta_grid.x /= (dim_grid.x - 1);
	delta_grid.y /= (dim_grid.y - 1);
	delta_grid.z /= (dim_grid.z - 1);

	for (int z = 0; z < dim_grid.z; ++z)
		for (int y = 0; y < dim_grid.y; ++y)
			for (int x = 0; x < dim_grid.x; ++x)
			{
				sProbe p;
				p.local.set(x, y, z);

				//index in the linear array
				p.index = x + y * dim_grid.x + z * dim_grid.x * dim_grid.y;

				//and its position
				p.pos = start_pos_grid + delta_grid * Vector3(x, y, z);
				probes.push_back(p);
			}

	computeIrradiance();
}

void Scene::computeIrradiance() 
{
	computeAllIrradianceCoefficients();
	setIrradianceTexture();
	writeProbesToDisk();
}

void Scene::computeAllIrradianceCoefficients()
{
	//now compute the coeffs for every probe
	for (int iP = 0; iP < probes.size(); ++iP)
	{
		int probe_index = iP;
		Application::instance->renderer->computeIrradianceCoefficients(probes[probe_index], Scene::instance);
	}
}

void Scene::setIrradianceTexture()
{
	//create the texture to store the probes (do this ONCE!!!)
	probes_texture = new Texture(
		9, //9 coefficients per probe
		probes.size(), //as many rows as probes
		GL_RGB, //3 channels per coefficient
		GL_FLOAT); //they require a high range

		//we must create the color information for the texture. because every SH are 27 floats in the RGB,RGB,... order, we can create an array of SphericalHarmonics and use it as pixels of the texture
	SphericalHarmonics* sh_data = NULL;
	sh_data = new SphericalHarmonics[dim_grid.x * dim_grid.y * dim_grid.z];

	//here we fill the data of the array with our probes in x,y,z order...
	for (int i = 0; i < probes.size(); i++)
	{
		sh_data[i] = probes[i].sh;
	}

	//now upload the data to the GPU
	probes_texture->upload(GL_RGB, GL_FLOAT, false, (uint8*)sh_data);

	//disable any texture filtering when reading
	probes_texture->bind();
	glClear(GL_COLOR_BUFFER_BIT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//always free memory after allocating it!!!
	delete[] sh_data;

}

void Scene::writeProbesToDisk()
{
	//fill header structure
	sIrrHeader header;

	header.start = start_pos_grid;
	header.end = end_pos_grid;
	header.dims = dim_grid;
	header.delta = delta_grid;
	header.num_probes = dim_grid.x * dim_grid.y *
		dim_grid.z;

	//write to file header and probes data
	FILE* f = fopen(probes_filename.c_str(), "wb");
	fwrite(&header, sizeof(header), 1, f);
	fwrite(&(probes[0]), sizeof(sProbe), probes.size(), f);
	fclose(f);

	std::cout << "* Probes coefficients written in " + probes_filename + "\n";
}

bool Scene::loadProbesFromDisk()
{
	//load probes info from disk
	FILE* f = fopen(probes_filename.c_str(), "rb");
	if (!f)
		return false;

	//read header
	sIrrHeader header;
	fread(&header, sizeof(header), 1, f);

	//copy info from header to our local vars
	start_pos_grid = header.start;
	end_pos_grid = header.end;
	dim_grid = header.dims;
	delta_grid = header.delta;
	int num_probes = header.num_probes;

	//allocate space for the probes
	probes.resize(num_probes);

	//read from disk directly to our probes container in memory
	fread(&probes[0], sizeof(sProbe), probes.size(), f);
	fclose(f);

	//build the texture again…
	setIrradianceTexture();

	return true;
}

void Scene::SetIrradianceUniforms(Shader* shader)
{
	shader->setUniform("u_irr_start", start_pos_grid);
	shader->setUniform("u_irr_end", end_pos_grid);
	shader->setUniform("u_irr_normal_distance", irr_normal_distance);
	shader->setUniform("u_irr_delta", delta_grid);
	shader->setUniform("u_irr_dims", dim_grid);
	shader->setUniform("u_num_probes", (int)probes.size());
	shader->setUniform("u_use_irradiance", use_irradiance);
	shader->setUniform("u_interpolate_probes", interpolate_probes);
	shader->setTexture("u_probes_texture", probes_texture, 9);
}

void Scene::defineReflectionGrid(Vector3 offset)
{
	int N = 4; //number of probes

	for (int i = 0; i < N; i++)
	{
		sReflectionProbe* rProbe = new sReflectionProbe();
		//set it up
		rProbe->pos.set(-250, 56, 0);
		rProbe->pos += offset + Vector3(i*300,0,0);
		rProbe->cubemap = new Texture();
		rProbe->cubemap->createCubemap(
			512, 512,
			NULL,
			GL_RGB, GL_UNSIGNED_INT, false);

		//add it to the list
		reflection_probes.push_back(rProbe);

		rProbe->cubemap->bind();
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	computeReflection();
}

void Scene::computeReflection()
{
	Camera cam;
	for (sReflectionProbe* rProbe : reflection_probes)
	{
		//render the view from every side
		for (int i = 0; i < 6; ++i)
		{
			Application* application = Application::instance;
			//assign cubemap face to FBO
			application->reflections_fbo->setTexture(rProbe->cubemap, i);
			
			//render view
			Vector3 eye = rProbe->pos;
			Vector3 center = rProbe->pos + cubemapFaceNormals[i][2];
			Vector3 up = cubemapFaceNormals[i][1];
			cam.lookAt(eye, center, up); 
			cam.setPerspective(74.f, application->window_width / (float)application->window_height, 1.0f, 10000.f);
			cam.enable();
			application->current_pipeline = Application::FORWARD;
			std::vector<GTR::Light*> shadow_caster_lights = application->renderer->renderSceneShadowmaps(Scene::instance);
			application->reflections_fbo->bind();
			application->renderer->renderSceneForward(Scene::instance, &cam);
			application->current_pipeline = Application::DEFERRED;
			application->reflections_fbo->unbind();
		}

		//generate the mipmaps
		rProbe->cubemap->generateMipmaps();
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
	ImGui::Checkbox("Use Volume Scattering", &use_volumetric);
	ImGui::Checkbox("Reverse Shadowmap", &reverse_shadowmap);
	ImGui::Checkbox("Apply AntiAliasing to Shadows", &AA_shadows);
	ImGui::Checkbox("Use Irradiance", &use_irradiance);
	ImGui::Checkbox("Show probes", &show_probes);
	ImGui::Checkbox("Show coefficients", &show_coefficients);
	ImGui::Checkbox("Interpolate probes", &interpolate_probes);
	if (ImGui::Button("Re-compute irradiance")) {
		computeIrradiance();
	}
	ImGui::DragFloat("Irr normal distance", &irr_normal_distance, .1f);
	ImGui::Checkbox("Show reflection probes", &show_rProbes);

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