#include <OpenGL/OpenGL.h>
#include <glad/gl.h>

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

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
  // Camera camera = new_camera(CameraType::Camera2D);

  unsigned player_texture = load_texture_opengl("textures/generic_girl.jpg");
  unsigned player_program = link_shader_program("shaders/player_vertex.glsl",
                                                "shaders/player_fragment.glsl");

  unsigned tilemap_program = link_shader_program(
      "shaders/tilemap_vertex.glsl", "shaders/tilemap_fragment.glsl");

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
      new_tilemap_opengl_render_data(tilemap_program, &tilemap);

  Player player = new_player({0.0, 0.0, 1.0}, {1.0, 1.0, 1.0});
  PlayerOpenGLRenderData player_render_data =
      new_player_opengl_render_data(player_program, player_texture);

  glClearColor(0.0f, 0.1f, 0.4, 1.0f);
  glEnable(GL_DEPTH_TEST);
  double t_prev = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double t_current = glfwGetTime();
    double delta_t = t_current - t_prev;
    (void)delta_t;

    int height, width;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // const CameraMatrices camera_matrices =
    // new_camera_matrices(&camera, width, height);
    // buffer_camera_matrices_to_gl_uniform_buffer(&camera_matrices);

    const glm::mat4 player_model = model_from_player(&player);
    const glm::mat4 tilemap_model = glm::mat4(1.0);

    opengl_draw_tilemap(&tilemap_render_data, tilemap_model);
    opengl_draw_player(&player_render_data, player_model);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
