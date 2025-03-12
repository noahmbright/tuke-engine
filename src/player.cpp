#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "camera.h"
#include "player.h"

PlayerOpenGLRenderData new_player_opengl_render_data(unsigned program,
                                                     unsigned texture) {
  PlayerOpenGLRenderData data;
  data.program = program;
  data.texture = texture;

  data.u_model_location = glGetUniformLocation(program, "u_model");

  data.matrices_buffer_index = glGetUniformBlockIndex(program, "u_Matrices");
  glUniformBlockBinding(program, data.matrices_buffer_index,
                        CAMERA_MATRICES_INDEX);

  glGenVertexArrays(1, &data.vao);
  glBindVertexArray(data.vao);

  glGenBuffers(1, &data.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(player_vertices), player_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &data.ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(player_indices), player_indices,
               GL_STATIC_DRAW);

  return data;
}

Player new_player(const glm::vec3 &pos, const glm::vec3 &size) {
  Player p;
  p.size = size;
  p.position = pos;

  return p;
}

void opengl_draw_player(const PlayerOpenGLRenderData *render_data,
                        const glm::mat4 &model) {
  glUseProgram(render_data->program);
  glBindVertexArray(render_data->vao);
  glBindTexture(GL_TEXTURE_2D, render_data->texture);
  glUniformMatrix4fv(render_data->u_model_location, 1, GL_FALSE,
                     glm::value_ptr(model));
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void update_player_position(Player *player, float delta_t,
                            const glm::vec4 &movement_direction) {
  const glm::vec3 dir3{movement_direction.x, movement_direction.y,
                       movement_direction.z};

  player->position += player->speed * delta_t * (movement_direction.w) * dir3;
}

glm::mat4 model_from_player(const Player *player) {
  glm::mat4 player_model = glm::scale(glm::mat4(1.0), player->size);
  player_model = glm::translate(player_model, player->position);
  return player_model;
}
