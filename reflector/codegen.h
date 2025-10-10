#pragma once

#include "parser.h"
#include "subprocess.h"
#include <stdio.h>

struct TemplateStringReplacement {
  const char *string;
  u32 length;
};

inline void print_bytes_array(FILE *destination, const SpirVBytesArray *bytes_array) {
  for (u32 i = 0; i < bytes_array->length; i += 4) {
    u32 word = (bytes_array->bytes[i + 0] << 0) | (bytes_array->bytes[i + 1] << 8) | (bytes_array->bytes[i + 2] << 16) |
               (bytes_array->bytes[i + 3] << 24);

    fprintf(destination, "0x%08X, ", word);
    u8 new_line = 8;
    if (((i / 4) + 1) % new_line == 0)
      fprintf(destination, "\n");
  }
  fprintf(destination, "\n");
}

inline void print_c_string_with_newlines(FILE *destination, const char *s) {
  for (; *s; s++) {
    if (*s == '\n') {
      fputc('"', destination);
      fputc(*s, destination);
      fputc('"', destination);
    } else {
      fputc(*s, destination);
    }
  }
}

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, GraphicsBackend backend);
void codegen(FILE *destination, const ParsedShadersIR *parsed_shaders_ir);
