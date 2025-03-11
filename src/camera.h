#pragma once

#include "GLFW/glfw3.h"
#include "glm/ext/matrix_transform.hpp"
#include <glm/vec3.hpp>

enum class CameraType { Camera2D, Camera3D, CameraFPS };

struct Camera {
  CameraType type;

  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 up;
  glm::vec3 right;

  float pitch;
  float yaw;
  float mouse_sensitivity = 1e-3;
  float speed = 5e-3;

  void (*move_camera_function)(Camera *, float, const glm::vec4 &);
};

Camera new_camera(CameraType type, const glm::vec3 &pos = {0.0, 0.0, 1.0},
                  const glm::vec3 &direction = {0.0, 0.0, -1.0},
                  const glm::vec3 &up = {0.0, 1.0, 0.0},
                  const glm::vec3 &right = {1.0, 0.0, 0.0});

Camera new_camera_from_window(CameraType type, GLFWwindow *window,
                              const glm::vec3 &pos = {0.0, 0.0, 1.0},
                              const glm::vec3 &direction = {0.0, 0.0, -1.0},
                              const glm::vec3 &up = {0.0, 1.0, 0.0},
                              const glm::vec3 &right = {1.0, 0.0, 0.0});

void move_camera_2d(Camera *camera, float delta_t,
                    const glm::vec4 &movement_direction);
void move_camera_3d(Camera *camera, float delta_t,
                    const glm::vec4 &movement_direction);
void move_camera(Camera *camera, float delta_t,
                 const glm::vec4 &movement_direction);
glm::mat4 look_at_from_camera(Camera *camera);
