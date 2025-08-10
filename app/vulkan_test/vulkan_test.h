#pragma once
#include "tuke_engine.h"
#include "vulkan_base.h"
#include <cstring>

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
   // x, y, z, u, v
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};

const u32 instanced_quad_count = 5;
const f32 quad_positions[] = {
  -0.5, -0.5
  -0.1, -0.1
  -0.1, -0.1
  -0.3, -0.3
  -0.3, -0.3
};

const f32 cube_vertices[] = {
    // Front face (+Z)
    // xyz,              nx, ny, nz, u, v
    -0.5f, -0.5f,  0.5f, 0, 0, 1, 0, 0,
     0.5f, -0.5f,  0.5f, 0, 0, 1, 1, 0,
     0.5f,  0.5f,  0.5f, 0, 0, 1, 1, 1,

    -0.5f, -0.5f,  0.5f, 0, 0, 1, 0, 0,
     0.5f,  0.5f,  0.5f, 0, 0, 1, 1, 1,
    -0.5f,  0.5f,  0.5f, 0, 0, 1, 0, 1,

     //Back face (-Z)
     0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0,
    -0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0,
    -0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 1,

     0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0,
    -0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 1,
     0.5f,  0.5f, -0.5f, 0, 0, -1, 0, 1,

     //Left face (-X)
    -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0,
    -0.5f, -0.5f,  0.5f, -1, 0, 0, 1, 0,
    -0.5f,  0.5f,  0.5f, -1, 0, 0, 1, 1,

    -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0,
    -0.5f,  0.5f,  0.5f, -1, 0, 0, 1, 1,
    -0.5f,  0.5f, -0.5f, -1, 0, 0, 0, 1,

     //Right face (+X)
     0.5f, -0.5f,  0.5f, 1, 0, 0, 0, 0,
     0.5f, -0.5f, -0.5f, 1, 0, 0, 1, 0,
     0.5f,  0.5f, -0.5f, 1, 0, 0, 1, 1,

     0.5f, -0.5f,  0.5f, 1, 0, 0, 0, 0,
     0.5f,  0.5f, -0.5f, 1, 0, 0, 1, 1,
     0.5f,  0.5f,  0.5f, 1, 0, 0, 0, 1,

     //Top face (+Y)
    -0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0,
     0.5f,  0.5f,  0.5f, 0, 1, 0, 1, 0,
     0.5f,  0.5f, -0.5f, 0, 1, 0, 1, 1,

    -0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0,
     0.5f,  0.5f, -0.5f, 0, 1, 0, 1, 1,
    -0.5f,  0.5f, -0.5f, 0, 1, 0, 0, 1,

     //Bottom face (-Y)
    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0,
     0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 0,
     0.5f, -0.5f,  0.5f, 0, -1, 0, 1, 1,

    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0,
     0.5f, -0.5f,  0.5f, 0, -1, 0, 1, 1,
    -0.5f, -0.5f,  0.5f, 0, -1, 0, 0, 1,
};
// clang-format on

enum TextureId {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  NUM_TEXTURES
};

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/girl_face.jpg",
    "textures/girl_face_normal_map.jpg"};
