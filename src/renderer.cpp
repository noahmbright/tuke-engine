#include "renderer.h"
#include "shaders.h"

#include <OpenGL/OpenGL.h>
#define GLFW_INCLUDE_NONE
// https://www.glfw.org/docs/3.3/quick_guide.html
#include <GLFW/glfw3.h>
#include <glad/gl.h>

void draw_cube(RenderData *render_data) {
  glUseProgram(render_data->program);
  glBindVertexArray(render_data->vao);
  glDrawArrays(GL_TRIANGLES, 0, 36);
}

RenderData init_cube(unsigned program) {
  unsigned vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  unsigned vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  RenderData rd;
  rd.program = program;
  rd.vao = vao;
  rd.vbo = vbo;
  rd.ebo = 0;
  return rd;
}

void draw_square(const RenderData *render_data) {
  glUseProgram(render_data->program);
  glBindVertexArray(render_data->vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

RenderData init_square(unsigned program) {
  unsigned vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  unsigned vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  RenderData rd;
  rd.vao = vao;
  rd.program = program;
  rd.vbo = vbo;
  rd.ebo = 0;
  return rd;
}
