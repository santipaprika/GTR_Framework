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

	//struct to store reflection probes info
	struct sReflectionProbe {
		Vector3 pos;
		Texture* cubemap = NULL;
	};

	// strict for writing probes to disk
	struct sIrrHeader {
		Vector3 start;
		Vector3 end;
		Vector3 delta;
		Vector3 dims;
		int num_probes;
	};


	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:

		//FBOs
		FBO* gbuffers_fbo;
		FBO* illumination_fbo;
		FBO* irr_fbo;
		FBO* reflections_fbo;
		FBO* reflections_component;
		FBO* volumetrics_fbo;

		//Flags
		bool show_gbuffers;
		bool use_geometry_on_deferred;
		bool show_deferred_light_geometry;
		bool forward_for_blends;
		//ssao
		bool use_ssao;
		bool show_ssao;
		FBO* ssao_fbo;
		Texture* ssao_blur;
		std::vector<Vector3> random_points;
		int kernel_size;
		float sphere_radius;
		int number_blur;

		bool reverse_shadowmap;
		bool AA_shadows;
		bool rendering_shadowmap;
		std::vector<GTR::Light*> shadow_caster_lights;

		bool use_gamma_correction;
		bool use_tone_mapping;

		// Irradiance
		bool use_irradiance;
		bool show_probes;
		bool show_coefficients;
		bool interpolate_probes;
		float irr_normal_distance;
		float refl_normal_distance;

		Vector3 dim_grid;
		Vector3 start_pos_grid;
		Vector3 end_pos_grid;
		Vector3 delta_grid;
		Texture* probes_texture;
		std::string probes_filename;

		bool use_reflections;
		bool show_rProbes;

		bool use_volumetric;


		// DEFERRED
		void renderGBuffers(Scene* scene, Camera* camera);

		void renderToGBuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		void renderSSAO(Camera* camera);

		void renderIlluminationToBuffer(Camera* camera);

		void renderReflectionsToBuffer(Camera* camera);

		void showGBuffers();

		void showSSAO();

		//Irradiance
		void computeIrradiance(Scene* scene);
		void computeAllIrradianceCoefficients(Scene* scene);
		void setIrradianceTexture(Scene* scene);
		void SetIrradianceUniforms(Shader* shader, Scene* scene);
		void writeProbesToDisk(Scene* scene);
		bool loadProbesFromDisk(Scene* scene);

		void computeReflection(Scene* scene);

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

		void showSceneShadowmaps();

		void setDefaultGLFlags();

		// PROBES
		void computeIrradianceCoefficients(sProbe &probe, Scene* scene);

		void renderIrradianceProbe(Vector3 pos, float size, float* coeffs);

		void renderReflectionProbe(Vector3 pos, float size, Texture* cubemap);

		// SKYBOX
		void renderSkybox(Camera* camera, Texture* environment);

		void initFlags();

		void renderToViewport(Camera* camera, Scene* scene);

		void renderInMenu(Scene* scene);
	};

	Texture* CubemapFromHDRE(const char* filename);

};