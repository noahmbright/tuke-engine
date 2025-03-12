#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"
#include <OpenGL/OpenGL.h>
#include <glad/gl.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "player.h"
#include "renderer.h"
#include "shaders.h"
#include "tilemap.h"
#include "utils.h"
#include "window.h"

int main() {
  GLFWwindow *window = new_window();
  Camera camera =
      new_camera_from_window(CameraType::Camera2D, window, {0, 0, 5});

  unsigned player_texture = load_texture("textures/generic_girl.jpg");
  unsigned tilemap_program = link_shader_program(
      "shaders/tilemap_vertex.glsl", "shaders/tilemap_fragment.glsl");
  int tilemap_mvp_location = glGetUniformLocation(tilemap_program, "mvp");

  unsigned player_program = link_shader_program("shaders/player_vertex.glsl",
                                                "shaders/player_fragment.glsl");

  const int level_width = 16;
  const int level_height = 9;
  // clang-format off
  int level_map[level_width * level_height] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  // clang-format on

  Tilemap tilemap = new_tilemap(level_width, level_height, level_map);
  TilemapOpenGLRenderData tilemap_render_data =
      new_tilemap_opengl_render_data(&tilemap);

  Player player = new_player({0.0, 0.0, 0.0}, {1.0, 1.0, 1.0});
  PlayerOpenGLRenderData player_render_data =
      new_player_opengl_render_data(player_program, player_texture);

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
    glm::mat4 perspective_projection =
        perspective_projection_from_camera(&camera, width, height);
    glm::mat4 view_proj = perspective_projection * view;

    glm::mat4 player_model = model_from_player(&player);
    glm::mat4 player_mvp = view_proj * player_model;

    glUseProgram(tilemap_program);
    glUniformMatrix4fv(tilemap_mvp_location, 1, GL_FALSE, &view_proj[0][0]);
    opengl_draw_tilemap(&tilemap_render_data);

    opengl_draw_player(player_render_data, player_mvp);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
