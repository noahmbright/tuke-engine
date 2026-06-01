# Reflector TODO

## Descriptor Generation

**Startup descriptor set init loop.**
Generate `static VkDescriptorSet descriptor_sets[NUM_DESCRIPTOR_SET_LAYOUTS]` and pointer tables
mapping enum index → binding array, binding count, write template array, write template count.
Startup loop allocates all sets and pre-wires `dstSet` into write templates so call sites never
touch it again.

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

**Pair vert+frag by name, emit a single program handle.**
`SHADER_HANDLE_FOO_VERT` + `SHADER_HANDLE_FOO_FRAG` → `SHADER_PROGRAM_FOO`. Covers both
split-file pairs (matched by stem) and combined files. Name uniqueness must be enforced — a
`.combined.in` and a `.vert.in` with the same stem is a hard error. Eliminates the two-handle
pipeline creation call and enables reflector-level validation that stages are compatible.
Per-program: which `DescriptorSetLayoutIDs` it uses, which `VertexLayoutID`, both shader handles.

---

## Further Learning Resources

- *Computer Architecture: A Quantitative Approach* (Hennessy/Patterson) — GPU architecture chapters
- NVIDIA Turing/Ampere/Ada architecture whitepapers (free PDFs) — actual silicon internals
- *GPU Gems* series (free online) — GPU algorithm patterns
- *Programming Massively Parallel Processors* (Kirk/Hwu) — standard CUDA text, first half is GPU architecture and API-agnostic
- Vulkan compute shaders + subgroup operations (`subgroupAdd`, `subgroupBallot`) — warp-level hardware concepts without CUDA
- Xcode Metal debugger/profiler — primary GPU profiling tool on M1

---

## Minor / Deferred

- Push constant parsing is a stub (`parse_push_constant_directive` does nothing).
- Add `const char *source_path` to `ShaderSpec` for hot reload.
- Validate that matching bindings across shaders sharing a SET_LABEL have the same struct type
  name, not just the same shape.
