#pragma once

#include "tuke_engine.h"

// TODO do I need this?
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"
#include "glm/trigonometric.hpp"

#define CAMERA_PERSPECTIVE_PROJECTION_NEAR_Z (0.1f)
#define CAMERA_PERSPECTIVE_PROJECTION_FAR_Z (100.f)

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

  void (*move_camera_function)(Camera *, const glm::vec3 &);

  f32 last_mouse_x, last_mouse_y;
  bool has_moused_yet;
  bool y_needs_inverted;
};

struct CameraMatrices {
  glm::mat4 view, projection;
};

Camera new_camera(CameraType type, const glm::vec3 &pos = {0.0, 0.0, 1.0},
                  const glm::vec3 &direction = {0.0, 0.0, -1.0}, const glm::vec3 &up = {0.0, 1.0, 0.0},
                  const glm::vec3 &right = {1.0, 0.0, 0.0});

inline void camera_move_2d(Camera *camera, const glm::vec3 &movement_direction) {
  camera->position += movement_direction;
}

inline void camera_move_3d(Camera *camera, const glm::vec3 &movement_direction) {
  camera->position += movement_direction.y * camera->direction;
  camera->position += movement_direction.x * camera->right;
}

inline void move_camera(Camera *camera, const glm::vec3 &movement_direction) {
  // move_camera_function will only ever be camera_move_2d or camera_move_3d
  camera->move_camera_function(camera, movement_direction);
}

inline glm::mat4 camera_look_at(const Camera *camera) {
  assert(glm::all(glm::isfinite(camera->direction)));
  assert(glm::length(camera->direction) > 0.0f);
  return glm::lookAt(camera->position, camera->position + camera->direction, camera->up);
}

inline glm::mat4 camera_lookat_with_offset(const Camera *camera, glm::vec3 offset) {
  glm::vec3 pos = camera->position + offset;
  return glm::lookAt(pos, pos + camera->direction, camera->up);
}

inline glm::mat4 camera_perspective_projection(const Camera *camera, u32 window_width, u32 window_height) {
  f32 aspect_ratio = f32(window_width) / f32(window_height);
  glm::mat4 proj = glm::perspective(glm::radians(camera->fovy), aspect_ratio, CAMERA_PERSPECTIVE_PROJECTION_NEAR_Z,
                                    CAMERA_PERSPECTIVE_PROJECTION_FAR_Z);
  proj[1][1] = camera->y_needs_inverted ? -proj[1][1] : proj[1][1];
  return proj;
}

inline CameraMatrices new_camera_matrices(const Camera *camera, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices;
  camera_matrices.view = camera_look_at(camera);
  camera_matrices.projection = camera_perspective_projection(camera, window_width, window_height);
  return camera_matrices;
}

inline CameraMatrices camera_matrices_with_offset(const Camera *camera, glm::vec3 offset, u32 window_width,
                                                  u32 window_height) {
  CameraMatrices camera_matrices;
  camera_matrices.view = camera_lookat_with_offset(camera, offset);
  camera_matrices.projection = camera_perspective_projection(camera, window_width, window_height);
  return camera_matrices;
}

inline void log_camera(const Camera *camera) {
  printf("camera position:  ");
  log_vec3(&camera->position);

  printf("camera direction: ");
  log_vec3(&camera->direction);

  printf("camera up:        ");
  log_vec3(&camera->up);
}
