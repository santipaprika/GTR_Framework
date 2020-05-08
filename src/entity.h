#ifndef ENTITY_H
#define ENTITY_H

#include "utils.h"
#include "prefab.h"
#include "shader.h"
#include "camera.h"
#include "fbo.h"
#include "mesh.h"

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

		//TO DO
		/*bool receive_light = true;
		bool cast_shadow = true;
		Vector4 entity_color = Vector4(1, 1, 1, 1);*/

		PrefabEntity(Prefab* prefab, Vector3 position = Vector3(0,0,0), Vector3 rotation = Vector3(0,0,0), bool visible = true);
		void renderInMenu();
		bool clickOnIt(Camera* camera, Node* node = NULL);
	};

	enum eLightType { DIRECTIONAL, POINT, SPOT };
	class Light : public BaseEntity
	{
	public:
		Vector3 color; //the color of the light
		float intensity; //the amount of light emited
		float max_distance = 500.0; //max distance of light effect
		eLightType light_type; //if the light is OMNI, SPOT or DIRECTIONAL
		Camera* camera;
		float ortho_cam_size = 800;
		FBO* shadow_fbo = NULL;
		float shadow_bias = 0.001;
		bool cast_shadows = true;
		Matrix44 shadow_viewprojs[6];

		float spot_cutoff_in_deg = 45;
		float spot_exponent = 10;

		PrefabEntity* lightSpherePrefab;

		Light(Color color = Color(1, 1, 1, 1), Vector3 position = Vector3(0, 0, 0), Vector3 frontVector = Vector3(0,-0.9,0.1), eLightType light_type = POINT, float intensity = 1, bool visible = true);
		void initializeLightCamera();
		void setVisiblePrefab();
		void setUniforms(Shader* shader);
		void updateLightCamera(bool type_changed = false);
		void setCutoffAngle(float angle_in_rad);
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
