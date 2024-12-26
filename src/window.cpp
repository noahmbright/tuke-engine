#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

void error_callback(int error, const char *description) {
  (void)error;
  fprintf(stderr, "%s", description);
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

GLFWwindow *new_window(const int width, const int height) {

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize glfw");
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  GLFWwindow *window = glfwCreateWindow(width, height, "Tuke", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to initialize window");
    glfwTerminate();
    exit(1);
  }

  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  gladLoadGL(glfwGetProcAddress);

  return window;
}
