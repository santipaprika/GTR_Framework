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
		const char* name;
		unsigned int id; //unique number
		Matrix44 model; //defines where it is
		bool visible; //says if the object must be rendered
		eType type; //tells us the type of this entity from a list of possible types

		//ctor
		BaseEntity(Vector3 position = Vector3(0, 0, 0), Vector3 rotation = Vector3(0, 0, 0), const char* name = "Base Ent", eType type = BASE_NODE, bool visible = true);
	};

	class PrefabEntity : public BaseEntity
	{
	public:
		Prefab* prefab;
		Prefab* lowres_prefab;

		//TO DO
		/*bool receive_light = true;
		bool cast_shadow = true;
		Vector4 entity_color = Vector4(1, 1, 1, 1);*/

		//ctor
		PrefabEntity(Prefab* prefab, Vector3 position = Vector3(0,0,0), Vector3 rotation = Vector3(0,0,0), const char* name = "prefab", Prefab* lowres_prefab = NULL, bool visible = true);
		
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
		
		// Shadows
		Camera* camera;
		float ortho_cam_size = 800;
		FBO* shadow_fbo = NULL;
		float shadow_bias = 0.001;
		Matrix44 shadow_viewprojs[6];

		// Flags
		bool cast_shadows;
		bool show_shadowmap;
		bool update_shadowmap;

		// Spot parameters
		float spot_cutoff_in_deg;
		float spot_exponent;

		Node* light_node;

		//ctor
		Light(Color color = Color(1, 1, 1, 1), Vector3 position = Vector3(0, 0, 0), Vector3 frontVector = Vector3(0,-0.9,0.1), const char* name = "light", eLightType light_type = POINT, float intensity = 1, bool visible = true);
		
		void initializeLightCamera();
		void setVisiblePrefab();
		void setLightUniforms(Shader* shader);
		void setShadowUniforms(Shader* shader);
		void updateLightCamera(bool type_changed = false);
		void setCutoffAngle(float angle_in_rad);
		void renderInMenu();
	};
}
#endif
