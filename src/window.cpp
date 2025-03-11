#include "glm/ext/quaternion_geometric.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "window.h"
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

void error_callback(int error, const char *description) {
  (void)error;
  fprintf(stderr, "GLFW error callback:\n%s\n", description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  (void)action;
  (void)mods;
  (void)scancode;

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

GLFWwindow *new_window(bool is_vulkan, const char *title, const int width,
                       const int height) {

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize glfw\n");
    exit(1);
  }

  if (is_vulkan) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  } else {
    // is opengl
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  }

  GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to initialize window\n");
    glfwTerminate();
    exit(1);
  }

  glfwSetKeyCallback(window, key_callback);

  if (!is_vulkan) {
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
  }

  return window;
}

glm::vec4 get_window_movement_vector(GLFWwindow *window) {

  glm::vec4 res{};

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    res.y += 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    res.x -= 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    res.y -= 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    res.x += 1.0f;
  }

  if (glm::length(res))
    res = glm::normalize(res);

  float sprint_boost =
      1.0f + float(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

  res.w = sprint_boost;
  return res;
}
