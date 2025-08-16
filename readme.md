
# Vulkan engine

Completed:
* Black screen
* Render a triangle
* Compile SPIR-V directly into C header
* Use a uniform to change a triangle's color in time
* Draw square and a triangle in one frame
* Nondynamic uniform buffer abstraction with persistently mapped memory
* Textures
* Descriptor set builder for uniforms and textures
* Shader caching
* Instancing
* Index buffers
* Descriptor set sizes out of reflection data
* Vertex layouts out of reflection data
* Depth buffer

TODO:
* Descriptor sets out of reflection
* Per frame uniform buffers
* Dynamic uniform buffer 
* Double/triple buffering
* Compute shaders
* Profiling
* Text rendering
* Swapchain recreation
* Materials
* Wireframe debug
* Screen shake
* Particles
* Vulkan Sync
* Normal mapping 
* move from combined image sampler to samplers + images
* Push constants

General
TODO:
* Lighting/shadows
* 2D characters with heights
* How to buffer a level
* Aspect ratio
* Textures/sprite/how to make sprites/atlas
* Automate vertex uploads with generated code

From GPT
# Vulkan Backend TODOs

- [ ] Implement image layout transitions for textures
- [ ] Implement shader hot reloading for faster iteration
- [ ] Implement shadow mapping pipeline
- [ ] Implement PBR shading with IBL
- [ ] Create a scene graph or ECS if gameplay demands it
- [ ] Implement deferred rendering pipeline if forward rendering becomes limiting
- [ ] Implement GPU-driven culling or compute-based rendering for large scenes
- [ ] Design interactive worlds with instancing and texture rendering
- [ ] Prototype gameplay mechanics leveraging existing rendering capabilities

# Principle
Build only what is required to unlock your immediate creative goals; refine abstractions and add infrastructure as project demands grow.

1. Phong Lighting with Multiple Light Types
    Uniforms:
        Light position, color, intensity (point/spot/directional)
        Material shininess, ambient/diffuse/specular coefficients
    Why?
    Classic lighting shader. Multiple lights mean arrays of structs (challenge: array handling, alignment). Per-material uniforms like shininess also needed.

2. Normal Mapping
    Uniforms:
        Tangent-space matrix (TBN matrix)
        Normal map texture sampler

    Why?
    Requires per-vertex or per-pixel transforms with additional uniforms. Handling combined uniforms + samplers is good practice.

3. Screen-Space Ambient Occlusion (SSAO)
    Uniforms:
        Kernel samples (array of vec3)
        Noise texture sampler
        Projection parameters
    Why?
    Lots of small arrays and textures, testing uniform buffer packing and texture descriptor management.

4. Depth of Field Blur
    Uniforms:
        Focus distance and range
        Blur radius
        Possibly additional sample weights
    Why?
    Post-processing effect with scalar and array uniforms — good for exploring uniform update frequency and data locality.

5. Screen-Space Reflections
    Uniforms:
        Camera matrices
        Screen parameters
        Reflection intensity and roughness parameters
    Why?
    Complex screen-space effect needing many uniforms and possibly multiple textures.

6. Parallax Occlusion Mapping
    Uniforms:
        Height scale
        View direction vector
        Textures: height map, diffuse map
    Why?
    Requires careful per-material parameter updates and texture handling.

7. Water Shader with Wave Animation
    Uniforms:
        Time parameter for wave animation
        Wave amplitude and frequency
        Reflection and refraction textures
    Why?
    Dynamic animation uniforms mixed with multiple textures.

8. Outline Shader
    Uniforms:
        Outline thickness
        Outline color
    Why?
    Simple scalar uniforms but useful for seeing how minimal uniforms fit into your system.

9. Skinning (Skeletal Animation)
    Uniforms:
        Array of bone transformation matrices
    Why?
    Large array uniforms challenge uniform buffer size limits and dynamic offset usage.

10. Volumetric Fog
    Uniforms:
        Fog density, color
        Height falloff
        Light scattering parameters
    Why?
    Continuous effect with scalar and vector uniforms that affect lighting calculations.
