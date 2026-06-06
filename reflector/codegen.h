#pragma once

#include "parser.h"
#include "reflector.h"

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

typedef struct {
  const char *string;
  u32 length;
} TemplateStringReplacement;

typedef struct {
  const ParsedShader *parsed[MAX_NUM_SHADERS];
  SpirVBytesArray spirv_bytes_arrays[MAX_NUM_SHADERS];
  GLSLSource gl_sources[MAX_NUM_SHADERS];
  GLSLSource vk_sources[MAX_NUM_SHADERS];
} CompiledShaders;

// Accumulate data needed for an entire compute or graphics pipeline.
// Deduplicate across vertex/fragment stages.
typedef struct {
  const char *name; // malloc'd and owned by shader to compile
  const ParsedShader *parsed_vert;
  const ParsedShader *parsed_frag;
  const ParsedShader *parsed_comp;
} ShaderProgram;

// Return value is whether codegen was successful or not.
bool codegen(const char *output_filepath, const ParsedShadersIR *parsed_shaders_ir);
