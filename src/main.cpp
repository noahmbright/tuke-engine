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

int main() {
  GLFWwindow *window = new_window();
  Camera camera =
      new_camera_from_window(CameraType::Camera2D, window, {0, 0, 5});

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
  Player player = new_player(player_program, {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0});

  glClearColor(0.0f, 0.1f, 0.4, 1.0f);
  glEnable(GL_DEPTH_TEST);
  double t_prev = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double t_current = glfwGetTime();
    double delta_t = t_current - t_prev;

    glm::vec4 movement_direction = get_window_movement_vector(window);
    move_camera(&camera, delta_t, movement_direction);
    update_player_position(&player, delta_t, movement_direction);

    int height, width;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glm::mat4 view = look_at_from_camera(&camera);
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
