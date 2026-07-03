#pragma once

#include "tuke_engine.h"

#include <stdio.h>

struct STBImage {
  int width;
  int height;
  int n_channels;
  unsigned char *data;
};

const char *read_file(const char *path, unsigned long *size);
STBImage load_texture(const char *path, bool flip_vertically);
void free_stb_image(STBImage *handle);

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

static inline u32 next_pow2(u32 x) {
#if __has_builtin(__builtin_clz)
  return x <= 1 ? 1 : 1u << (32 - __builtin_clz(x - 1));
#else
  // Don't understand this that well. Why only >> by the powers of 2?
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
#endif
}
