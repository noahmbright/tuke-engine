#pragma once

#include "physics.h"
#include "tuke_engine.h"

#include <glm/glm.hpp>

enum CameraType { CAMERA_TYPE_2D, CAMERA_TYPE_3D, CAMERA_TYPE_FPS };

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

  f32 last_mouse_x, last_mouse_y;
  bool has_moused_yet;
  bool y_needs_inverted;
};

struct CameraMatrices {
  glm::mat4 view, projection;
};

struct ScreenShake {
  DampedHarmonicOscillator x_oscillator;
  DampedHarmonicOscillator y_oscillator;
  bool active;
  f32 time_elapsed;
  f32 cutoff_duration; // seconds
};

Camera new_camera(CameraType type, const glm::vec3 &pos = {0.0, 0.0, 1.0},
                  const glm::vec3 &direction = {0.0, 0.0, -1.0}, const glm::vec3 &up = {0.0, 1.0, 0.0},
                  const glm::vec3 &right = {1.0, 0.0, 0.0});

void move_camera_2d(Camera *camera, f32 delta_t, const glm::vec4 &movement_direction);
void move_camera_3d(Camera *camera, f32 delta_t, const glm::vec4 &movement_direction);
void move_camera(Camera *camera, f32 delta_t, const glm::vec4 &movement_direction);

glm::mat4 look_at_from_camera(const Camera *camera);
glm::mat4 look_at_from_camera_with_offset(const Camera *camera, glm::vec3 offset);
glm::mat4 perspective_projection_from_camera(const Camera *camera, u32 window_width, u32 window_height);
CameraMatrices new_camera_matrices(const Camera *camera, u32 window_width, u32 window_height);
CameraMatrices new_camera_matrices_with_offset(const Camera *camera, glm::vec3 offset, u32 window_width,
                                               u32 window_height);
void log_camera(const Camera *camera);
