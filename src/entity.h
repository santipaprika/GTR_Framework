#ifndef ENTITY_H
#define ENTITY_H

#include "utils.h"
#include "prefab.h"
#include "shader.h"

namespace GTR {

	enum eType { BASE_NODE, PREFAB, LIGHT };
	class BaseEntity
	{
	public:
		unsigned int id; //unique number
		Matrix44 model; //defines where it is
		bool visible; //says if the object must be rendered
		eType type; //tells us the type of this entity from a list of possible types

		BaseEntity(Vector3 position = Vector3(0, 0, 0), Vector3 rotation = Vector3(0, 0, 0), eType type = BASE_NODE, bool visible = true);
	};

	class PrefabEntity : public BaseEntity
	{
	public:
		Prefab* prefab;

		PrefabEntity(Prefab* prefab, Vector3 position = Vector3(0,0,0), Vector3 eulerAngles = Vector3(0,0,0), bool visible = true);
		void renderInMenu();
	};

	enum eLightType { DIRECTIONAL, POINT, SPOT };
	class Light : public BaseEntity
	{
	public:
		Vector3 color; //the color of the light
		float intensity; //the amount of light emited
		float max_distance = 50; //max distance of light effect
		eLightType light_type; //if the light is OMNI, SPOT or DIRECTIONAL

		float spotCosineCutoff = 0.5;
		float spotExponent = 10;

		Light(Color color = Color(1, 1, 1, 1), Vector3 position = Vector3(0, 0, 0), Vector3 eulerAngles = Vector3(0,0,0), eLightType light_type = POINT, float intensity = 1, bool visible = true);
		void setUniforms(Shader* shader);
		void renderInMenu();
	};

	class Scene
	{
	public:
		static Scene* instance;
		std::vector<Light*> lights;
		std::vector<PrefabEntity*> prefabs;
		Vector3 ambientLight;

		Scene();
		void AddEntity(BaseEntity* entity);
		void renderInMenu();
	};

}
#endif
