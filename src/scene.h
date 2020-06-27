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
	class Scene 
	{
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


		//ctor
		Scene();

		// Irradiance
		void defineIrradianceGrid(Vector3 offset);

		// Reflection
		void defineReflectionGrid(Vector3 offset);
		void placeReflectionProbe(Vector3 pos, Vector3 offset = Vector3(0,0,0));

		void AddEntity(BaseEntity* entity);
		void renderInMenu();

	};
};