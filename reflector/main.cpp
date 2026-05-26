#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {

  // Parse args
  bool force_shaders = false;
  const char *parsed_input_dir_path = NULL;
  for (int i = 1; i < argc; i++) {
    bool force =
        (strcmp(argv[i], "--force-shaders") == 0) || (strcmp(argv[i], "-f") == 0) || (strcmp(argv[i], "--force") == 0);
    if (force) {
      force_shaders = true;
    } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
      parsed_input_dir_path = argv[++i];
    }
  }

  const char *input_dir_path_raw;
  if (parsed_input_dir_path != NULL) {
    input_dir_path_raw = parsed_input_dir_path;
  } else {
    printf("Input path not provided. Defaulting to searching shaders/ in cwd...\n");
    input_dir_path_raw = "shaders";
  }

  char input_dir_path[FULL_PATH_BUFFER_LENGTH] = {0};
  validate_in_path(input_dir_path_raw, input_dir_path);
  printf("Looking for shaders in %s\n", input_dir_path);

  // Derive output path
  char output_path[FULL_PATH_BUFFER_LENGTH];
  int output_path_n;
  if (strlen(input_dir_path) == strlen("shaders")) {
    output_path_n = snprintf(output_path, sizeof(output_path), "gen/shaders.h");
  } else {
    size_t input_len = strlen(input_dir_path);
    size_t parent_end = input_len - strlen("shaders") - 1; // index of '/' before "shaders"
    size_t parent_start = parent_end;
    while (parent_start > 0 && input_dir_path[parent_start - 1] != '/') {
      parent_start--;
    }
    int len = (int)(parent_end - parent_start);
    output_path_n =
        snprintf(output_path, sizeof(output_path), "gen/%.*s/shaders.h", len, input_dir_path + parent_start);
  }
  if (output_path_n < 0 || output_path_n >= FULL_PATH_BUFFER_LENGTH) {
    fprintf(stderr, "output path too long\n");
    return 1;
  }
  printf("Output: %s\n", output_path);

  // Main flow:
  // 1) Walk dirs
  // 2) Collect shaders
  // 3) Parse shaders and populate symbol tables
  // 4) Codegen

  // 1) Walk dirs
  SubdirectoryList subdirectory_list;
  memset(&subdirectory_list, 0, sizeof(subdirectory_list));
  walk_dirs(input_dir_path, &subdirectory_list);

  // 2) Collect shaders
  ShaderToCompileList shader_to_compile_list = collect_shaders_to_compile(&subdirectory_list, input_dir_path, output_path);
  if (shader_to_compile_list.num_shaders == 0) {
    printf("Got no shaders to compile, not recompiling.\n");
    return 0;
  }

  if (!force_shaders && shader_to_compile_list.needs_recompiled == false) {
    printf("Determined that we should not recompile, reflector exiting.\n");
    return 0;
  }

  // 3) Parse shaders
  ParsedShadersIR parsed_shaders_ir = parse_all_shaders_and_populate_global_tables(&shader_to_compile_list);
  if (!parsed_shaders_ir.parsing_successful) {
    printf("Parsing error in shaders, reflector exiting.\n");
    return 1;
  }

  // 4) Codegen
  bool codegen_successful = codegen(output_path, &parsed_shaders_ir);
  if (!codegen_successful) {
    printf("Codegen error in shaders, reflector exiting.\n");
    return 1;
  }

  // 5) cleanup
  free_shader_to_compile_list(&shader_to_compile_list);
  printf("Successfully compiled shaders.\n\n");
  return 0;
}
