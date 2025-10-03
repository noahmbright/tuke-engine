#pragma once

#include "reflector.h"

#define MAX_NUM_SUBDIRECTORIES 32
#define MAX_NUM_SHADERS 128
#define SUBDIRECTORY_PATH_BUFFER_LENGTH 512
#define FULL_PATH_BUFFER_LENGTH 4096

struct SubdirectoryList {
  u32 num_subdirectories;
  char subdirectories[MAX_NUM_SUBDIRECTORIES][SUBDIRECTORY_PATH_BUFFER_LENGTH];
};

// ShaderToCompile is the owner of source, responsible for freeing
struct ShaderToCompile {
  ShaderStage stage;
  const char *source;
  u64 source_length;
  const char *name;
  u32 name_length;
};

struct ShaderToCompileList {
  u32 num_shaders;
  ShaderToCompile shaders[MAX_NUM_SHADERS];
};

void free_shader_to_compile_list(ShaderToCompileList *shader_to_compile_list);
void push_subdirectory(SubdirectoryList *subdirectory_list, const char *s);
void walk_dirs(const char *path, SubdirectoryList *subdirectory_list);
ShaderToCompileList collect_shaders_to_compile(const SubdirectoryList *subdirectory_list);
