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

bool aabb_collision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2,
                    glm::vec3 size2) {

  f32 x1_min = pos1.x - size1.x * 0.5f;
  f32 x1_max = pos1.x + size1.x * 0.5f;
  f32 y1_min = pos1.y - size1.y * 0.5f;
  f32 y1_max = pos1.y + size1.y * 0.5f;
  f32 z1_min = pos1.z - size1.z * 0.5f;
  f32 z1_max = pos1.z + size1.z * 0.5f;

  f32 x2_min = pos2.x - size2.x * 0.5f;
  f32 x2_max = pos2.x + size2.x * 0.5f;
  f32 y2_min = pos2.y - size2.y * 0.5f;
  f32 y2_max = pos2.y + size2.y * 0.5f;
  f32 z2_min = pos2.z - size2.z * 0.5f;
  f32 z2_max = pos2.z + size2.z * 0.5f;

  bool x_overlaps = (x1_min < x2_max) && (x2_min < x1_max);
  bool y_overlaps = (y1_min < y2_max) && (y2_min < y1_max);
  bool z_overlaps = (z1_min < z2_max) && (z2_min < z1_max);

  return x_overlaps && y_overlaps && z_overlaps;
}
