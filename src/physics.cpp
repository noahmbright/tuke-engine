#include "physics.h"
#include "tuke_engine.h"

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

static inline bool swept_aabb_get_entry_exit_times(f32 v_rel, f32 min1,
                                                   f32 max1, f32 min2, f32 max2,
                                                   f32 *t_entry_out,
                                                   f32 *t_exit_out) {

  // compute distances for entry/exit
  f32 d_entry, d_exit;
  if (v_rel > 0.0f) {
    d_entry = min1 - max2;
    d_exit = max1 - min2;
  } else {
    d_entry = min2 - max1;
    d_exit = max2 - min1;
  }

  // compute times
  if (fabs(v_rel) < EPSILON) {
    if (min1 > max2 || min2 > max1) {
      return false;
    } else {
      *t_entry_out = -INFINITY_F32;
      *t_exit_out = INFINITY_F32;
    }
  } else {
    *t_entry_out = d_entry / v_rel;
    *t_exit_out = d_exit / v_rel;
  }

  return true;
}

// https://www.gamedev.net/tutorials/programming/general-and-gameplay-programming/swept-aabb-collision-detection-and-response-r3084/
SweptAABBCollisionCheck swept_aabb_collision(f32 dt, glm::vec3 pos1,
                                             glm::vec3 size1, glm::vec3 v1,
                                             glm::vec3 pos2, glm::vec3 size2,
                                             glm::vec3 v2) {

  SweptAABBCollisionCheck swept_aabb_collision_check;
  swept_aabb_collision_check.t = 0.0f;
  swept_aabb_collision_check.did_collide = false;
  swept_aabb_collision_check.was_overlapping = false;
  swept_aabb_collision_check.penetration_depth = 0.0f;
  swept_aabb_collision_check.normal = glm::vec3(0.0f);

  // we are in the reference frame of box 1
  // we are checking if box 2 is going to slam into us
  glm::vec3 half_size1 = 0.5f * size1;
  glm::vec3 half_size2 = 0.5f * size2;

  glm::vec3 maxes1 = pos1 + half_size1;
  glm::vec3 mins1 = pos1 - half_size1;

  glm::vec3 maxes2 = pos2 + half_size2;
  glm::vec3 mins2 = pos2 - half_size2;

  if (aabb_collision(pos1, size1, pos2, size2)) {
    swept_aabb_collision_check.did_collide = true;
    swept_aabb_collision_check.was_overlapping = true;

    f32 overlap_x = fmin(maxes1.x, maxes2.x) - fmax(mins1.x, mins2.x);
    f32 overlap_y = fmin(maxes1.y, maxes2.y) - fmax(mins1.y, mins2.y);
    f32 overlap_z = fmin(maxes1.z, maxes2.z) - fmax(mins1.z, mins2.z);

    // pick axis of min overlap
    // normal is from perspective of object 1 - if object 2 is to our left,
    // i.e., pos2.x < pos1.x, the normal is -1.0 \hat x
    if (overlap_x < overlap_y && overlap_x < overlap_z) {
      swept_aabb_collision_check.normal = {(pos1.x < pos2.x ? 1.0f : -1.0f), 0,
                                           0};
      swept_aabb_collision_check.penetration_depth = overlap_x;
    } else if (overlap_y < overlap_z) {
      swept_aabb_collision_check.normal = {0, (pos1.y < pos2.y ? 1.0f : -1.0f),
                                           0};
      swept_aabb_collision_check.penetration_depth = overlap_y;
    } else {
      swept_aabb_collision_check.normal = {0, 0,
                                           (pos1.z < pos2.z ? 1.0f : -1.0f)};
      swept_aabb_collision_check.penetration_depth = overlap_z;
    }

    return swept_aabb_collision_check;
  }

  // v_rel is the apparent velocity of box 2 in box 1's reference frame
  glm::vec3 v_rel = v2 - v1;

  // compute times
  f32 t_x_entry, t_x_exit;
  if (!swept_aabb_get_entry_exit_times(v_rel.x, mins1.x, maxes1.x, mins2.x,
                                       maxes2.x, &t_x_entry, &t_x_exit)) {
    return swept_aabb_collision_check;
  }

  f32 t_y_entry, t_y_exit;
  if (!swept_aabb_get_entry_exit_times(v_rel.y, mins1.y, maxes1.y, mins2.y,
                                       maxes2.y, &t_y_entry, &t_y_exit)) {
    return swept_aabb_collision_check;
  }

  f32 t_z_entry, t_z_exit;
  if (!swept_aabb_get_entry_exit_times(v_rel.z, mins1.z, maxes1.z, mins2.z,
                                       maxes2.z, &t_z_entry, &t_z_exit)) {
    return swept_aabb_collision_check;
  }

  // get the max of all entry times
  f32 entry_time = (t_x_entry > t_y_entry) ? t_x_entry : t_y_entry;
  entry_time = (entry_time > t_z_entry) ? entry_time : t_z_entry;

  // get the min of all exit times
  f32 exit_time = (t_x_exit < t_y_exit) ? t_x_exit : t_y_exit;
  exit_time = (exit_time < t_z_exit) ? exit_time : t_z_exit;

  if (entry_time > dt || entry_time < 0 || entry_time > exit_time) {
    return swept_aabb_collision_check;
  }
  swept_aabb_collision_check.t = entry_time;

  // normal from the perspective of object 1
  // if v_rel.x > 0, it looks like object 2 is moving to the right
  // so it will hit the left side of object 1, and we return -1.0 \hat x
  swept_aabb_collision_check.did_collide = true;
  if (entry_time == t_x_entry) {
    f32 nx = (v_rel.x > 0) ? -1.0f : 1.0f;
    swept_aabb_collision_check.normal.x = nx;
  } else if (entry_time == t_y_entry) {
    f32 ny = (v_rel.y > 0) ? -1.0f : 1.0f;
    swept_aabb_collision_check.normal.y = ny;
  } else if (entry_time == t_z_entry) {
    f32 nz = (v_rel.z > 0) ? -1.0f : 1.0f;
    swept_aabb_collision_check.normal.z = nz;
  }

  return swept_aabb_collision_check;
}
