#include "tilemap.h"

static inline void populate_tile_vertex(f32 x, f32 y, f32 z, f32 u, f32 v, f32 texture_id,
                                        TileVertex *out_tile_vertex) {
  out_tile_vertex->texture_coords[0] = u;
  out_tile_vertex->texture_coords[1] = v;

  out_tile_vertex->position[0] = x;
  out_tile_vertex->position[1] = y;
  out_tile_vertex->position[2] = z;

  out_tile_vertex->texture_id = texture_id;
}

// populating a pointer to an existing array instead of returning the pointer
// anticipating callers will manage the memory, allowing for usage in an arena
//
// each tile in the map gets 6 vertices, BL, TL, TR - TR, BR, BL
void tilemap_generate_vertices(const Tilemap *tilemap, TileVertex *out_tile_vertices) {
  u32 i = 0;
  // TODO before deciding on how to manage world size, just do stuff in screen space
  const f32 w_scale = 2.0f / (f32)tilemap->level_width;
  const f32 h_scale = 2.0f / (f32)tilemap->level_height;

  const f32 u0 = 0.0f;
  const f32 u1 = 1.0f;
  const f32 v0 = 0.0f;
  const f32 v1 = 1.0f;
  // TODO
  const f32 z0 = 0.0f;

  for (u32 y = 0; y < tilemap->level_height; y++) {
    for (u32 x = 0; x < tilemap->level_width; x++) {

      f32 x0 = -1.0 + x * w_scale;
      f32 x1 = -1.0 + (x + 1) * w_scale;
      f32 y0 = -1.0 + y * h_scale;
      f32 y1 = -1.0 + (y + 1) * h_scale;
      f32 texture_id = (f32)tilemap->level_map[y * tilemap->level_width + x];
      TileVertex *tile_vertex = out_tile_vertices + i;

      populate_tile_vertex(x0, y0, z0, u0, v0, texture_id, tile_vertex + 0); // BL
      populate_tile_vertex(x0, y1, z0, u0, v1, texture_id, tile_vertex + 1); // TL
      populate_tile_vertex(x1, y1, z0, u1, v1, texture_id, tile_vertex + 2); // TR

      populate_tile_vertex(x1, y1, z0, u1, v1, texture_id, tile_vertex + 3); // TR
      populate_tile_vertex(x1, y0, z0, u1, v0, texture_id, tile_vertex + 4); // BR
      populate_tile_vertex(x0, y0, z0, u0, v0, texture_id, tile_vertex + 5); // BL

      i += 6;
    }
  }
}
