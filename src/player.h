#pragma once

#include "glm/glm.hpp"
#include "renderer.h"

// draw the player as a textured quad
// BL, TL, TR, BR

// clang-format off
const float player_vertices[] = {
    -0.5f, -0.5f, 0.0f, 0.0, 0.0,
    -0.5f,  0.5f, 0.0f, 0.0, 1.0,
     0.5f,  0.5f, 0.0f, 1.0, 1.0,
     0.5f, -0.5f, 0.0f, 1.0, 0.0,
};
// clang-format on

struct Player {
  glm::vec3 position;
  glm::vec3 size;
  glm::vec3 color; // FIXME
  float speed = 5e-3;

  unsigned program;
  RenderData render_data;
  int u_mvp_location;
  unsigned vao;
};

Player new_player(unsigned program, const glm::vec3 &pos = {0.0f, 0.0f, 0.0f},
                  const glm::vec3 &size = {1.0f, 1.0f, 1.0f},
                  const glm::vec3 &color = {0.3f, 0.6f, 0.3f});

void draw_player(const Player &player, const glm::mat4 &mvp);
void update_player_position(Player *player, float delta_t,
                            const glm::vec4 &movement_direction);
