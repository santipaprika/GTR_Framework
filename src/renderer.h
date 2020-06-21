#pragma once
#include "prefab.h"
#include "BaseEntity.h"
#include "scene.h"
#include "sphericalharmonics.h"

//forward declarations
class Camera;
namespace GTR {

	class Prefab;
	class Material;
	
	//struct to store probes
	struct sProbe {
		Vector3 pos; //where is located
		Vector3 local; //its ijk pos in the matrix
		int index; //its index in the linear array
		SphericalHarmonics sh; //coeffs
	};


	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		// DEFERRED
		void renderGBuffers(Scene* scene, Camera* camera);

		void renderToGBuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		void renderSSAO(Camera* camera);

		void renderIlluminationToBuffer(Camera* camera);

		void showGBuffers();

		void showSSAO();

		// MULTIPASS

		//to render the scene to viewport
		void renderSceneForward(GTR::Scene* scene, Camera* camera);

		//manages blendign
		void manageBlendingAndCulling(GTR::Material* material, bool rendering_light, bool is_first_pass = true);
		
		//to render the scene to texture (shadowmap)
		std::vector<GTR::Light*> renderSceneShadowmaps(GTR::Scene* scene);

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

		// PROBES
		void computeIrradianceCoefficients(sProbe &probe, Scene* scene);

		void renderProbe(Vector3 pos, float size, float* coeffs);
	};

	Texture* CubemapFromHDRE(const char* filename);

};