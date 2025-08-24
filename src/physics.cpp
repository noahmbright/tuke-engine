#include "physics.h"

glm::vec3 random_unit_vec3(RNG *rng) {
  f32 u = random_f32_xoroshiro128_plus(rng);
  f32 v = random_f32_xoroshiro128_plus(rng); // [0,1)

  assert(interval_contains(u, 0.0f, 1.0f));
  assert(interval_contains(v, 0.0f, 1.0f));

  f32 theta = 2.0f * PI * u;
  f32 z_val = 2.0f * v - 1.0f;
  f32 r = sqrtf(1.0f - z_val * z_val);

  f32 x = r * cosf(theta);
  f32 y = r * sinf(theta);
  f32 z = z_val;
  return glm::vec3(x, y, z);
}
