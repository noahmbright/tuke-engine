#pragma once

struct TileVec2 {
  float x, y;
};

struct TileVec3 {
  float x, y, z;
};

struct TileVec4 {
  float x, y, z, w;
};

struct TileVertex {
  TileVec2 texture_coords;
  TileVec3 position;
  TileVec4 color;
  float texture_id;
};

struct Tilemap {
  const int level_width;
  const int level_height;
  const int *level_map;
  const float *vertices;
  const unsigned *element_indices;

  unsigned vbo;
  unsigned vao;
  unsigned ebo;
};

struct TileQuad {
  TileVertex bottom_left;
  TileVertex top_left;
  TileVertex top_right;
  TileVertex bottom_right;
};

Tilemap new_tilemap(const int width, const int height, const int map[]);
void draw_tilemap(Tilemap *tilemap);
