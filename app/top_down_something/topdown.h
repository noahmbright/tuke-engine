#pragma once

#include "generated_shader_utils.h"
#include "tilemap.h"
#include "tuke_engine.h"

const u32 width_in_tiles = 9;
const u32 height_in_tiles = 20;

// clang-format off
inline u8 tilemap_data[width_in_tiles * height_in_tiles] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
};

const Tilemap tilemap = {
    .level_map = &tilemap_data[0],
    .level_width = width_in_tiles,
    .level_height = height_in_tiles
};

const u32 width_in_tiles1 = 16;
const u32 height_in_tiles1 = 8;

inline u8 tilemap1_data[width_in_tiles1 * height_in_tiles1] = {
    1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 1,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 1,   1, 1, 1, 1,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,
};

const f32 player_vertices[] = {
  // x,y,z            u, v
  -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // BL
  -0.5f,  0.5f,  0.0f,  0.0f, 1.0f, // TL
   0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // TR

   0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // TR
   0.5f, -0.5f,  0.0f,  1.0f, 0.0f, // BR
  -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // BL
};

const Tilemap tilemap1 = {
    .level_map = &tilemap1_data[0],
    .level_width = width_in_tiles1,
    .level_height = height_in_tiles1
};

// clang-format on

struct Scene0Data {
  glm::vec3 player_pos;
};

struct Scene1Data {
  glm::vec3 player_pos;
};
