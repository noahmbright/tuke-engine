#pragma once

#include "tuke_engine.h"

struct STBHandle {
  int width, height, n_channels;
  unsigned char *data;
};

const char *read_file(const char *path, unsigned long *size = nullptr);
STBHandle load_texture(const char *path);
STBHandle load_texture_metadata(const char *path);
void free_stb_handle(STBHandle *handle);
u32 load_texture_opengl(const char *path);

inline u32 clamp_u32(u32 x, u32 min, u32 max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

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
