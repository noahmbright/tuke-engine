#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "camera.h"
#include "tilemap.h"

#include "glm/gtc/type_ptr.hpp"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

static void gen_vertices_and_elements_from_map(Tilemap *tm,
                                               TilemapOpenGLRenderData *data) {
  const int height = tm->level_height;
  const int width = tm->level_width;
  const int *map = tm->level_map;

  const float aspect = (float)width / (float)height;
  const float dx = 2.0 / width * aspect;
  const float dy = 2.0 / height;

  const unsigned long long num_quads = width * height;

  float *vertex_data = (float *)malloc(num_quads * sizeof(TileQuad));
  unsigned *element_indices =
      (unsigned *)malloc(6 * num_quads * sizeof(unsigned));

  for (unsigned long long i = 0; i < num_quads; i++) {
    // positions
    unsigned long long height_index = i / width;
    unsigned long long width_index = i % width;
    float x0 = -1.0 + width_index * dx;
    float y0 = 1.0 - height_index * dy;

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

  data->element_indices = element_indices;
  data->vertices = vertex_data;
}

TilemapOpenGLRenderData new_tilemap_opengl_render_data(unsigned program,
                                                       Tilemap *tm) {
  TilemapOpenGLRenderData data;

  data.num_quads = tm->level_width * tm->level_height;
  gen_vertices_and_elements_from_map(tm, &data);

  data.program = program;
  data.u_model_location = glGetUniformLocation(program, "u_model");

  data.matrices_buffer_index = glGetUniformBlockIndex(program, "u_Matrices");
  glUniformBlockBinding(program, data.matrices_buffer_index,
                        CAMERA_MATRICES_INDEX);

  glGenVertexArrays(1, &data.vao);
  glBindVertexArray(data.vao);

  // element indices
  glGenBuffers(1, &data.ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.num_quads * 6 * sizeof(unsigned),
               data.element_indices, GL_STATIC_DRAW);

  // vertex data
  glGenBuffers(1, &data.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(TileQuad) * data.num_quads,
               data.vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(data.vao);
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

  return data;
}

void opengl_draw_tilemap(const TilemapOpenGLRenderData *tm,
                         const glm::mat4 &model) {
  glUseProgram(tm->program);
  glBindVertexArray(tm->vao);
  glUniformMatrix4fv(tm->u_model_location, 1, GL_FALSE, glm::value_ptr(model));
  glDrawElements(GL_TRIANGLES, 6 * tm->num_quads, GL_UNSIGNED_INT, 0);
}

Tilemap new_tilemap(const int width, const int height, int *map) {
  Tilemap tm;
  tm.level_width = width;
  tm.level_height = height;
  tm.level_map = map;

  return tm;
}
