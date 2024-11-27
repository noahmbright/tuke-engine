#include <OpenGL/OpenGL.h>
#define GLFW_INCLUDE_NONE
// https://www.glfw.org/docs/3.3/quick_guide.html
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdio.h>
#include <stdlib.h>

// for glfw errors
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

int main(void) {
  const int WIDTH = 800;
  const int HEIGHT = 600;

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize glfw");
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Tuke", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to initialize window");
    glfwTerminate();
    exit(1);
  }

  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  gladLoadGL(glfwGetProcAddress);

  while (!glfwWindowShouldClose(window)) {
    int height, width;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
