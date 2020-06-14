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

		//ctor
		Scene();

		void defineGrid();
		void computeAllIrradianceCoefficients();
		void AddEntity(BaseEntity* entity);
		void renderInMenu();

	};
};