#pragma once

#include "tuke_engine.h"

#include <stdio.h>

struct STBHandle {
  int width;
  int height;
  int n_channels;
  unsigned char *data;
};

const char *read_file(const char *path, unsigned long *size);
STBHandle load_texture(const char *path);
STBHandle load_texture_metadata(const char *path);
void free_stb_handle(STBHandle *handle);

inline f32 clamp_f32(f32 x, f32 min, f32 max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }

  return x;
}

inline f32 sign_f32(f32 x) { return x >= 0.0f ? 1.0f : -1.0f; }
