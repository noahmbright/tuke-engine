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

## Pipeline Creation

**render_pass parameter is redundant for the common case.**
shader_handles_to_graphics_pipeline takes render_pass explicitly despite the context having one.
Default to context->render_pass; override requires going through PipelineConfig directly.

**Inconsistent abstraction level.**
create_graphics_pipeline takes VkDevice + VkPipelineCache separately.
create_default_graphics_pipeline takes VulkanContext*. Pick one.

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

## Clear Values / Render Pass (Deferred)

Deferred pending render pass / dynamic rendering redesign. Issues to revisit:
- NUM_ATTACHMENTS leaks into application code
- VkClearValue union syntax is ugly; should accept glm::vec4 clear color
- Depth clear value appears in tests that don't use depth
- Consider VK_KHR_dynamic_rendering to drop render passes entirely
- Color-only render pass: create_color_render_pass exists but no framebuffer creation helper

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
