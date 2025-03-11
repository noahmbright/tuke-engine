#include "player.h"
#include "renderer.h"

#include <OpenGL/OpenGL.h>
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

PlayerOpenGLRenderData new_player_opengl_render_data(unsigned program,
                                                     unsigned texture) {
  PlayerOpenGLRenderData data;
  data.program = program;
  data.texture = texture;
  data.u_mvp_location = glGetUniformLocation(program, "u_mvp");

  unsigned vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(player_vertices), player_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(player_indices), player_indices,
               GL_STATIC_DRAW);

  data.vao = vao;
  return data;
}

Player new_player(const glm::vec3 &pos, const glm::vec3 &size) {
  Player p;
  p.size = size;
  p.position = pos;

  return p;
}

void opengl_draw_player(const PlayerOpenGLRenderData &render_data,
                        const glm::mat4 &mvp) {
  glUseProgram(render_data.program);
  glBindVertexArray(render_data.vao);
  glBindTexture(GL_TEXTURE_2D, render_data.texture);
  glUniformMatrix4fv(render_data.u_mvp_location, 1, GL_FALSE, &mvp[0][0]);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void update_player_position(Player *player, float delta_t,
                            const glm::vec4 &movement_direction) {
  const glm::vec3 dir3{movement_direction.x, movement_direction.y,
                       movement_direction.z};

  player->position += player->speed * delta_t * (movement_direction.w) * dir3;
}
