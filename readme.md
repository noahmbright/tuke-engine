
Vulkan engine:
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

TODO:
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
