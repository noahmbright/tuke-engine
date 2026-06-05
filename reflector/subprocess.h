#pragma once

#include "reflector.h"

#include <unistd.h>

#define TEMPLATE_FILE_LENGTH 128

typedef struct {
  const u8 *bytes;
  u32 length;
} SpirVBytesArray;

typedef struct {
  const char *glsl_source_str;
  char glsl_path[TEMPLATE_FILE_LENGTH];
  char spirv_path[TEMPLATE_FILE_LENGTH];
  pid_t pid;
} CompileJob;

bool compile_to_spirv(const GLSLSource *sources, SpirVBytesArray *bytes_arrays, u32 num_sources);
