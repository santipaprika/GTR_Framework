# Real Time Realistic Renderer

## Main Concept
This program renders a scene in real time built in the context of a final project for the UPF course "Real-Time Graphics", held by [Javi Agenjo](https://github.com/jagenjo). The following features have been implemented (most of them can be tuned through the visual debugging interface):

**Rendering Pipelines**
- Forward
- Deferred

**Deferred illumination optimization**
- Compute light only on projected points contained inside the light area of effect.

**Dynamic lights**
- For Point, Spot and Directional lights.
- Color, Intensity, etc.

**Dynamic Shadows**
- Real-Time shadowmaps.
- Working for Spot and Directional lights, and partially working for Point Lights.

**Light Dependant Materials**
- Phong
- PBR

**Screen Space Ambient Occlusion** 
- SSAO texture
- Blur SSAO texture
- Blur parameters (ImGUI): Kernel size (0 = no blur), number of blurs to apply.
- Other parameters (ImGUI):  Use hemisphere aligned to normal (SSAO+), number of points generated in the sphere, radius of the sphere.

**Irradiance:**
- Irradiance weight (ImGUI parameter).
- Irradiance probes (visible via ImGUI).
- Coefficient texture (visible via ImGUI).
- Irradiance ignored at points outside the probes grid.
- Irradiance recomputation “in-game” (ImGUI button).
- Probe normal distance (ImGUI parameter): distance to add in the normal direction before choosing the closest probe (each unit will choose the next probe).
- Probe interpolation (toggled via ImGUI).
- Write / Read probes coefficients from disk: If there is an existing “irradiance.bin” file in the root project directory, the spherical coefficients for each probe will be loaded from it (on start). Otherwise, a new “irradiance.bin” file will be created at the same root directory. When the “regenerate irradiance” button is pressed, a new coefficients file is generated overwriting the previous one, so it is recommended to delete the “irradiance.bin” file if it has been recomputed in-game with a modified scene.

**Volumetric light**
- Working for directional and spotlights.
- Jittering: non-uniform start of rays using noise (to avoid banding) by reading to a 2D texture.
- Undersampling: we store the light shafts in a lower texture.
- Apply blur to the volumetric texture.
- Parameters (ImGUI): quality (number of steps of the raymarching algorithm), air density (contribution in alpha at each step), and distance of field (how far the sample of the ray takes to goes completely opaque).

**Tone Mapper**
- Param (ImGUI): gamma value, luminance intensity, tone mapper scale, and the average luminance of the frame.

**Decals**
- Both the color and material textures of the decal being used can be found in the textures folder.
- Material texture: [occlusion, roughness, metalness].


**Reflection** 
- Reflection probes (ImGUI parameter)
- Probe normal distance (ImGUI parameter): distance to add in the normal direction before choosing the closest probe.

Any material physical behavior (roughness and metalness) can be modified via ImGUI inside each specific node.

## Software Engine
This program has been developed using the framework provided by Javi Agenjo (in C++, and using shaders via OpenGL) and with his assistance.

## Demo video
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/SdBhR91hi8U/0.jpg)](https://youtu.be/SdBhR91hi8U)

## Contributions
**Software Developers**: Santi Paprika, [Víctor Ubieto](https://github.com/victorubieto)   
**Framework developer**: [Javi Agenjo](https://github.com/jagenjo) (UPF teacher)   
**Assistance**: [Javi Agenjo](https://github.com/jagenjo) (UPF teacher)   
**Maps**: [Renafox - Sketchfab](https://sketchfab.com/kryik1023)    
