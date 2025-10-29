#pragma once

#include "tuke_engine.h"
#include <stdio.h>

struct TileVertex {
  f32 texture_coords[2];
  f32 position[3];
  f32 texture_id;
};

struct Tilemap {
  u32 level_width;
  u32 level_height;
  const u8 *level_map;
};

struct TileQuad {
  TileVertex bottom_left;
  TileVertex top_left;
  TileVertex top_right;
  TileVertex bottom_right;
};

inline void log_tile_vertex(const TileVertex *tile_vertex) {
  printf("  %f %f %f, %f %f, %f\n", tile_vertex->position[0], tile_vertex->position[1], tile_vertex->position[2],
         tile_vertex->texture_coords[0], tile_vertex->texture_coords[1], tile_vertex->texture_id);
}

Tilemap new_tilemap(const u32 width, const u32 height, const u8 *map);
void tilemap_generate_vertices(const Tilemap *tilemap, TileVertex *out_tile_vertices);
