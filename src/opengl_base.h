#pragma once

#include "glad/gl.h"
#include "tuke_engine.h"

struct OpenGLMesh {
  const f32 *data;
  u32 data_num_f32s;

  u32 vao, vbo;
  u32 num_vertices;
};

struct OpenGLMaterial {
  u32 program;
  u32 texture;
  u32 uniform;
};

inline OpenGLMaterial create_opengl_material(u32 program) {
  return {
      .program = program,
      .texture = 0,
      .uniform = 0,
  };
}

inline u32 create_opengl_ubo(u32 size, u32 draw_mode) {
  u32 ubo;
  glGenBuffers(1, &ubo);
  assert(ubo != 0 && "Tried to bind null UBO");
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, size, NULL, draw_mode);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  return ubo;
}

inline void opengl_material_add_uniform(OpenGLMaterial *opengl_material, u32 ubo, u32 binding_point,
                                        const char *block_name) {
  u32 block_index = glGetUniformBlockIndex(opengl_material->program, block_name);
  if (block_index == GL_INVALID_INDEX) {
    fprintf(stderr, "Uniform block '%s' not found in program %u\n", block_name, opengl_material->program);
    return;
  }
  opengl_material->uniform = ubo;

  glUniformBlockBinding(opengl_material->program, block_index, binding_point);
  glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, opengl_material->uniform);
}

inline u32 create_vao() {
  u32 vao;
  glGenVertexArrays(1, &vao);
  return vao;
}

inline u32 create_vbo() {
  u32 vbo;
  glGenBuffers(1, &vbo);
  return vbo;
}

inline u32 create_vbo_from_f32_array(const f32 *arr, f32 num_f32s, u32 draw_mode) {
  u32 vbo = create_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, num_f32s, arr, draw_mode);
  return vbo;
}

inline OpenGLMesh create_opengl_mesh(const f32 *arr, f32 num_f32s, u32 draw_mode) {
  OpenGLMesh opengl_mesh;

  opengl_mesh.data = arr;
  opengl_mesh.data_num_f32s = num_f32s;
  opengl_mesh.vbo = create_vbo_from_f32_array(arr, num_f32s, draw_mode);
  opengl_mesh.vao = create_vao();

  return opengl_mesh;
}

inline void draw_opengl_mesh(const OpenGLMesh *opengl_mesh, OpenGLMaterial material) {
  glBindTexture(GL_TEXTURE_2D, material.texture);
  glBindBuffer(GL_UNIFORM_BUFFER, material.uniform);
  glUseProgram(material.program);
  glBindVertexArray(opengl_mesh->vao);
  glDrawArrays(GL_TRIANGLES, 0, opengl_mesh->num_vertices);
}
