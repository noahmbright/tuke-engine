# Vulkan Backend TODO

## Highest Priority: Render Loop & Sync

The render loop is the biggest hole in the infrastructure. Game and test code currently
manages backend state and calls raw Vulkan — this all needs to move into the backend.

**What leaks into application code today:**
- `vkCmdEndRenderPass` — raw Vulkan, belongs in the backend
- `vkEndCommandBuffer` — raw Vulkan, belongs in the backend
- `vkCmdSetViewport` / `vkCmdSetScissor` — always called together after begin_render_pass, should fold in
- `context.framebuffers[context.image_index]` — app manages framebuffer selection
- `context.current_frame++` / `context.current_frame_index = ...` — app increments backend state
- `NUM_ATTACHMENTS` — backend constant leaking into test code

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

## Window / Context Init (Deferred)

Deferred until a Renderer type is introduced. Issues to revisit:
- Three-line init (GLFWwindow, VulkanWindowInfo, VulkanContext) should collapse to two lines
- VulkanWindowInfo intermediate should be hidden from test code
- Window type should wrap the native handle and own the get_vulkan_info function pointer
- create_window(true /* is_vulkan */) bool should go away
