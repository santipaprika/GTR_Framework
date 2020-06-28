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

		// FBOs & Textures
		FBO* illumination_fbo;
		FBO* gbuffers_fbo;
		FBO* ssao_fbo;
		Texture* ssao_blur;
		Texture* probes_texture;
		FBO* irr_fbo;
		FBO* reflections_fbo;
		FBO* reflections_component;
		FBO* volumetrics_fbo;
		Texture* depth_texture_aux;
		Texture* normal_texture_aux;

		// FLAGS
		bool show_gbuffers;					//Deferred general
		bool use_geometry_on_deferred;
		bool show_deferred_light_geometry;
		bool forward_for_blends;
		
		bool use_ssao;						//SSAO
		bool use_ssao_plus;
		bool show_ssao;
		int kernel_size;
		float sphere_radius;
		int number_blur;
		int number_points;

		bool reverse_shadowmap;				//Shadows
		bool AA_shadows;
		bool rendering_shadowmap;

		bool use_gamma_correction;			//Color correction
		bool use_tone_mapping;
		
		bool use_irradiance;				//Irradiance
		bool show_probes;
		bool show_coefficients;
		bool interpolate_probes;
		float irr_normal_distance;
		float refl_normal_distance;

		bool use_reflections;				//Reflection
		bool show_rProbes;

		bool use_volumetric;				//Volumetric
		int u_quality;
		float u_air_density;
		float u_clamp;

		bool show_decal;					//Decal
		Mesh* decal_cube;
		
		// Vectors, Aux and Imgui
		std::vector<GTR::Light*> shadow_caster_lights;	//vector that stores the lights that uses shadows
		std::vector<Vector3> random_points;				//vector of random points for the irradiance
		Vector3 dim_grid;								//Grid parameters for irradiance
		Vector3 start_pos_grid;
		Vector3 end_pos_grid;
		Vector3 delta_grid;
		std::string probes_filename;					//Name of the file that stores the probes


		// FLAGS & SELECTORS
		void initFlags();
		void setDefaultGLFlags();
		void manageBlendingAndCulling(GTR::Material* material, bool rendering_light, bool is_first_pass = true);
		void enableShader(Shader* shader);
		Shader* chooseShader(GTR::Light* light);

		// RENDER to Buffers
		void renderGBuffers(Scene* scene, Camera* camera);
		void renderToGBuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
		std::vector<GTR::Light*> renderSceneShadowmaps(GTR::Scene* scene); //to render the scene to texture (shadowmap)
		void renderSSAO(Camera* camera);
		void renderIlluminationToBuffer(Camera* camera);
		void renderReflectionsToBuffer(Camera* camera);
		
		// BUFFERS to Viewport
		void renderToViewport(Camera* camera, Scene* scene);
		void showGBuffers();
		void showSceneShadowmaps();
		void showSSAO();

		// PROBES
		void renderIrradianceProbe(Vector3 pos, float size, float* coeffs);		//Render
		void renderReflectionProbe(Vector3 pos, float size, Texture* cubemap);
		void writeProbesToDisk(Scene* scene);									//Store to disk
		bool loadProbesFromDisk(Scene* scene);
		void computeIrradiance(Scene* scene);									//Irradiance
		void computeIrradianceCoefficients(sProbe &probe, Scene* scene);
		void computeAllIrradianceCoefficients(Scene* scene);
		void setIrradianceTexture(Scene* scene);
		void SetIrradianceUniforms(Shader* shader, Scene* scene);
		void computeReflection(Scene* scene);									//Reflection

		// MAIN Render Scene
		void renderScene(GTR::Scene* scene, Camera* camera); //to render a scene
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera); //to render a whole prefab (with all its nodes)
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera); //to render one node from the prefab and its children
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera); //to render one mesh given its material and transformation matrix
		// other render types
		void renderSceneForward(GTR::Scene* scene, Camera* camera); //forward render to viewport
		void renderPointShadowmap(Light* light);
		void renderMultiPass(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera); //render for each light in the scene
		void renderWithoutLights(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera); //render if no lights
		void renderSimple(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera); //for the shadowmap
		void renderSkybox(Camera* camera, Texture* environment);
		
		void renderInMenu(Scene* scene); //ImGUI

	};

	Texture* CubemapFromHDRE(const char* filename);

};