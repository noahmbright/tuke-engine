#pragma once

#include "glm/glm.hpp"
#include <stdint.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

const f32 PI = 3.14159265358979323846264338327950288;

static inline void log_vec3(const glm::vec3 *v) {
  const char *fmt = "%+9.3f";
  printf(fmt, v->x);
  printf(" ");
  printf(fmt, v->y);
  printf(" ");
  printf(fmt, v->z);
  printf("\n");
}
