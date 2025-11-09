#include "tilemap.h"

static inline void populate_tile_vertex(f32 x, f32 y, f32 z, f32 u, f32 v, u32 texture_id,
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
  // centering the tilemap at 0.0
  const f32 dw = TILE_SIDE_LENGTH_METERS;
  const f32 dh = TILE_SIDE_LENGTH_METERS;
  const f32 total_width = dw * tilemap->level_width;
  const f32 total_height = dh * tilemap->level_height;
  const f32 x0 = -0.5f * total_width;
  const f32 y0 = -0.5f * total_height;

  const f32 u0 = 0.0f;
  const f32 u1 = 1.0f;
  const f32 v0 = 0.0f;
  const f32 v1 = 1.0f;
  const f32 z0 = 0.0f; // TODO

  u32 i = 0;
  for (u32 y = 0; y < tilemap->level_height; y++) {
    for (u32 x = 0; x < tilemap->level_width; x++) {

      f32 x1 = x0 + x * dw;
      f32 x2 = x1 + dw;
      f32 y1 = y0 + y * dh;
      f32 y2 = y1 + dh;
      f32 texture_id = (f32)tilemap->level_map[y * tilemap->level_width + x];
      TileVertex *tile_vertex = out_tile_vertices + i;

      populate_tile_vertex(x1, y1, z0, u0, v0, texture_id, tile_vertex + 0); // BL
      populate_tile_vertex(x1, y2, z0, u0, v1, texture_id, tile_vertex + 1); // TL
      populate_tile_vertex(x2, y2, z0, u1, v1, texture_id, tile_vertex + 2); // TR

      populate_tile_vertex(x2, y2, z0, u1, v1, texture_id, tile_vertex + 3); // TR
      populate_tile_vertex(x2, y1, z0, u1, v0, texture_id, tile_vertex + 4); // BR
      populate_tile_vertex(x1, y1, z0, u0, v0, texture_id, tile_vertex + 5); // BL

      i += 6;
    }
  }
}

bool tilemap_check_collision(const Tilemap *tilemap, const glm::vec3 tilemap_top_left, glm::vec3 pos, glm::vec3 size) {

  glm::vec3 delta_r = pos - tilemap_top_left;
  delta_r.y = -delta_r.y; // tile index grows as we go downward in view space
  glm::vec3 half_size = 0.5f * size;
  f32 x0 = delta_r.x - half_size.x;
  f32 x1 = delta_r.x + half_size.x;
  f32 y0 = delta_r.y - half_size.y;
  f32 y1 = delta_r.y + half_size.y;

  u32 nx0 = (x0 < EPSILON) ? 0 : x0 / TILE_SIDE_LENGTH_METERS;
  u32 ny0 = (y0 < EPSILON) ? 0 : y0 / TILE_SIDE_LENGTH_METERS;

  u32 nx1 = (x1 / TILE_SIDE_LENGTH_METERS) + 1;
  u32 ny1 = (y1 / TILE_SIDE_LENGTH_METERS) + 1;

  // TODO logging system
#if 0
  printf("tilemap_check_collision:\n");
  printf("  pos is (%4.3f, %4.3f), size is (%4.3f, %4.3f), tilemap_top_left is (%4.3f, %4.3f)\n", pos.x, pos.y, size.x,
         size.y, tilemap_top_left.x, tilemap_top_left.y);
  printf("  delta r is (%4.3f, %4.3f)\n  iterating nx in [%u, %u), ny in [%u, %u)\n", delta_r.x, delta_r.y, nx0, nx1,
         ny0, ny1);
#endif

  bool collided = false;
  for (u32 ny = ny0; ny < ny1; ny++) {
    for (u32 nx = nx0; nx < nx1; nx++) {
      if (tilemap_get_at(tilemap, nx, ny) == 1) {
        return true;
      }
    }
  }

  return collided;
}
