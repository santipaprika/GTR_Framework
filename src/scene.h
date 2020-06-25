#pragma once

#include "framework.h"
#include "BaseEntity.h"
#include "extra/hdre.h"
#include <cassert>
#include <map>
#include <string>


namespace GTR {

	struct sProbe;
	struct sReflectionProbe;

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
		std::vector<sReflectionProbe*> reflection_probes;

		//Values that defnes the scene
		Texture* environment;
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
		bool show_rProbes;
		bool use_irradiance;
		bool show_coefficients;
		bool interpolate_probes;
		bool use_volumetric;
		bool use_reflections;

		Vector3 dim_grid;
		Vector3 start_pos_grid;
		Vector3 end_pos_grid;
		Vector3 delta_grid;
		float irr_normal_distance;

		Texture* probes_texture;

		std::string probes_filename;

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

		// Irradiance
		void defineIrradianceGrid(Vector3 offset);
		void computeIrradiance();
		void computeAllIrradianceCoefficients();
		void setIrradianceTexture();
		void SetIrradianceUniforms(Shader* shader);
		void writeProbesToDisk();
		bool loadProbesFromDisk();

		// Reflection
		void defineReflectionGrid(Vector3 offset);
		void computeReflection();

		void AddEntity(BaseEntity* entity);
		void renderInMenu();

	};
};