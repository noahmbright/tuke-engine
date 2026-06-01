#pragma once

#include "parser.h"
#include <stdio.h>

typedef struct {
  const char *string;
  u32 length;
} TemplateStringReplacement;

typedef struct {
  const char *name; // malloc'd and owned by shader to compile
  bool vert, frag, comp;
} ShaderNameValid;

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, GraphicsBackend backend);

// Return value is whether codegen was successful or not.
bool codegen(const char *output_filepath, const ParsedShadersIR *parsed_shaders_ir);
