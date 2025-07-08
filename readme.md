
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

TODO:
* Per frame uniform buffers
* Shader caching
* Dynamic uniform buffer 
* Instancing
* Double/triple buffering
* Index buffers
* Compute shaders
* Profiling
* Text rendering
* Swapchain recreation
* Materials
* Wireframe debug
* Screen shake
* Particles

General
TODO:
* Lighting/shadows
* 2D characters with heights
* How to buffer a level
* Aspect ratio
* Textures/sprite/how to make sprites/atlas
* Print glsl compiler errors in color from python script
* Hash table

From GPT
# Vulkan Backend TODOs

## ✅ Immediate Next Steps
- [ ] Implement texture loading and staging uploads
- [ ] Implement image layout transitions for textures
- [ ] Create image views and samplers for texture binding
- [ ] Integrate UV coordinates into vertex data pipeline
- [ ] Sample textures in fragment shaders to verify setup

## 🔜 Near-Term Infrastructure
- [ ] Load models with positions, normals, UVs, and indices (e.g. OBJ or glTF)
- [ ] Implement a camera system with view and projection matrices
- [ ] Add basic input handling for camera movement (WASD + mouse)
- [ ] Ensure robust synchronization, frame submission, and swapchain recreation
- [ ] Implement basic directional lighting in shaders
- [ ] Optional: Implement shader hot reloading for faster iteration

## 💡 Optional Enhancements (Future, as needed)
- [ ] Implement shadow mapping pipeline
- [ ] Add normal mapping support
- [ ] Implement PBR shading with IBL
- [ ] Create a scene graph or ECS if gameplay demands it
- [ ] Implement deferred rendering pipeline if forward rendering becomes limiting
- [ ] Implement GPU-driven culling or compute-based rendering for large scenes

## 🎯 Creative Focus Unlock
- [ ] Prototype interesting shaders for visual effects
- [ ] Design interactive worlds with instancing and texture rendering
- [ ] Prototype gameplay mechanics leveraging existing rendering capabilities

# Principle
Build only what is required to unlock your immediate creative goals; refine abstractions and add infrastructure as project demands grow.
