#pragma once

#include "glm/fwd.hpp"
#include "glm/glm.hpp"
#include "renderer.h"

// draw the player as a textured quad
// BL, TL, TR, BR

// clang-format off
const float player_vertices[] = {
    // pos x, y, z,     tex u, v
    -0.5f, -0.5f, 0.0f, 0.0, 0.0,
    -0.5f,  0.5f, 0.0f, 0.0, 1.0,
     0.5f,  0.5f, 0.0f, 1.0, 1.0,
     0.5f, -0.5f, 0.0f, 1.0, 0.0,
};

const unsigned player_indices[] = {
    0, 1, 2, 0, 2, 3,
};
// clang-format on

struct PlayerOpenGLRenderData {
  unsigned program;
  unsigned texture;
  unsigned vao, vbo, ebo;

  int u_mvp_location;
};

struct Player {
  glm::vec3 position;
  glm::vec3 size;
  float speed = 5e-3;
};

Player new_player(const glm::vec3 &pos = {0.0f, 0.0f, 0.0f},
                  const glm::vec3 &size = {1.0f, 1.0f, 1.0f});

void opengl_draw_player(const PlayerOpenGLRenderData &data,
                        const glm::mat4 &mvp);
void update_player_position(Player *player, float delta_t,
                            const glm::vec4 &movement_direction);

PlayerOpenGLRenderData new_player_opengl_render_data(unsigned program,
                                                     unsigned texture);

glm::mat4 model_from_player(const Player *player);
