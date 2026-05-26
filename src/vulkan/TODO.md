# Vulkan Backend TODO

## Highest Priority: Render Loop & Sync

The render loop is the biggest hole in the infrastructure. Game and test code currently
manages backend state and calls raw Vulkan — this all needs to move into the backend.

**What leaks into application code today:**
- `vkCmdEndRenderPass` — raw Vulkan, belongs in the backend
- `vkEndCommandBuffer` — raw Vulkan, belongs in the backend
- `vkCmdSetViewport` / `vkCmdSetScissor` — always called together after begin_render_pass, should fold in
- `context.framebuffers[context.image_index]` — app manages framebuffer selection
- `NUM_ATTACHMENTS` — backend constant leaking into test code

**Done:**
- `context.current_frame++` / `context.current_frame_index = ...` — replaced by `update_frame_index()`
- `current_frame` removed from `VulkanContext` — now lives in game State

**Target API:**
```cpp
if (!begin_frame(&context)) continue;
VkCommandBuffer cb = begin_frame_commands(&context, clear_color);
render_mesh(cb, &render_call);
end_frame(&context, cb);  // EndRenderPass + EndCommandBuffer + submit + present + increment index
```

**Frame index ownership:**
- `current_frame_index` stays in context, incremented inside `end_frame`, never touched by app
- `current_frame` (game frame number) belongs to game logic, remove from VulkanContext
- App queries `context.current_frame_index` to index its own per-frame resources (double-buffered uniforms etc.)
- `MAX_FRAMES_IN_FLIGHT` must not leak past the renderer layer into game logic

**Render packet pattern (longer term):**
- Game produces a plain data struct (RenderPacket) each frame
- Renderer owns all per-frame GPU resources (N copies where N = frames in flight)
- Renderer routes packet fields to the right buffer slot using current_frame_index
- Game never sees MAX_FRAMES_IN_FLIGHT, current_frame_index, or double-buffering details

---

## Reflector / Shader Ergonomics (High Priority)

Adding new shaders is too painful. Main issues:

**No shader program concept.**
Vert and frag are completely independent handles. Pipeline creation requires two handles with
no reflector-level validation that stages are compatible. The reflector should pair foo.vert.in
+ foo.frag.in by name and emit a single program handle:
```cpp
// current
shader_handles_to_graphics_pipeline(&context, render_pass,
    SHADER_HANDLE_COMMON_HARDCODED_TRIANGLE_VERT,
    SHADER_HANDLE_COMMON_HARDCODED_TRIANGLE_FRAG, layout);
// target
create_pipeline(&context, SHADER_HARDCODED_TRIANGLE, layout);
```

**Single-file shader format (additive, not a rewrite).**
Allow writing vert and frag stages in one `.shader.in` file, separated by a stage marker:
```glsl
#stage vertex
// ... vert source ...

#stage fragment
// ... frag source ...
```
The reflector splits on the marker, processes each stage through the existing `.vert.in` / `.frag.in`
pipeline, and emits a single program handle. Existing `.vert.in` + `.frag.in` pairs continue to
work — the single-file format is an extension, not a replacement. Migrate shaders incrementally.
Primary benefit: one enum instead of two, vert/frag varyings visible in the same file so
interface mismatches are obvious at a glance.

**Enum names encode filesystem path.**
`SHADER_HANDLE_COMMON_HARDCODED_TRIANGLE_VERT` — the SHADER_HANDLE_ prefix, COMMON_ directory
component, and _VERT stage suffix are all noise at the callsite. Drop prefix and stage suffix
for program-level handles. Keep directory prefix only on collision.

**Descriptor wiring is manual despite reflector knowing everything.**
The reflector extracts set/binding/type for every uniform and sampler. It should generate
descriptor set layout creation helpers per program. Manual add_uniform_buffer_descriptor_set
calls should be replaced by generated code.

**No source path in ShaderSpec.**
Hot reloading needs the path to the .in file at runtime. Add `const char *source_path` to
ShaderSpec. Dev builds watch this path and trigger recompilation + pipeline recreation.

**Dev/release shader compilation split.**
Dev builds should compile shaders from source at startup (subprocess to glslc/glslangValidator,
same machinery the reflector already uses). Release builds use SPIR-V baked as C arrays by the
reflector (already emitted). Single `#ifdef TUKE_DEV` branch in `get_shader_spirv(handle)` —
`VkShaderModule` creation is identical in both paths. This is the low-cost path to hot reload:
startup recompile first (trivial), file watching later. Pipeline recreation on change requires
the render loop to hold pipelines by index/pointer so the handle can be swapped; `vkDeviceWaitIdle`
is acceptable for dev. Double-buffered pipelines needed only for seamless reload without a hitch.

**Push constant parsing is a stub.**
parse_push_constant_directive() does nothing.

**init_generated_shader_vk_modules** should not be the caller's job.
Should happen inside create_vulkan_context or be triggered automatically.
Deferred until reflector redesign.

---

## Pipeline Creation

**Wrong blend default.**
create_default_graphics_pipeline_config defaults to BLEND_MODE_ALPHA. Most geometry is opaque.
Default should be BLEND_MODE_OPAQUE.

**Dead field: PipelineConfig.swapchain_extent.**
create_graphics_pipeline computes a static viewport from swapchain_extent, but dynamic state
(VK_DYNAMIC_STATE_VIEWPORT + SCISSOR) is always enabled, so the static viewport is never used.
Remove the field and the dead computation.

**Unnecessary middle wrapper.**
create_default_graphics_pipeline only exists to call create_default_graphics_pipeline_config
then create_graphics_pipeline. Remove it; shader_handles_to_graphics_pipeline should build
a config and call create_graphics_pipeline directly.

**render_pass parameter is redundant for the common case.**
shader_handles_to_graphics_pipeline takes render_pass explicitly despite the context having one.
Default to context->render_pass; override requires going through PipelineConfig directly.

**Inconsistent abstraction level.**
create_graphics_pipeline takes VkDevice + VkPipelineCache separately.
create_default_graphics_pipeline takes VulkanContext*. Pick one.

**generated_shader_utils.cpp mixes concerns.**
Shader module init/teardown belongs there. Pipeline creation (shader_handles_to_graphics_pipeline)
is a pipeline concern and should move closer to create_graphics_pipeline once program handles exist.
OpenGL shader linking (`link_gl_program`) has been moved to `opengl_base.h` — Vulkan and OpenGL
concerns are now separated at the file level. The include of `opengl_base.h` from
`generated_shader_utils.h` still couples them at compile time; deferred until per-backend
compilation is addressed.

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
struct MyUniforms { alignas(16) glm::mat4 mvp; alignas(4) float time; };
MyUniforms *u = (MyUniforms *)buf.mapped;
u->mvp = ...; u->time = t;
```

**write_to_uniform_buffer should take explicit data_size.**
Currently uses uniform_write.size as the memcpy length. When caller passes a smaller type
into a larger slot it reads past the variable. Add data_size parameter, assert data_size <= write.size.

---

## Material Setup (Deferred — needs more drawn worlds first)

Adding a new drawable currently requires 7 touch points: a new `DescriptorHandleID` enum entry,
a new `UniformWrite` field, a `push_uniform` call, a descriptor set builder block, a new
`init_X_material` function (~30 lines of pipeline layout + pipeline + RenderCall population),
a `render_mesh` call in the render loop, and a destroy call. Seen clearly in pong.

The right abstraction here is not obvious yet. A `create_material` helper that collapses
pipeline layout + pipeline + RenderCall into one call is plausible, but the shape of a material
system — how descriptors are owned, how programs map to pipelines, what varies per-instance vs.
per-material — needs to be developed from experience drawing many simple meshes and worlds.
Do not draft a material system. Draw things first. Let the abstraction emerge.

Specific rough edges to revisit once the pattern is clear:
- `Material` does not own all its descriptors — `init_background_material` takes external
  handles and manually assembles the layout array.
- `UniformWrites` struct in pong manually mirrors the `push_uniform` call sequence; adding
  a uniform requires touching both in sync.
- `DescriptorHandleID` enum grows by one per drawable regardless of whether descriptors are
  actually distinct.

---

## Depth Buffer

**Image view missing stencil aspect bit.**
create_depth_buffer creates the image view with VK_IMAGE_ASPECT_DEPTH_BIT only.
D32_SFLOAT_S8_UINT requires VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
for framebuffer attachments when separateDepthStencilLayouts is not enabled.

---

## Clear Values / Render Pass (Deferred)

Deferred pending render pass / dynamic rendering redesign. Issues to revisit:
- NUM_ATTACHMENTS leaks into application code
- VkClearValue union syntax is ugly; should accept glm::vec4 clear color
- Depth clear value appears in tests that don't use depth
- Consider VK_KHR_dynamic_rendering to drop render passes entirely
- Color-only render pass: create_color_render_pass exists but no framebuffer creation helper

---

## Housekeeping

**`state.current_frame` double increment in pong.**
`render()` does `state->current_frame++` and `main.cpp` does `state.current_frame++` after
returning from `render`. Frame counter incremented twice per loop; FPS reads double. Remove
the increment from `render()` — the counter belongs to the loop owner.

**`BUFFER_SIZE` macro define/undef in `opengl_base.h`.**
`#define BUFFER_SIZE 512` sits at file scope in a header, undef'd after `link_gl_program`.
Fragile pattern. Replace with `constexpr int` or a plain literal inside the function.

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
