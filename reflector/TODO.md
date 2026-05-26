# Reflector TODO

## Highest Priority: Per-App Shader Organization

**Move shaders into app subdirectories.**
Currently all shaders live under `shaders/<subdirectory>/`. Move them to `app/<appname>/shaders/`.
Audit shows `shaders/common/` is mostly bringup — the only shader genuinely shared across two apps
is `fullscreen_quad` (vulkan_test + top_down). Duplication of that one shader is cheaper than the
namespace problems the current layout causes.

**Per-app reflector invocation → per-app generated header.**
Run the reflector once per app, fed that app's `shaders/` dir. Emit `app/<appname>/gen/shaders.h`
instead of the current monolith `gen/c_reflector_bringup.h`. `BUFFER_LABEL` namespace becomes
app-local, eliminating cross-app label conflicts.

---

## Descriptor Generation (High Priority)

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

## Parser Code Quality

**Replace advance+check+error with `expect_token` helper.**
Every directive parser repeats:
```c
current_token = parser_advance_and_get_next_token(parser);
if (current_token.type != TOKEN_TYPE_X) {
    report_parser_error(parser, current_token.start, RECOVERY, "Expected X after Y, got %s",
                        token_type_to_string[current_token.type]);
    return ...;
}
```
An `expect_token(parser, type, recovery_type, fmt, ...)` helper that advances, checks, and
errors would cut the parser roughly in half.

**StringView for C string handling.**
`strncmp` + length pairs appear throughout parser and IR code. A small `StringView` struct with
an equality helper would reduce noise and eliminate the risk of length/pointer mismatches.

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

## Minor / Deferred

- Push constant parsing is a stub (`parse_push_constant_directive` does nothing).
- `init_generated_shader_vk_modules` should not be the caller's job — move into context init
  once the per-app header split is done.
- Add `const char *source_path` to `ShaderSpec` for hot reload.
- `SHADER_HANDLE_` prefix and `_VERT`/`_FRAG` suffixes are noise at callsites — drop with
  program handle introduction.
