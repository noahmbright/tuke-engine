#include "glm/ext/matrix_transform.hpp"
#include <OpenGL/OpenGL.h>
#define GLFW_INCLUDE_NONE
// https://www.glfw.org/docs/3.3/quick_guide.html
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "renderer.h"
#include "shaders.h"

Camera2d camera = new_camera2d({});
void *GlobalCamera = &camera;

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

  unsigned shader_program =
      link_shader_program("shaders/vertex.glsl", "shaders/fragment.glsl");
  glUseProgram(shader_program);

  unsigned square_VAO;
  glGenVertexArrays(1, &square_VAO);
  glBindVertexArray(square_VAO);

  unsigned square_VBO;
  glGenBuffers(1, &square_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, square_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        (void *)NULL);
  glEnableVertexAttribArray(0);
  glClearColor(0.0f, 0.1f, 0.4, 1.0f);

  set_camera2d_window_and_scroll_callback(&camera, window);

  int mvp_location = glGetUniformLocation(shader_program, "mvp");

  double t_prev = glfwGetTime(), t_current;
  while (!glfwWindowShouldClose(window)) {
    t_current = glfwGetTime();
    double delta_t = t_current - t_prev;
    int height, width;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glfwSwapBuffers(window);
    glfwPollEvents();
    move_camera_2d(window, &camera);
    glm::mat4 view = glm::lookAt(
        camera.position, camera.position + camera.direction, {0.0, 1.0, 0.0});
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, &view[0][0]);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
