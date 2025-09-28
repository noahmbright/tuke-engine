#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"
#include "reflector.h"
#include "subprocess.h"

#include <stdio.h>
#include <string.h>

int main() {
  SubdirectoryList subdirectory_list;
  memset(&subdirectory_list, 0, sizeof(subdirectory_list));

  walk_dirs("shaders", &subdirectory_list);

  ShaderToCompileList shader_to_compile_list;
  memset(&shader_to_compile_list, 0, sizeof(shader_to_compile_list));

  collect_shaders_to_compile(&subdirectory_list, &shader_to_compile_list);
  printf("Collected %d shaders to compile\n", shader_to_compile_list.num_shaders);
  ShaderToCompile shader_to_compile = shader_to_compile_list.shaders[0];
  printf("Shader to compile source:\n%s\n", shader_to_compile.source);

  TemplateStringSlice template_string_slices[MAX_NUM_STRING_SLICES];
  u32 num_string_slices = 0;
  parse_shader(shader_to_compile_list.shaders[0], template_string_slices, &num_string_slices);

  for (u32 i = 0; i < num_string_slices; i++) {
    TemplateStringSlice slice = template_string_slices[i];
    const char *start = slice.start;
    const char *end = slice.end;
    i32 length = (i32)(end - start);
    if (slice.type == DIRECTIVE_TYPE_GLSL_SOURCE) {
      printf("GLSL source slice of length %d:\n%.*s\n", length, (int)(end - start), start);
    } else {
      printf("String slice of length %d:\n%.*s\n", length, (int)(end - start), start);
    }
  }

  GLSLSource glsl_source = replace_string_slices(template_string_slices, num_string_slices, GRAPHICS_BACKEND_VULKAN);
  printf("\nCompiled source:\n");
  printf("%s\n", glsl_source.string);

#if 0
  SpirVBytesArray bytes_array = compile_vulkan_source_to_glsl(glsl_source, shader_to_compile_list.shaders[0].stage);
  printf("\n Spirv: \n");
  for (u32 i = 0; i < bytes_array.length; i++) {
    printf("%02X ", bytes_array.bytes[i]);
    if ((i + 1) % 16 == 0)
      printf("\n"); // optional: newline every 16 bytes
  }
  printf("\n");
#endif

  free_shader_to_compile_list(&shader_to_compile_list);
  return 0;
}
