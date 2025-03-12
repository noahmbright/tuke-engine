#pragma once

#include "camera.h"

#include "glm/glm.hpp"

struct TileVertex {
  glm::vec2 texture_coords;
  glm::vec3 position;
  glm::vec4 color;
  float texture_id;
};

struct Tilemap {
  int level_width;
  int level_height;
  int *level_map;
};

struct TileQuad {
  TileVertex bottom_left;
  TileVertex top_left;
  TileVertex top_right;
  TileVertex bottom_right;
};

struct TilemapOpenGLRenderData {
  unsigned vbo, vao, ebo, program;
  unsigned num_quads;
  const float *vertices;
  const unsigned *element_indices;

  int u_model_location;
  unsigned matrices_buffer_index = CAMERA_MATRICES_INDEX;
};

Tilemap new_tilemap(const int width, const int height, int map[]);
void opengl_draw_tilemap(const TilemapOpenGLRenderData *tm,
                         const glm::mat4 &model);
TilemapOpenGLRenderData new_tilemap_opengl_render_data(unsigned program,
                                                       Tilemap *tm);
