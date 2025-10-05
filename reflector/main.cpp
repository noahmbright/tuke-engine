#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"
#include "reflector.h"
#include "subprocess.h"

#include <stdio.h>
#include <string.h>

int main() {
  // main flow:
  // 1) walk dirs
  // 2) collect shaders
  // 3) parse shaders and populate symbol tables
  // 4) codegen

  // 1) walk dirs
  SubdirectoryList subdirectory_list;
  memset(&subdirectory_list, 0, sizeof(subdirectory_list));
  walk_dirs("shaders", &subdirectory_list);

  // 2) collect shaders
  ShaderToCompileList shader_to_compile_list = collect_shaders_to_compile(&subdirectory_list);
  if (shader_to_compile_list.num_shaders == 0) {
    printf("Got no shaders to compile\n");
    exit(0);
  } else {
    // debug hack
    // shader_to_compile_list.num_shaders = 1;
  }

  printf("Collected %d shaders to compile\n", shader_to_compile_list.num_shaders);
  ShaderToCompile shader_to_compile = shader_to_compile_list.shaders[0];
  printf("Shader to compile name:\n%s\n", shader_to_compile.name);
  printf("Shader to compile source:\n%s\n", shader_to_compile.source);

  // 3) parse shaders
  // hack
  ParsedShadersIR parsed_shaders_ir = parse_all_shaders_and_populate_global_tables(&shader_to_compile_list);

  // 4) codegen
  FILE *output_file = fopen("c_reflector_bringup.h", "w");
  codegen(output_file, &parsed_shaders_ir);

  // (5) cleanup
  free_shader_to_compile_list(&shader_to_compile_list);
  return 0;
}
