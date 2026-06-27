#pragma once

// Casey explaining the GJK algorithm
// https://www.youtube.com/watch?v=Qupqu1xe7Io
//
// Decent Minkowski blog
// https://www.spaderthomas.com/minkowski/

#include "linalg.h"
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
  Vec3 normal;
  bool did_collide;
  bool was_overlapping;
  f32 t;
  f32 penetration_depth;
};

static inline f32 evaluate_damped_harmonic_oscillator(DampedHarmonicOscillator dho, f32 t) {
  return dho.amplitude * expf(-dho.decay_constant * t) * cosf(dho.omega * t + dho.phase);
}

static inline bool interval_contains(f32 x, f32 min, f32 max) { return max >= x && x >= min; }

static inline Vec2 random_unit_vec2(RNG *rng) {
  f32 theta = 2 * PI * random_f32_xoroshiro128_plus(rng);
  f32 x = cosf(theta);
  f32 y = sinf(theta);
  return vec2(x, y);
}

static inline f32 cosine_vec3(Vec3 v, Vec3 w) {
  f32 dot = dot_v3(v, w);
  f32 norm_v_squared = dot_v3(v, v);
  f32 norm_w_squared = dot_v3(w, w);
  f32 denom = sqrt(norm_v_squared * norm_w_squared);
  return dot / denom;
}

// project v onto w
static inline Vec3 project_vec3(Vec3 v, Vec3 w) {
  f32 s = dot_v3(v, w) / dot_v3(w, w);
  return scale_v3(w, s);
}

// reflect v across w
static inline Vec3 reflect_vec3(Vec3 v, Vec3 w) {
  Vec3 projection = project_vec3(v, w);
  Vec3 perpendicular_projection = sub_v3(v, projection);
  Vec3 scaled = scale_v3(perpendicular_projection, 2.0f);
  return sub_v3(v, scaled);
}

// Functions
Vec3 random_unit_vec3(RNG *rng);

// Size in AABB collisions are full sizes. Positions are the center of the AABBs.
// From center to edges requires the use of half sizes in implementations.
bool aabb_collision(Vec3 pos1, Vec3 size1, Vec3 pos2, Vec3 size2);
bool aabb_collision_v2(Vec2 pos1, Vec2 size1, Vec2 pos2, Vec2 size2);
SweptAABBCollisionCheck swept_aabb_collision(f32 dt, Vec3 pos1, Vec3 size1, Vec3 v1, Vec3 pos2, Vec3 size2, Vec3 v2);

struct ScreenShake {
  DampedHarmonicOscillator x_oscillator;
  DampedHarmonicOscillator y_oscillator;
  bool active;
  f32 time_elapsed;
  f32 cutoff_duration; // seconds
};
