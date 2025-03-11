#include "player.h"
#include "renderer.h"

#include <OpenGL/OpenGL.h>
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

Player new_player(unsigned program, const glm::vec3 &pos, const glm::vec3 &size,
                  const glm::vec3 &color) {
  Player p;
  p.color = color;
  p.size = size;
  p.position = pos;
  p.program = program;
  p.u_mvp_location = glGetUniformLocation(program, "u_mvp");

  unsigned vao, vbo;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  p.vao = vao;

  return p;
}

void draw_player(const Player &player, const glm::mat4 &mvp) {
  glUseProgram(player.program);
  glBindVertexArray(player.vao);
  glUniformMatrix4fv(player.u_mvp_location, 1, GL_FALSE, &mvp[0][0]);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void update_player_position(Player *player, float delta_t,
                            const glm::vec4 &movement_direction) {
  const glm::vec3 dir3{movement_direction.x, movement_direction.y,
                       movement_direction.z};

  player->position += player->speed * delta_t * (movement_direction.w) * dir3;
}
