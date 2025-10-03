#pragma once

#include "parser.h"
#include "subprocess.h"
#include <stdio.h>

struct TemplateStringReplacement {
  const char *string;
  u32 length;
};

inline void print_bytes_array(FILE *destination, const SpirVBytesArray *bytes_array) {
  for (u32 i = 0; i < bytes_array->length; i++) {
    fprintf(destination, "%02X ", bytes_array->bytes[i]);
    if ((i + 1) % 16 == 0)
      fprintf(destination, "\n");
  }

  printf("\n");
}

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, GraphicsBackend backend);
void codegen(FILE *destination, const ParsedShadersIR *parsed_shaders_ir);
