# Reflector TODO

## Descriptor Generation

**Per-label aggregate structs (replaces UniformBufferManager).**
Generate one C struct per label containing all bound uniform structs as fields, sorted by binding.
Buffer creation becomes `sizeof(LabelUniforms)`. Offsets come from `offsetof`. Alignment is
already handled correctly by the per-field `alignas` the reflector emits.

---

## Reflector / Shader Ergonomics (High Priority)

**Dev/release shader compilation split.**
Dev builds should compile shaders from source at startup (subprocess to glslc/glslangValidator,
same machinery the reflector already uses). Release builds use SPIR-V baked as C arrays by the
reflector (already emitted). Single `#ifdef TUKE_DEV` branch in `get_shader_spirv(handle)` —
`VkShaderModule` creation is identical in both paths. This is the low-cost path to hot reload:
startup recompile first (trivial), file watching later. Pipeline recreation on change requires
the render loop to hold pipelines by index/pointer so the handle can be swapped; `vkDeviceWaitIdle`
is acceptable for dev. Double-buffered pipelines needed only for seamless reload without a hitch.

---

## Shader Program Concept

**Combined shader file format.**
Allow vert and frag stages in one `.combined.in` file. Preamble (before `{{ VERTEX_BEGIN }}`)
is shared — version, bindings, shared declarations. Parser emits preamble slices + stage slices;
compilation duplicates the preamble for each stage. Bindings in the preamble get
`stage = VERTEX | FRAGMENT` automatically. `{{ LOCATION }}` directives are only valid after
`{{ VERTEX_BEGIN }}` — assert otherwise. Existing `.vert.in` / `.frag.in` pairs continue to work.
Compute stays standalone, no combined format.

---

## Generated Header Split

**Split generated output into a types header and a Vulkan data header.**
Currently everything lands in one `shaders.h`. Split into two:
- `shader_types.h` — enums (`ShaderHandle`, `ShaderProgram`, `VertexLayoutID`,
  `DescriptorSetLayoutID`, `UniformBufferLabel`), C structs from GLSL uniforms, and
  `#define` constants. No Vulkan types, no SPIR-V arrays. Safe to include anywhere.
- `shaders.h` (or `shader_vulkan.h`) — SPIR-V byte arrays, `VkDescriptorSetLayoutBinding`
  arrays, write templates, `ProgramSpec` literals, vertex layout `VkPipelineVertexInputStateCreateInfo`
  arrays. Includes `shader_types.h` and `<vulkan/vulkan.h>`.

This lets non-rendering code (simulation, asset system, UI logic) include only
`shader_types.h` without pulling in Vulkan headers.

---

## Minor / Deferred

- Drop `SET_BINDING N` from the directive syntax. Binding index becomes an assigned offset:
  sort instance names alphabetically within each label, assign indices 0..N-1 in that order.
  Deterministic across parse order. The `background.frag.in` class of bug becomes impossible.
- Add `const char *source_path` to `ShaderSpec` for hot reload.
- Allow a fragment shader to declare a shared vertex shader via directive, e.g.
  `{{ VERT_SHADER fullscreen_quad }}`. Enables N fullscreen-effect frag shaders to share
  one compiled vertex shader instead of duplicating it per program.
- Allow `vec3` in struct members. C side already emits `alignas(16)` so layout is correct.
  Fix `populate_glsl_struct_size` to count vec3 as 16 bytes (not 12) so push constant size
  assertions and pipeline layout ranges are correct.
