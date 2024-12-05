#include "camera.h"
#include "GLFW/glfw3.h"
#include <glm/ext/matrix_common.hpp>

void move_camera_2d(GLFWwindow *window, Camera2d *camera) {
  const double dt = 1e-3;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    camera->position.y += dt;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    camera->position.x -= dt;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    camera->position.y -= dt;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    camera->position.x += dt;
  }
}

void scroll_callback_2d_camera(GLFWwindow *window, double xoffset,
                               double yoffset) {
  (void)window;
  (void)xoffset;
  const double speed = 1e-3;
  ((Camera2d *)GlobalCamera)->position.z += speed * yoffset;
}

Camera2d new_camera2d(const glm::vec3 &pos0, const glm::vec3 dir0) {
  return {pos0, dir0, NULL};
}

void set_camera2d_window_and_scroll_callback(Camera2d *cam,
                                             GLFWwindow *window) {
  glfwSetScrollCallback(window, scroll_callback_2d_camera);
  cam->window = window;
}
