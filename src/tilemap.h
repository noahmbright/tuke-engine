#pragma once

#include "tuke_engine.h"
#include <stdio.h>

#define TILE_SIDE_LENGTH_METERS (1.0f)

struct TileVertex {
  f32 texture_coords[2];
  f32 position[3];
  u32 texture_id;
};

struct Tilemap {
  u32 level_width;
  u32 level_height;
  u8 *level_map;
  glm::vec3 top_left;
};

struct TileQuad {
  TileVertex bottom_left;
  TileVertex top_left;
  TileVertex top_right;
  TileVertex bottom_right;
};

inline void log_tile_vertex(const TileVertex *tile_vertex) {
  printf("  %f %f %f, %f %f, %u\n", tile_vertex->position[0], tile_vertex->position[1], tile_vertex->position[2],
         tile_vertex->texture_coords[0], tile_vertex->texture_coords[1], tile_vertex->texture_id);
}

// by default, assume the center of the tilemap is at (0,0) in its local coordinate system
inline Tilemap new_tilemap(const u32 width, const u32 height, u8 *map) {
  glm::vec3 top_left;
  top_left.x = -0.5f * TILE_SIDE_LENGTH_METERS * width;
  top_left.y = 0.5f * TILE_SIDE_LENGTH_METERS * height;

  return {.level_width = width, .level_height = height, .level_map = map, .top_left = top_left};
}

static inline u8 tilemap_get_at(const Tilemap *tilemap, u32 x, u32 y) {
  return tilemap->level_map[y * tilemap->level_width + x];
}

// passing in the top left corner of the tilemap in whatever coordinate system it is in from the caller
// pos is in the same coordinate system the caller is using for the map
// pos is the position of the colliding object
// unclear if it would be better to parametrize using the already processed position within the tilemap
int tilemap_check_collision(const Tilemap *tilemap, glm::vec3 pos, glm::vec3 size);

void tilemap_generate_vertices(const Tilemap *tilemap, TileVertex *out_tile_vertices);
