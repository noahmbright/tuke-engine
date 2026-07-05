#pragma once

#include "linalg.h"
#include "tuke_engine.h"

#include <assert.h>
#include <stdio.h>

#define CAMERA_NEAR_Z (0.1f)
#define CAMERA_FAR_Z (100.f)

enum CameraType { CAMERA_TYPE_2D, CAMERA_TYPE_3D, CAMERA_TYPE_FPS };

struct Camera {
  CameraType type;

  Vec3 position;
  Vec3 direction;
  Vec3 up;
  Vec3 right;

  f32 pitch, yaw;
  f32 mouse_sensitivity = 1e-3;
  f32 fovy = 45.0f; // Degrees. TODO Kill default.

  f32 last_mouse_x, last_mouse_y;
  bool has_moused_yet;
  bool y_needs_inverted;
};

struct CameraMatrices {
  Mat4 view;
  Mat4 projection;
};

// TODO kill default args
const Vec3 CAMERA_POSITION0 = Vec3(0.0, 0.0, 1.0);
const Vec3 CAMERA_DIRECTION0 = Vec3(0.0, 0.0, -1.0);
const Vec3 CAMERA_UP0 = Vec3(0.0, 1.0, 0.0);
const Vec3 CAMERA_RIGHT0 = Vec3(1.0, 0.0, 0.0);

Camera create_camera(
    CameraType type,
    Vec3 pos = CAMERA_POSITION0,
    Vec3 direction = CAMERA_DIRECTION0,
    Vec3 up = CAMERA_UP0,
    Vec3 right = CAMERA_RIGHT0
);

// xpos and y pos are the positions of the mouse queried from GLFW
// This function assumes that direction is always synced with pitch and yaw.
// If not, and the camera transitions to using this function, there will be a jump.
void process_mouse_input3d(Camera *camera, f64 xpos, f64 ypos);

inline void camera_move_2d(Camera *camera, const Vec2 movement_direction) {
  camera->position.x += movement_direction.x;
  camera->position.y += movement_direction.y;
}

inline void camera_move_3d(Camera *camera, const Vec2 movement_direction) {
  inc_v3(&camera->position, scale_v3(camera->direction, movement_direction.y));
  inc_v3(&camera->position, scale_v3(camera->right, movement_direction.x));
}

inline void move_camera(Camera *camera, const Vec2 movement_direction) {
  switch (camera->type) {
  case CAMERA_TYPE_2D:
    camera_move_2d(camera, movement_direction);
    return;
  case CAMERA_TYPE_FPS: // TODO FPS
  case CAMERA_TYPE_3D:
    camera_move_3d(camera, movement_direction);
    break;
  default:
    assert(false);
  }
}

inline Mat4 camera_perspective_projection(const Camera *camera, f32 aspect) {
  f32 fov = camera->fovy * PI / 180.0f;
  Mat4 proj = perspective_proj(aspect, fov, CAMERA_NEAR_Z, CAMERA_FAR_Z);
  proj.arr[1][1] = camera->y_needs_inverted ? -proj.arr[1][1] : proj.arr[1][1];
  return proj;
}

inline CameraMatrices create_camera_matrices(const Camera *camera, f32 aspect) {
  CameraMatrices camera_matrices;
  assert(isfinite_v3(camera->direction));
  assert(len2_v3(camera->direction) > EPSILON);

  camera_matrices.view = make_camera_from_world(camera->position, camera->direction, camera->up);
  camera_matrices.projection = camera_perspective_projection(camera, aspect);
  return camera_matrices;
}

inline Mat4 make_camera_vp(const Camera *camera, f32 aspect) {
  assert(isfinite_v3(camera->direction));
  assert(len2_v3(camera->direction) > EPSILON);

  Mat4 view = make_camera_from_world(camera->position, camera->direction, camera->up);
  Mat4 proj = camera_perspective_projection(camera, aspect);
  Mat4 out;
  mult_m4(&proj, &view, &out);
  return out;
}

inline CameraMatrices camera_matrices_offset(const Camera *camera, Vec3 offset, f32 aspect) {
  CameraMatrices camera_matrices;

  Vec3 pos = add_v3(camera->position, offset);
  camera_matrices.view = make_camera_from_world(pos, camera->direction, camera->up);
  camera_matrices.projection = camera_perspective_projection(camera, aspect);
  return camera_matrices;
}

inline void log_camera(const Camera *camera) {
  printf("Camera position:  ");
  log_v3(camera->position);

  printf("Camera direction: ");
  log_v3(camera->direction);

  printf("Camera up:        ");
  log_v3(camera->up);
}
