# Reflector TODO

## Next Up: Descriptor Generation

**`generate_vulkan_descriptor_set_layout_binding_lists` is a stub.**
The function in `codegen.cpp` has an empty loop body. This is where per-program descriptor setup
helpers should be emitted. The IR already has every `(set, binding, type, stage, struct)` tuple.

**Track label → (binding, struct) in the IR.**
The parser records `buffer_label_name` per `SET_BINDING` but does not globally associate labels
with their bindings and struct types (see TODO comment at parser.cpp:1466). Add a per-label table
to `ParsedShadersIR` that accumulates `(binding, struct*)` pairs across all shaders sharing that
label. This is the prerequisite for:
- Generating per-label aggregate C structs (replacing `UniformBufferManager` + `push_uniform`)
- Generating per-program descriptor set setup functions

**Per-program descriptor helpers.**
Once label→binding→struct is tracked, emit functions like:
```c
DescriptorSetHandle create_uniform_bringup_descriptors(
    VulkanContext *ctx, VkDescriptorPool pool, const UniformBuffer *ub
);
```
Eliminating the manual `add_uniform_buffer_descriptor_set` calls at every callsite. See
`app/vulkan_test/02_uniform_triangle.cpp` for the current pain.

**Per-label aggregate structs (replaces UniformBufferManager).**
Generate one C struct per label containing all bound uniform structs as fields, sorted by binding.
Buffer creation becomes `sizeof(LabelUniforms)`. Offsets come from `offsetof`. Alignment is
already handled correctly by the per-field `alignas` the reflector emits.

---

## Parser

**Error reporting is the main pain point.**
The advance+check+error pattern repeated ~20 times is verbose but functional. The specific issue
is error message quality. An `expect_token` helper would fix both if it ever becomes worth it.

---

## Codegen Code Quality

**Magic byte offsets in `replace_string_slices` are fragile.**
Set/binding numbers are patched into the replacement string by hardcoded byte indices:
`current_start[13]` (set), `current_start[26]` (binding). If the template string changes
by one character, output is silently corrupted. Replace with `snprintf` into a local buffer.

---

## Shader Program Concept

**Pair vert+frag by name, emit a single program handle.**
`SHADER_HANDLE_FOO_VERT` + `SHADER_HANDLE_FOO_FRAG` → `SHADER_PROGRAM_FOO`. Eliminates the
two-handle pipeline creation call and enables reflector-level validation that stages are compatible.
See `src/vulkan/TODO.md` for the target API.

**Single-file shader format.**
Allow vert and frag in one `.shader.in` file split by a `#stage vertex` / `#stage fragment`
marker. Additive — existing `.vert.in` / `.frag.in` pairs continue to work. See `src/vulkan/TODO.md`.

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
- `init_generated_shader_vk_modules` should not be the caller's job — move into context init
  once the per-app header split is done.
- Add `const char *source_path` to `ShaderSpec` for hot reload.
- `SHADER_HANDLE_` prefix and `_VERT`/`_FRAG` suffixes are noise at callsites — drop with
  program handle introduction.
