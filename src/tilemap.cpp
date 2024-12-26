#include "tilemap.h"

#include <OpenGL/OpenGL.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <glad/gl.h>
#include <stdlib.h>

static const TileQuad new_quad(const float x, const float y, const float z,
                               const float dx, const float dy, const float r,
                               const float g, const float b, const float alpha,
                               const float tex_id) {
  TileVertex top_left;
  top_left.position = {x, y + dy, z};
  top_left.color = {r, g, b, alpha};
  top_left.texture_coords = {0.0, 1.0};
  top_left.texture_id = tex_id;

  TileVertex top_right;
  top_right.position = {x + dx, y + dy, z};
  top_right.color = {r, g, b, alpha};
  top_right.texture_coords = {1.0, 1.0};
  top_right.texture_id = tex_id;

  TileVertex bottom_left;
  bottom_left.position = {x, y, z};
  bottom_left.color = {r, g, b, alpha};
  bottom_left.texture_coords = {0.0, 0.0};
  bottom_left.texture_id = tex_id;

  TileVertex bottom_right;
  bottom_right.position = {x + dx, y, z};
  bottom_right.color = {r, g, b, alpha};
  bottom_right.texture_coords = {1.0, 0.0};
  bottom_right.texture_id = tex_id;

  TileQuad tq;
  tq.top_left = top_left;
  tq.top_right = top_right;
  tq.bottom_left = bottom_left;
  tq.bottom_right = bottom_right;
  return tq;
}

static void
gen_vertices_and_elements_from_map(const int height, const int width,
                                   const int map[], const float **vertices_out,
                                   const unsigned **element_indices_out) {
  const float dx = 2.0 / width;
  const float dy = 2.0 / height;

  const unsigned long long num_quads = width * height;

  float *vertex_data = (float *)malloc(num_quads * sizeof(TileQuad));
  unsigned *element_indices =
      (unsigned *)malloc(6 * num_quads * sizeof(unsigned));

  for (unsigned long long i = 0; i < num_quads; i++) {
    // positions
    unsigned long long height_index = i / height;
    unsigned long long width_index = i % height;
    float x0 = -1.0 + width_index * dx;
    float y0 = -1.0 + height_index * dy;

    // vertices
    float r = 0.5 * map[i];
    float g = 0;
    float b = 0.5 * (1 - map[i]);
    float alpha = 1.0;
    TileQuad tq = new_quad(x0, y0, 0.0, dx, dy, r, g, b, alpha, map[i]);
    memcpy(&vertex_data[i * (sizeof(TileQuad) / sizeof(float))], &tq,
           sizeof(TileQuad));

    // elements indices
    element_indices[i * 6 + 0] = unsigned(4 * i);
    element_indices[i * 6 + 1] = unsigned(4 * i + 1);
    element_indices[i * 6 + 2] = unsigned(4 * i + 2);
    element_indices[i * 6 + 3] = unsigned(4 * i + 2);
    element_indices[i * 6 + 4] = unsigned(4 * i + 3);
    element_indices[i * 6 + 5] = unsigned(4 * i + 0);
  }

  *element_indices_out = element_indices;
  *vertices_out = vertex_data;
}

void init_tilemap_gl_buffers(Tilemap *tm) {
  glGenVertexArrays(1, &tm->vao);
  glBindVertexArray(tm->vao);

  // element indices
  glGenBuffers(1, &tm->ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tm->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               tm->level_width * tm->level_height * 6 * sizeof(unsigned),
               tm->element_indices, GL_STATIC_DRAW);

  // vertex data
  glGenBuffers(1, &tm->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, tm->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(TileQuad) * tm->level_width * tm->level_height,
               tm->vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(tm->vao);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                        (void *)offsetof(TileVertex, texture_coords));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                        (void *)offsetof(TileVertex, position));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                        (void *)offsetof(TileVertex, color));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                        (void *)offsetof(TileVertex, texture_id));
  glEnableVertexAttribArray(3);
}

void draw_tilemap(Tilemap *tm) {
  glBindVertexArray(tm->vao);
  glDrawElements(GL_TRIANGLES, 6 * tm->level_height * tm->level_width,
                 GL_UNSIGNED_INT, 0);
}

Tilemap new_tilemap(const int width, const int height, const int map[]) {
  const float *vertices;
  const unsigned *element_indices;
  gen_vertices_and_elements_from_map(width, height, map, &vertices,
                                     &element_indices);
  Tilemap tm{width, height, map, vertices, element_indices, 0, 0, 0};

  init_tilemap_gl_buffers(&tm);

  return tm;
}
