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

struct SweptAABBCollisionCheck {
  glm::vec3 normal;
  bool did_collide;
  bool was_overlapping;
  f32 t;
  f32 penetration_depth;
};

static inline f32 evaluate_damped_harmonic_oscillator(DampedHarmonicOscillator dho, f32 t) {
  return dho.amplitude * expf(-dho.decay_constant * t) * cosf(dho.omega * t + dho.phase);
}

static inline bool interval_contains(f32 x, f32 min, f32 max) { return max >= x && x >= min; }

static inline glm::vec2 random_unit_vec2(RNG *rng) {
  f32 theta = 2 * PI * random_f32_xoroshiro128_plus(rng);
  f32 x = cosf(theta);
  f32 y = sinf(theta);
  return glm::vec2(x, y);
}

static inline f32 cosine_vec3(glm::vec3 v, glm::vec3 w) {
  f32 dot = glm::dot(v, w);
  f32 norm_v_squared = glm::dot(v, v);
  f32 norm_w_squared = glm::dot(w, w);
  f32 denom = sqrt(norm_v_squared * norm_w_squared);
  return dot / denom;
}

// project v onto w
static inline glm::vec3 project_vec3(glm::vec3 v, glm::vec3 w) { return glm::dot(v, w) / glm::dot(w, w) * w; }

// reflect v across w
static inline glm::vec3 reflect_vec3(glm::vec3 v, glm::vec3 w) {
  glm::vec3 projection = project_vec3(v, w);
  glm::vec3 perpendicular_projection = v - projection;
  return v - 2.0f * perpendicular_projection;
}

// functions
glm::vec3 random_unit_vec3(RNG *rng);

// size in AABB collisions are full sizes, so half sizes have to be used
// in implementations
bool aabb_collision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2, glm::vec3 size2);
SweptAABBCollisionCheck swept_aabb_collision(f32 dt, glm::vec3 pos1, glm::vec3 size1, glm::vec3 v1, glm::vec3 pos2,
                                             glm::vec3 size2, glm::vec3 v2);

struct ScreenShake {
  DampedHarmonicOscillator x_oscillator;
  DampedHarmonicOscillator y_oscillator;
  bool active;
  f32 time_elapsed;
  f32 cutoff_duration; // seconds
};
