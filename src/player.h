#pragma once

#include "glm/glm.hpp"
#include "renderer.h"

struct Player {
  glm::vec3 position;
  glm::vec3 size;
  glm::vec3 color; // FIXME

  unsigned program;
  RenderData render_data;
  int u_mvp_location;
  unsigned vao;
};

Player new_player(unsigned program, glm::vec3 pos = {0.0f, 0.0f, 0.0f},
                  glm::vec3 size = {1.0f, 1.0f, 1.0f},
                  glm::vec3 color = {0.3f, 0.6f, 0.3f});

void draw_player(const Player &player, const glm::mat4 &mvp);
