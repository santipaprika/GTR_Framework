#pragma once
#include "prefab.h"
#include "BaseEntity.h"
#include "scene.h"

//forward declarations
class Camera;
namespace GTR {

	class Prefab;
	class Material;
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:

		//to render the scene to texture (shadowmap)
		std::vector<GTR::Light*> renderSceneShadowmaps(GTR::Scene* scene);

		//to render the scene to viewport
		void renderSceneToScreen(GTR::Scene* scene, Camera* camera);
		
		void renderPointShadowmap(Light* light);

		//to render a scene
		void renderScene(GTR::Scene* scene, Camera* camera);
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		void renderWithoutLights(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
		
		void renderMultiPass(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
		
		Shader* chooseShader(GTR::Light* light);

		void enableShader(Shader* shader);
		//render shadowmap
		void renderSimple(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		void showSceneShadowmaps(std::vector<Light*> shadow_caster_lights);

		void setDefaultGLFlags();

		//manages blendign
		void manageBlendingAndCulling(GTR::Material* material, bool rendering_light, bool is_first_pass = true);
	};

};