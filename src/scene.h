#pragma once

#include "framework.h"
#include "BaseEntity.h"
#include <cassert>
#include <map>
#include <string>


namespace GTR {

	struct sProbe;

	//this class contains all info relevant of how to create and define the scene
	class Scene {
	public:

		//instance delcaration
		static Scene* instance;

		//list to store al entities of the scene
		std::vector<Light*> lights;
		std::vector<PrefabEntity*> prefabs;

		//a place to store the probes
		std::vector<sProbe> probes;

		//Values that defnes the scene
		Vector3 ambient_light;
		Vector4 bg_color;
		float ambient_power;

		//Flags
		bool reverse_shadowmap; //Shadows flags
		bool AA_shadows;
		bool show_gbuffers;
		bool use_geometry_on_deferred;
		bool show_deferred_light_geometry;
		bool forward_for_blends;
		bool show_ssao;
		bool show_probes;
		bool use_irradiance;
		bool show_coefficients;
		bool interpolate_probes;

		Vector3 dim_grid;
		Vector3 start_pos_grid;
		Vector3 end_pos_grid;
		Vector3 delta_grid;
		float irr_normal_distance;

		Texture* probes_texture;

		// strict for writing probes to disk
		struct sIrrHeader {
			Vector3 start;
			Vector3 end;
			Vector3 delta;
			Vector3 dims;
			int num_probes;
		};

		//ctor
		Scene();

		void defineGrid(Vector3 offset, bool recomputing = false);
		void computeAllIrradianceCoefficients();
		void setIrradianceTexture();
		void SetIrradianceUniforms(Shader* shader);
		void writeProbesToDisk();
		bool loadProbesFromDisk();
		void AddEntity(BaseEntity* entity);
		void renderInMenu();

	};
};