#include "camera.h"
#include "GLFW/glfw3.h"
#include "glm/trigonometric.hpp"
#include <glm/ext/matrix_common.hpp>
#include <iostream>

static void mouse_callback_3d(GLFWwindow *window, double xpos, double ypos) {
  Camera *camera = (Camera *)glfwGetWindowUserPointer(window);
  if (!camera) {
    fprintf(stderr, "mouse_callback_3d: camera is null\n");
  }

  static double last_mouse_x, last_mouse_y;
  static bool has_moused_yet = false;
  if (!has_moused_yet) {
    has_moused_yet = true;
    last_mouse_x = xpos;
    last_mouse_y = ypos;
  }

  double dx = xpos - last_mouse_x;
  double dy = ypos - last_mouse_y;
  last_mouse_x = xpos;
  last_mouse_y = ypos;
  camera->yaw += camera->mouse_sensitivity * dx;
  camera->pitch -= camera->mouse_sensitivity * dy;

  if (camera->pitch > 1.57) {
    camera->pitch = 1.57;
  }
  if (camera->pitch < -1.57) {
    camera->pitch = -1.57;
  }

  camera->direction.x = glm::cos(camera->pitch) * glm::cos(camera->yaw);
  camera->direction.y = glm::sin(camera->pitch);
  camera->direction.z = glm::cos(camera->pitch) * glm::sin(camera->yaw);
  camera->right = glm::cross(camera->direction, camera->up);
}

Camera new_camera(CameraType type, const glm::vec3 &pos,
                  const glm::vec3 &direction, const glm::vec3 &up,
                  const glm::vec3 &right) {
  Camera camera;
  camera.type = type;
  camera.position = pos;
  camera.direction = direction;
  camera.up = up;
  camera.right = right;

  switch (camera.type) {

  case CameraType::Camera2D:
    camera.move_camera_function = move_camera_2d;
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
      camera.yaw = -1.57;
    } else {
      camera.yaw = glm::atan(camera.direction.z / xy_magnitude);
    }

    if (xz_magnitude <= tolerance) {
      camera.pitch = -3.14;
    } else {
      camera.pitch = glm::atan(camera.direction.y / xz_magnitude);
    }
    camera.move_camera_function = move_camera_3d;
    break;
  }

  return camera;
}

void move_camera_2d(GLFWwindow *window, Camera *camera, float delta_t) {
  const float speed = 5e-3;

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

void move_camera_3d(GLFWwindow *window, Camera *camera, float delta_t) {
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

void move_camera(GLFWwindow *window, Camera *camera, float delta_t) {
  camera->move_camera_function(window, camera, delta_t);
}

Camera new_camera_from_window(CameraType type, GLFWwindow *window,
                              const glm::vec3 &pos, const glm::vec3 &direction,
                              const glm::vec3 &up, const glm::vec3 &right) {
  Camera camera = new_camera(type, pos, direction, up, right);
  glfwSetWindowUserPointer(window, &camera);

  if (camera.type == CameraType::Camera3D) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback_3d);
  }

  return camera;
}
