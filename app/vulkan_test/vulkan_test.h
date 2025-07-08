#pragma once
#include "tuke_engine.h"

// clang-format off
const f32 triangle_vertices[] = {
  0.0f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 
  0.5f, 0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.0f,  0.0f, 0.0f, 1.0f
};

const f32 square_vertices[] = {
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
};

// TL, BL, BR, TR
const f32 unit_square_positions[] = {
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};

const f32 quad_positions[] = {
   0.5,  0.5,
  -0.5, -0.5
  -0.1, -0.1
  -0.1, -0.1
  -0.3, -0.3
  -0.3, -0.3
};
// clang-format on
