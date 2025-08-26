#pragma once

#include "tuke_engine.h"

// use SplitMix64 and xoroshiro128+, requiring up to 128 bits
struct RNG {
  u64 state;
  u64 state1;
};

// rotate left
static inline u64 rotl(const u64 x, i32 k) {
  return (x << k) | (x >> (64 - k));
}

static inline u64 random_u64_splitmix64(RNG *rng) {
  rng->state += 0x9e3779b97f4a7c15;
  u64 z = rng->state;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

static inline RNG create_rng(u64 seed) {
  RNG rng;
  rng.state = seed;
  rng.state1 = random_u64_splitmix64(&rng);
  return rng;
}

// RNG shootout
// https://prng.di.unimi.it/
// https://prng.di.unimi.it/xoroshiro128plus.c
// also includes jumps by 2^64 and 2^96 next() calls, if you need to jump and
// make non overlapping sequences
static inline u64 random_u64_xoroshiro128plus(RNG *rng) {

  // parameters of the algorithm
  const u32 a = 24;
  const u32 b = 16;
  const u32 c = 37;

  const u64 s0 = rng->state;
  u64 s1 = rng->state1;
  const u64 result = s0 + s1;

  s1 ^= s0;
  rng->state = rotl(s0, a) ^ s1 ^ (s1 << b);
  rng->state1 = rotl(s1, c);

  return result;
}

static inline f32 random_f32_xoroshiro128_plus(RNG *rng) {
  u64 rand = random_u64_xoroshiro128plus(rng);
  u32 n = (u32)(rand >> 40);
  const f32 x = 1.0f / 16777216.0f;
  return n * x;
}

static inline f32 random_f32_in_range_xoroshiro128_plus(RNG *rng, f32 min,
                                                        f32 max) {
  f32 rand = random_f32_xoroshiro128_plus(rng);
  return (max - min) * rand + min;
}
