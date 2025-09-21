#include "filesystem_utils.h"
#include "reflector.h"

#include <stdio.h>
#include <string.h>

int main() {
  SubdirectoryList subdirectory_list;
  memset(&subdirectory_list, 0, sizeof(subdirectory_list));

  walk_dirs("shaders", &subdirectory_list);

  ShaderToCompileList shader_to_compile_list;
  memset(&shader_to_compile_list, 0, sizeof(shader_to_compile_list));
  collect_shaders_to_compile(&subdirectory_list, &shader_to_compile_list);
  free_shader_to_compile_list(&shader_to_compile_list);

  return 0;
}
