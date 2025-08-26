#pragma once

#include "statistics.h"
#include "tuke_engine.h"

static inline bool interval_contains(f32 x, f32 min, f32 max) {
  return max >= x && x >= min;
}

static inline glm::vec2 random_unit_vec2(RNG *rng) {
  f32 theta = 2 * PI * random_f32_xoroshiro128_plus(rng);
  f32 x = cosf(theta);
  f32 y = sinf(theta);
  return glm::vec2(x, y);
}

glm::vec3 random_unit_vec3(RNG *rng);

bool aabb_collision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2,
                    glm::vec3 size2);
