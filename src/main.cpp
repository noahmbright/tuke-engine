#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "player.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"
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
  Camera camera = new_camera(window, CameraType::Camera2D, {0, 0, 5});
  global_camera = &camera;

  unsigned tilemap_program = link_shader_program(
      "shaders/tilemap_vertex.glsl", "shaders/tilemap_fragment.glsl");
  int tilemap_mvp_location = glGetUniformLocation(tilemap_program, "mvp");

  unsigned player_program = link_shader_program("shaders/player_vertex.glsl",
                                                "shaders/player_fragment.glsl");

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

#if 0
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

  Player player = new_player(player_program, {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0});

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

    glm::mat4 player_model = glm::scale(glm::mat4(1.0), player.size);
    player_model = glm::translate(player_model, player.position);
    player_model = view * player_model;

    glUseProgram(tilemap_program);
    glUniformMatrix4fv(tilemap_mvp_location, 1, GL_FALSE, &view[0][0]);
    draw_tilemap(&tilemap);

    draw_player(player, player_model);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
