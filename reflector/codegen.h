#pragma once

#include "parser.h"
#include "reflection_data.h"
#include "subprocess.h"
#include <stdio.h>

typedef struct {
  const char *string;
  u32 length;
} TemplateStringReplacement;

// Accumulate data needed for an entire compute or graphics pipeline.
// Deduplicate across vertex/fragment stages.
typedef struct {
  const char *name; // malloc'd and owned by shader to compile
  bool vert, frag, comp;
} ShaderProgram;

typedef struct {
  const ParsedShader *parsed;
  SpirVBytesArray spirv_bytes_array;
  GLSLSource sources[NUM_GRAPHICS_BACKENDS];
} CompiledShader;

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, GraphicsBackend backend);

// Return value is whether codegen was successful or not.
bool codegen(const char *output_filepath, const ParsedShadersIR *parsed_shaders_ir);
