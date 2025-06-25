#pragma once

// clang-format off
const float square_vertices[] = {
    -0.5f, 0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    0.5f, 0.5f, 0.0f,

    -0.5f, 0.5f, 0.0f,
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f
};

const float cube_vertices[] = {
    -0.5f, -0.5f,  0.5f, 0.4, 0.0, 0.4,
    -0.5f,  0.5f,  0.5f, 0.4, 0.0, 0.4,
     0.5f,  0.5f,  0.5f, 0.4, 0.0, 0.4,
    -0.5f, -0.5f,  0.5f, 0.4, 0.0, 0.4,
     0.5f, -0.5f,  0.5f, 0.4, 0.0, 0.4,
     0.5f,  0.5f,  0.5f, 0.4, 0.0, 0.4,

    -0.5f, -0.5f, -0.5f, 0.0, 0.8, 0.1,
    -0.5f,  0.5f, -0.5f, 0.0, 0.8, 0.1,
     0.5f,  0.5f, -0.5f, 0.0, 0.8, 0.1,
    -0.5f, -0.5f, -0.5f, 0.0, 0.8, 0.1,
     0.5f, -0.5f, -0.5f, 0.0, 0.8, 0.1,
     0.5f,  0.5f, -0.5f, 0.0, 0.8, 0.1,

     0.5f, -0.5f, -0.5f, 0.5, 0.2, 0.0,
     0.5f, -0.5f,  0.5f, 0.5, 0.2, 0.0,
     0.5f,  0.5f,  0.5f, 0.5, 0.2, 0.0,
     0.5f, -0.5f, -0.5f, 0.5, 0.2, 0.0,
     0.5f,  0.5f, -0.5f, 0.5, 0.2, 0.0,
     0.5f,  0.5f,  0.5f, 0.5, 0.2, 0.0,

    -0.5f, -0.5f, -0.5f, 0.2, 0.2, 0.8,
    -0.5f, -0.5f,  0.5f, 0.2, 0.2, 0.8,
    -0.5f,  0.5f,  0.5f, 0.2, 0.2, 0.8,
    -0.5f, -0.5f, -0.5f, 0.2, 0.2, 0.8,
    -0.5f,  0.5f, -0.5f, 0.2, 0.2, 0.8,
    -0.5f,  0.5f,  0.5f, 0.2, 0.2, 0.8,

    -0.5f,  0.5f, -0.5f, 0.9, 0.9, 0.0,
    -0.5f,  0.5f,  0.5f, 0.9, 0.9, 0.0,
     0.5f,  0.5f,  0.5f, 0.9, 0.9, 0.0,
    -0.5f,  0.5f, -0.5f, 0.9, 0.9, 0.0,
     0.5f,  0.5f, -0.5f, 0.9, 0.9, 0.0,
     0.5f,  0.5f,  0.5f, 0.9, 0.9, 0.0,

    -0.5f, -0.5f, -0.5f, 0.1, 0.7, 0.4,
    -0.5f, -0.5f,  0.5f, 0.1, 0.7, 0.4,
     0.5f, -0.5f,  0.5f, 0.1, 0.7, 0.4,
    -0.5f, -0.5f, -0.5f, 0.1, 0.7, 0.4,
     0.5f, -0.5f, -0.5f, 0.1, 0.7, 0.4,
     0.5f, -0.5f,  0.5f, 0.1, 0.7, 0.4
};
// clang-format on

enum VertexLayoutID {
  VERTEX_LAYOUT_POSITION,
  VERTEX_LAYOUT_POSITION_NORMAL,
  VERTEX_LAYOUT_POSITION_NORMAL_UV,

  VERTEX_LAYOUT_COUNT
};

extern const void **active_vertex_layouts;

struct RenderData {
  unsigned program;
  unsigned vao;
  unsigned vbo;
  unsigned ebo;
};

void draw_cube(const RenderData *render_data);
RenderData init_cube(unsigned program);

void draw_square(const RenderData *);
RenderData init_square(unsigned program);
