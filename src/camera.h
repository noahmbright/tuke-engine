#pragma once

#include "GLFW/glfw3.h"
#include <glm/vec3.hpp>

enum class CameraType { Camera2D, Camera3D, CameraFPS };

struct Camera {
  CameraType type;
  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 up;
  glm::vec3 right;
  GLFWwindow *window;
  void (*move_camera)(Camera *, float);
};

Camera new_camera(GLFWwindow *window, CameraType type,
                  const glm::vec3 &pos = {0.0, 0.0, 0.0},
                  const glm::vec3 &direction = {0.0, 0.0, -1.0},
                  const glm::vec3 &up = {0.0, 1.0, 0.0},
                  const glm::vec3 &right = {1.0, 0.0, 0.0});

void move_camera_2d(Camera *camera, float delta_t);
void move_camera_3d(Camera *camera, float delta_t);
