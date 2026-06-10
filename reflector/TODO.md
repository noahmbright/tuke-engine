# Reflector TODO

## Descriptor Generation

**Per-label aggregate structs (replaces UniformBufferManager).**
Generate one C struct per label containing all bound uniform structs as fields, sorted by binding.
Buffer creation becomes `sizeof(LabelUniforms)`. Offsets come from `offsetof`. Alignment is
already handled correctly by the per-field `alignas` the reflector emits.

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

## Further Learning Resources

- *Computer Architecture: A Quantitative Approach* (Hennessy/Patterson) — GPU architecture chapters
- NVIDIA Turing/Ampere/Ada architecture whitepapers (free PDFs) — actual silicon internals
- *GPU Gems* series (free online) — GPU algorithm patterns
- *Programming Massively Parallel Processors* (Kirk/Hwu) — standard CUDA text, first half is GPU architecture and API-agnostic
- Vulkan compute shaders + subgroup operations (`subgroupAdd`, `subgroupBallot`) — warp-level hardware concepts without CUDA
- Xcode Metal debugger/profiler — primary GPU profiling tool on M1

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

- Push constant parsing is a stub (`parse_push_constant_directive` does nothing).
- Add `const char *source_path` to `ShaderSpec` for hot reload.
