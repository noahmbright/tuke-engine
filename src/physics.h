#pragma once

#include "statistics.h"
#include "tuke_engine.h"
#include <math.h>

struct DampedHarmonicOscillator {
  f32 omega; // angular frequency
  f32 phase;
  f32 amplitude;
  f32 decay_constant;
};

static inline f32
evaluate_damped_harmonic_oscillator(DampedHarmonicOscillator dho, f32 t) {
  return dho.amplitude * expf(-dho.decay_constant * t) *
         cosf(dho.omega * t + dho.phase);
}

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
