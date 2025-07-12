#pragma once

#define CAMERA_MATRICES_INDEX 0

#include "tuke_engine.h"
#include <glm/glm.hpp>

enum class CameraType { Camera2D, Camera3D, CameraFPS };

struct Camera {
  CameraType type;

  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 up;
  glm::vec3 right;

  f32 pitch, yaw;
  f32 mouse_sensitivity = 1e-3;
  f32 speed = 5e-3;
  f32 fovy = 45.0f;

  void (*move_camera_function)(Camera *, f32, const glm::vec4 &);

  // TODO remove and decouple from opengl
  unsigned ubo;

  f32 last_mouse_x, last_mouse_y;
  bool has_moused_yet;
};

struct CameraMatrices {
  glm::mat4 view, projection;
};

Camera new_camera(CameraType type, const glm::vec3 &pos = {0.0, 0.0, 1.0},
                  const glm::vec3 &direction = {0.0, 0.0, -1.0},
                  const glm::vec3 &up = {0.0, 1.0, 0.0},
                  const glm::vec3 &right = {1.0, 0.0, 0.0});

void move_camera_2d(Camera *camera, f32 delta_t,
                    const glm::vec4 &movement_direction);
void move_camera_3d(Camera *camera, f32 delta_t,
                    const glm::vec4 &movement_direction);
void move_camera(Camera *camera, f32 delta_t,
                 const glm::vec4 &movement_direction);

glm::mat4 look_at_from_camera(const Camera *camera);
glm::mat4 perspective_projection_from_camera(const Camera *camera,
                                             int window_width,
                                             int window_height);
CameraMatrices new_camera_matrices(const Camera *camera, u32 window_width,
                                   u32 window_height);

void buffer_camera_matrices_to_gl_uniform_buffer(
    const CameraMatrices *camera_matrices);
