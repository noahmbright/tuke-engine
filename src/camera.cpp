#include "camera.h"
#include "GLFW/glfw3.h"
#include "glm/trigonometric.hpp"
#include <glm/ext/matrix_common.hpp>
#include <iostream>

static bool has_moused_yet = false;
extern float pitch;
extern float yaw;
extern float mouse_sensitivity;
extern Camera *global_camera;

double last_mouse_x, last_mouse_y;
static void mouse_callback_3d(GLFWwindow *window, double xpos, double ypos) {
  (void)window;
  if (!has_moused_yet) {
    has_moused_yet = true;
    last_mouse_x = xpos;
    last_mouse_y = ypos;
  }

  double dx = xpos - last_mouse_x;
  double dy = ypos - last_mouse_y;
  last_mouse_x = xpos;
  last_mouse_y = ypos;
  yaw += mouse_sensitivity * dx;
  pitch -= mouse_sensitivity * dy;

  if (pitch > 1.57) {
    pitch = 1.57;
  }
  if (pitch < -1.57) {
    pitch = -1.57;
  }

  global_camera->direction.x = glm::cos(pitch) * glm::cos(yaw);
  global_camera->direction.y = glm::sin(pitch);
  global_camera->direction.z = glm::cos(pitch) * glm::sin(yaw);
  global_camera->right =
      glm::cross(global_camera->direction, global_camera->up);

#if 0
  std::cout << pitch << " " << yaw << std::endl;
  std::cout << global_camera->direction.x << " " << global_camera->direction.y
            << " " << global_camera->direction.z << std::endl;
#endif
}

Camera new_camera(GLFWwindow *window, CameraType type, const glm::vec3 &pos,
                  const glm::vec3 &direction, const glm::vec3 &up,
                  const glm::vec3 &right) {
  Camera camera{type, pos, direction, up, right, window, nullptr};
  camera.type = type;
  camera.window = window;

  switch (camera.type) {
  case CameraType::Camera2D:
    camera.move_camera = move_camera_2d;
    break;
  case CameraType::Camera3D:
  case CameraType::CameraFPS:
    // FIXME FPS

    float xy_magnitude =
        glm::length(glm::vec2{camera.direction.x, camera.direction.y});
    float xz_magnitude =
        glm::length(glm::vec2{camera.direction.x, camera.direction.z});
    const float tolerance = 1e-5;
    if (xy_magnitude < tolerance) {
      yaw = -1.57;
    } else {
      yaw = glm::atan(camera.direction.z / xy_magnitude);
    }

    if (xz_magnitude <= tolerance) {
      pitch = -3.14;
    } else {
      pitch = glm::atan(camera.direction.y / xz_magnitude);
    }
    camera.move_camera = move_camera_3d;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback_3d);
    break;
  }

  return camera;
}

void move_camera_2d(Camera *camera, float delta_t) {
  const float speed = 1e-3;
  GLFWwindow *window = camera->window;

  float sprint_boost =
      1.0 + float(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    camera->position.y += sprint_boost * speed * delta_t;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    camera->position.x -= sprint_boost * speed * delta_t;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    camera->position.y -= sprint_boost * speed * delta_t;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    camera->position.x += sprint_boost * speed * delta_t;
  }
}

void move_camera_3d(Camera *camera, float delta_t) {
  GLFWwindow *window = camera->window;
  const float speed = 1e-2;
  float sprint_boost =
      1.0 + float(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    camera->position += (sprint_boost * speed * delta_t) * camera->direction;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    camera->position -= (sprint_boost * speed * delta_t) * camera->right;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    camera->position -= (sprint_boost * speed * delta_t) * camera->direction;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    camera->position += (sprint_boost * speed * delta_t) * camera->right;
  }
}
