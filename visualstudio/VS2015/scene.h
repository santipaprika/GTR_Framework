#pragma once

#include "framework.h"
#include "BaseEntity.h"
#include <cassert>
#include <map>
#include <string>


namespace GTR {

	//this class contains all info relevant of how to create and define the scene
	class Scene {
	public:

		//instance delcaration
		static Scene* instance;

		//list to store al entities of the scene
		std::vector<BaseEntity*> sceneEntities;

		//Values that defnes the scene
		Vector3 ambient_light;
		float ambient_power;
		int num_lights;
		bool reverse_shadowmap; //Shadows flags
		bool AA_shadows;

		//ctor
		Scene();

		void addPrefab(Prefab* entity, Vector3 pos = Vector3(0, 0, 0), const char* name = "Prefab", float angle = 0.0, Vector3 axis = Vector3(0, 1, 0), Prefab* lowres_entity = NULL);
		void addLight(Vector3 pos = Vector3(0, 0, 0), Vector3 color = Vector3(0.5, 0.5, 0.5), float intensity = 2, const char* name = "Light",
			eLightType ltype = POINT, Vector3 dir = Vector3(0, -1, 0), bool cast = false);
		void addCamera(Camera entity, const char* name = "Camera");
		void renderInMenu();

	};
};