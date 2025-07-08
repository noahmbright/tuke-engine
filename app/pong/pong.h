#pragma once

#include "glm/glm.hpp"
#include "tuke_engine.h"

// clang-format off
// start with rectangles, TODO cube
// TL, BL, BR, TR
const f32 paddle_vertices[] = {
   // x, y, z         u, v
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};
// clang-format on

enum GameState {
  GAMESTATE_PAUSED,
  GAMESTATE_PLAYING,
  GAMESTATE_MAIN_MENU,
};

struct Paddle {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::vec3 size;
};
