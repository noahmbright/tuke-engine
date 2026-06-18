#pragma once

#include "tuke_engine.h"
#include "utils.h"

#define CAMERA_NEAR_Z (0.1f)
#define CAMERA_FAR_Z (100.f)

// TODO do I need this?
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"
#include "glm/trigonometric.hpp"

enum CameraType { CAMERA_TYPE_2D, CAMERA_TYPE_3D, CAMERA_TYPE_FPS };

struct Camera {
  CameraType type;

  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 up;
  glm::vec3 right;

  f32 pitch, yaw;
  f32 mouse_sensitivity = 1e-3;
  f32 fovy = 45.0f;

  f32 last_mouse_x, last_mouse_y;
  bool has_moused_yet;
  bool y_needs_inverted;
};

struct CameraMatrices {
  glm::mat4 view;
  glm::mat4 projection;
};

// TODO kill default args
const glm::vec3 CAMERA_POSITION0 = glm::vec3(0.0, 0.0, 1.0);
const glm::vec3 CAMERA_DIRECTION0 = glm::vec3(0.0, 0.0, -1.0);
const glm::vec3 CAMERA_UP0 = glm::vec3(0.0, 1.0, 0.0);
const glm::vec3 CAMERA_RIGHT0 = glm::vec3(1.0, 0.0, 0.0);

Camera create_camera(
    CameraType type,
    glm::vec3 pos = CAMERA_POSITION0,
    glm::vec3 direction = CAMERA_DIRECTION0,
    glm::vec3 up = CAMERA_UP0,
    glm::vec3 right = CAMERA_RIGHT0
);

// xpos and y pos are the positions of the mouse queried from GLFW
// This function assumes that direction is always synced with pitch and yaw.
// If not, and the camera transitions to using this function, there will be a jump.
void process_mouse_input3d(Camera *camera, f64 xpos, f64 ypos);

inline void camera_move_2d(Camera *camera, const glm::vec2 movement_direction) {
  camera->position.x += movement_direction.x;
  camera->position.y += movement_direction.y;
}

inline void camera_move_3d(Camera *camera, const glm::vec2 movement_direction) {
  camera->position += movement_direction.y * camera->direction;
  camera->position += movement_direction.x * camera->right;
}

inline void move_camera(Camera *camera, const glm::vec2 movement_direction) {
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

inline glm::mat4 camera_look_at(const Camera *camera) {
  assert(glm::all(glm::isfinite(camera->direction)));
  assert(glm::length(camera->direction) > 0.0f);
  return glm::lookAt(camera->position, camera->position + camera->direction, camera->up);
}

inline glm::mat4 camera_perspective_projection(const Camera *camera, u32 window_width, u32 window_height) {
  f32 aspect_ratio = f32(window_width) / f32(window_height);
  f32 fov = glm::radians(camera->fovy);

  glm::mat4 proj = glm::perspective(fov, aspect_ratio, CAMERA_NEAR_Z, CAMERA_FAR_Z);

  proj[1][1] = camera->y_needs_inverted ? -proj[1][1] : proj[1][1];
  return proj;
}

inline CameraMatrices create_camera_matrices(const Camera *camera, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices;
  camera_matrices.view = camera_look_at(camera);
  camera_matrices.projection = camera_perspective_projection(camera, window_width, window_height);
  return camera_matrices;
}

inline CameraMatrices
camera_matrices_with_offset(const Camera *camera, glm::vec3 offset, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices;

  glm::vec3 pos = camera->position + offset;
  camera_matrices.view = glm::lookAt(pos, pos + camera->direction, camera->up);

  camera_matrices.projection = camera_perspective_projection(camera, window_width, window_height);
  return camera_matrices;
}

inline void log_camera(const Camera *camera) {
  printf("Camera position:  ");
  log_vec3(&camera->position);

  printf("Camera direction: ");
  log_vec3(&camera->direction);

  printf("Camera up:        ");
  log_vec3(&camera->up);
}
