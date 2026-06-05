#pragma once

#include "parser.h"
#include "reflection_data.h"
#include "subprocess.h"
#include <stdio.h>

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

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, Backend backend);

// Return value is whether codegen was successful or not.
bool codegen(const char *output_filepath, const ParsedShadersIR *parsed_shaders_ir);
