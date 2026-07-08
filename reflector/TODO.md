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

## Pipeline / Descriptor Set Architecture

**Hoist pipeline to shader program level, not material level.**
Currently each material owns both a pipeline and descriptor sets. Materials that share the same
shader program duplicate the pipeline. Pipeline should live at the `ShaderProgram` level; a
material becomes just a set of descriptor sets bound to a program. This makes per-scene UI
texture swapping cheap — same program, different descriptor set, one `vkCmdBindDescriptorSets`.
Codegen should emit helpers for allocating and writing descriptor sets independently of pipeline
creation.

---

## Bindless Textures

**Descriptor indexing / bindless texture array.**
Requires `VK_EXT_descriptor_indexing` (core in Vulkan 1.2). Bind all unique images once into a
large `VkDescriptorSet` with `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT`. GLSL side becomes
`uniform sampler2D tex[]` with `#extension GL_EXT_nonuniform_qualifier`. Index via push constant
or vertex attribute. Eliminates per-draw descriptor set switches; all texture variation is just
an integer. Reflector needs to emit the binding with `descriptorCount = MAX_TEXTURES` and the
nonuniform qualifier on array accesses.

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
