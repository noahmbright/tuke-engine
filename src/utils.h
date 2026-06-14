#pragma once

#include "tuke_engine.h"

#include <glm/glm.hpp>
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

inline void log_mat4(const glm::mat4 &m) {
  printf("logging mat4...\n-----\n");
  for (u32 row = 0; row < 4; ++row) {
    for (u32 col = 0; col < 4; ++col) {
      printf("%10.4f ", m[col][row]);
    }
    printf("\n");
  }
}

inline bool matrix_has_nan(const glm::mat4 &mat) {
  return glm::any(glm::isnan(mat[0])) || glm::any(glm::isnan(mat[1])) || glm::any(glm::isnan(mat[2])) ||
         glm::any(glm::isnan(mat[3]));
}

inline f32 sign_f32(f32 x) { return x >= 0.0f ? 1.0f : -1.0f; }
static inline void log_vec3(const glm::vec3 *v) {
  const char *fmt = "%+9.3f";
  printf(fmt, v->x);
  printf(" ");
  printf(fmt, v->y);
  printf(" ");
  printf(fmt, v->z);
  printf("\n");
}

static inline void log_vec2(const glm::vec2 *v) {
  const char *fmt = "%+9.3f";
  printf(fmt, v->x);
  printf(" ");
  printf(fmt, v->y);
  printf("\n");
}
