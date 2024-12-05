#pragma once

#include "GLFW/glfw3.h"
#include <glm/vec3.hpp>

extern void *GlobalCamera;

struct Camera3d {
  glm::vec3 position;
  glm::vec3 direction;
  GLFWwindow *window;
};

struct Camera2d {
  glm::vec3 position;
  glm::vec3 direction;
  GLFWwindow *window;
};

void move_camera_2d(GLFWwindow *window, Camera2d *camera);
void scroll_callback_2d_camera(GLFWwindow *window, double xoffset,
                               double yoffset);
Camera2d new_camera2d(const glm::vec3 &pos0,
                      const glm::vec3 dir0 = {0.0, 0.0, -1.0});
void set_camera2d_window_and_scroll_callback(Camera2d *cam, GLFWwindow *window);
