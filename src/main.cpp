#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <OpenGL/OpenGL.h>
#include <glad/gl.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "renderer.h"
#include "shaders.h"
#include "tilemap.h"
#include "window.h"

float pitch, yaw;
// mouse_sensitivity global to allow for changing in game settings and then have
// that change visible in the global mouse callback function
float mouse_sensitivity = 1e-3;
Camera *global_camera;

int main(void) {
  GLFWwindow *window = new_window();
  // Camera camera = new_camera(window, CameraType::Camera3D, {0.0, 0.0, 2.0});
  Camera camera = new_camera(window, CameraType::Camera2D, {0, 0, 1});
  global_camera = &camera;

  unsigned square_program =
      link_shader_program("shaders/vertex.glsl", "shaders/fragment.glsl");
  glUseProgram(square_program);
  int square_mvp_location = glGetUniformLocation(square_program, "mvp");
  (void)square_mvp_location;

  unsigned cube_program = link_shader_program(
      "shaders/simple_cube_vertex.glsl", "shaders/simple_cube_fragment.glsl");
  glUseProgram(cube_program);
  int cube_mvp_location = glGetUniformLocation(cube_program, "mvp");
  (void)cube_mvp_location;

  unsigned tilemap_program = link_shader_program(
      "shaders/tilemap_vertex.glsl", "shaders/tilemap_fragment.glsl");
  int tilemap_mvp_location = glGetUniformLocation(tilemap_program, "mvp");
  (void)tilemap_program;

  const int level_width = 16;
  const int level_height = 9;
  // clang-format off
  const int level_map[level_width * level_height] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  // clang-format on
  Tilemap tilemap = new_tilemap(level_width, level_height, level_map);
  (void)tilemap;

  const int trial_level_width = 1;
  const int trial_level_height = 1;
  const int trial_map[1] = {1};
  Tilemap trial_tilemap =
      new_tilemap(trial_level_width, trial_level_height, trial_map);

#if 1
  for (int i = 0; i < 6; i++) {
    printf("%d ", trial_tilemap.element_indices[i]);
  }
  printf("\n ");
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 10; i++) {
      printf("%f ", trial_tilemap.vertices[10 * j + i]);
    }
    printf("\n ");
  }
#endif

  RenderData square_data = init_square(square_program);
  RenderData cube_data = init_cube(cube_program);
  (void)square_data;
  (void)cube_data;

  glClearColor(0.0f, 0.1f, 0.4, 1.0f);
  glEnable(GL_DEPTH_TEST);
  double t_prev = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    double t_current = glfwGetTime();
    double delta_t = t_current - t_prev;

    int height, width;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = glm::lookAt(camera.position,
                                 camera.position + camera.direction, camera.up);
    camera.move_camera(&camera, delta_t);
    glm::mat4 perspective_projection = glm::perspective(
        glm::radians(45.0f), float(width) / float(height), 0.1f, 100.0f);
    view = perspective_projection * view;
    // glUniformMatrix4fv(square_mvp_location, 1, GL_FALSE, &view[0][0]);
    // glUniformMatrix4fv(cube_mvp_location, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(tilemap_mvp_location, 1, GL_FALSE, &view[0][0]);
    glUseProgram(tilemap_program);
    // draw_tilemap(&trial_tilemap);
    draw_tilemap(&tilemap);

    // draw_square(&square_data);
    // draw_cube(&cube_data);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
