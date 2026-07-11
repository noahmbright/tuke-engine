# Vulkan Backend TODO

## Highest Priority: Render Loop & Sync

**What leaks into application code today:**
- `vkCmdEndRenderPass` — raw Vulkan, belongs in the backend
- `NUM_ATTACHMENTS` — backend constant leaking into test code

**Render packet pattern (longer term):**
- Game produces a plain data struct (RenderPacket) each frame
- Renderer owns all per-frame GPU resources (N copies where N = frames in flight)
- Renderer routes packet fields to the right buffer slot using current_frame_index
- Game never sees MAX_FRAMES_IN_FLIGHT, current_frame_index, or double-buffering details

---

## Uniform Buffer Management

**No alignment in UniformBufferManager.**
push_uniform advances current_offset by raw size only. Offsets passed to vkUpdateDescriptorSets
must be multiples of minUniformBufferOffsetAlignment. Add alignment field, round up after each push.

**Replace push/handle/write pattern with typed struct.**
The ergonomic alternative: define a struct for the uniform layout, use offsetof for descriptor
bindings, write directly through the mapped pointer. No handles, no memcpy ceremony, alignment
handled by alignas on struct fields.
```cpp
struct MyUniforms { alignas(16) Mat4 mvp; alignas(4) float time; };
MyUniforms *u = (MyUniforms *)buf.mapped;
u->mvp = ...; u->time = t;
```

---

## Clear Values / Render Pass (Deferred)

Deferred pending render pass / dynamic rendering redesign. Issues to revisit:
- NUM_ATTACHMENTS leaks into application code
- Depth clear value appears in tests that don't use depth
- Consider VK_KHR_dynamic_rendering to drop render passes entirely
- Color-only render pass: create_color_render_pass exists but no framebuffer creation helper

---

## Missing Features

**Hot reloading. [HIGH PRIORITY]**
Two phases:
1. Game library: compile game logic as `.dylib`, main loop calls through function pointer table,
   `dlopen`/`dlclose` on file change. All game state lives in a blob owned by the executable
   and passed in each frame — library is stateless functions only.
2. Shader hot reload: poll `stat()` on shader source files, recompile on change,
   `vkDeviceWaitIdle`, swap `VkPipeline` in `VulkanMaterial`, destroy old pipeline.

**Pipeline variants.**
One pipeline per program spec works now, but transparent objects need depth write off and
blending enabled. Need a way to parameterize pipelines (blend mode, depth write, cull mode)
without duplicating shaders. A PipelineKey struct hashed into a pipeline cache is the standard
approach.

**Mipmaps.**
Textures loaded without mip chains. Visible quality degradation at distance, worse GPU cache
behavior. Generate mipmaps at load time via `vkCmdBlitImage` chain or load pre-built KTX.

**SSBOs for compute.**
Compute shaders need read/write buffers without the 64KB UBO size cap. SSBOs use a different
binding type (`VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`) and different GLSL qualifier (`buffer` vs
`uniform`). Buffer creation and descriptor write paths need to support this.

**Barriers between compute and graphics.**
Compute output consumed by a graphics pass (or vice versa) requires explicit pipeline barriers
(`vkCmdPipelineBarrier`) with correct src/dst stage and access masks. This is where sync gets
real — worth reading the Vulkan sync spec section before implementing.

**Memory suballocation.**
One `vkAllocateMemory` per `VulkanBuffer`, but meshes and uniforms are already batched into
single buffers per manager, so allocation count stays low in practice. Only relevant if the
number of distinct `BufferManager` / `UniformBuffer` instances grows into the thousands.
VMA is the standard solution if it ever matters.

**Bindless resources.**
Descriptor-per-material doesn't scale past a few hundred draw calls. Bindless: one large
descriptor array of textures/buffers, index into it via push constant per draw. Pairs naturally
with GPU-driven indirect drawing. Defer until scene complexity demands it.

---

## Housekeeping

**`vkQueueWaitIdle` in `end_single_use_command_buffer`.**
Blocks the CPU until the entire graphics queue drains. Correct for init-time uploads (texture
loading, layout transitions) but unusable for runtime streaming. If runtime texture loading is
ever needed, replace with a fence-based async path.

**Command buffers sized by `NUM_SWAPCHAIN_IMAGES` instead of `MAX_FRAMES_IN_FLIGHT`.**
`graphics_command_buffers[NUM_SWAPCHAIN_IMAGES]` — correct count is `MAX_FRAMES_IN_FLIGHT`.
Currently safe because `NUM_SWAPCHAIN_IMAGES (4) > MAX_FRAMES_IN_FLIGHT (2)`, but semantically
wrong and fragile if either constant changes.

---

## Window / Context Init (Deferred)

Deferred until a Renderer type is introduced. Issues to revisit:
- Three-line init (GLFWwindow, VulkanWindowInfo, VulkanContext) should collapse to two lines
- VulkanWindowInfo intermediate should be hidden from test code
- Window type should wrap the native handle and own the get_vulkan_info function pointer
- create_window(true /* is_vulkan */) bool should go away
- Render graph: https://www.youtube.com/watch?v=1Sb3s7Xie4M, https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
