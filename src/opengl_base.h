#pragma once

#include "glad/gl.h"
#include "tuke_engine.h"
#include <OpenGL/OpenGL.h>

#define MAX_NUM_VBOS (4)

enum VBOTypes { VBO_VERTEX, VBO_INSTANCE };

// TODO decide how to manage ownership of a VAO.
// A VAO is semi determined by the Mesh, which contains the data, which constrains how a shader can interpret its vertex
// input.
// A VAO is also determined by the shader, and multiple shaders can draw the same mesh.
// The practical issue is that identifying vertex layouts is harder than identifying a shader.
struct OpenGLMesh {
  u32 vao;

  u32 vbo; // Goal is to delete this vbo
  u32 vbos[MAX_NUM_VBOS];
  u32 num_vbos;

  u32 num_vertices;
};

inline u32 create_vbo() {
  u32 vbo;
  glGenBuffers(1, &vbo);
  return vbo;
}

inline u32 allocate_vbo(u32 num_bytes, u32 draw_mode) {
  u32 vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, num_bytes, NULL, draw_mode);
  return vbo;
}

inline u32 allocate_vbo_with_data(const void *arr, u32 num_bytes, u32 draw_mode) {
  u32 vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, num_bytes, arr, draw_mode);
  return vbo;
}

inline u32 create_vao() {
  u32 vao;
  glGenVertexArrays(1, &vao);
  return vao;
}

inline OpenGLMesh create_opengl_mesh(const void *arr, u32 num_bytes, u32 num_vertices, u32 draw_mode) {
  OpenGLMesh opengl_mesh;

  opengl_mesh.vbo = allocate_vbo_with_data(arr, num_bytes, draw_mode);
  opengl_mesh.vao = create_vao();
  opengl_mesh.num_vertices = num_vertices;

  return opengl_mesh;
}

// OpenGLMaterial
struct OpenGLMaterial {
  u32 program;
  u32 texture;
  u32 uniform;
  GLenum primitive;
};

inline OpenGLMaterial create_opengl_material(u32 program) {
  return {
      .program = program,
      .texture = 0,
      .uniform = 0,
      .primitive = GL_TRIANGLES,
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

inline void draw_opengl_mesh(const OpenGLMesh *opengl_mesh, OpenGLMaterial material) {
  glBindTexture(GL_TEXTURE_2D, material.texture);
  glBindBuffer(GL_UNIFORM_BUFFER, material.uniform);
  glUseProgram(material.program);
  glBindVertexArray(opengl_mesh->vao);
  glDrawArrays(GL_TRIANGLES, 0, opengl_mesh->num_vertices);
}

inline void draw_opengl_mesh_instanced(const OpenGLMesh *opengl_mesh, OpenGLMaterial material, u32 num_instances) {
  glBindTexture(GL_TEXTURE_2D, material.texture);
  glBindBuffer(GL_UNIFORM_BUFFER, material.uniform);
  glUseProgram(material.program);
  glBindVertexArray(opengl_mesh->vao);
  glDrawArraysInstanced(material.primitive, 0, opengl_mesh->num_vertices, num_instances);
}

// textures

struct OpenGLTextureConfig {
  u32 height;
  u32 width;
  GLenum internal_format; // e.g. GL_RGBA8
  GLenum format;          // e.g. GL_RGBA
  GLenum type;            // e.g. GL_UNSIGNED_BYTE
  GLenum min_filter;      // GL_LINEAR, GL_NEAREST, etc
  GLenum mag_filter;
  GLenum wrap_s;
  GLenum wrap_t;
  bool generate_mipmaps;
};

inline OpenGLTextureConfig create_default_opengl_texture_config(u32 height, u32 width) {
  return {
      .height = height,
      .width = width,
      .internal_format = GL_RGBA8,
      .format = GL_RGBA,
      .type = GL_UNSIGNED_BYTE,
      .min_filter = GL_LINEAR,
      .mag_filter = GL_NEAREST,
      .wrap_s = GL_CLAMP_TO_EDGE,
      .wrap_t = GL_CLAMP_TO_EDGE,
      .generate_mipmaps = false,
  };
}

inline u32 create_opengl_texture2d(const OpenGLTextureConfig *config) {
  u32 texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 0, config->internal_format, config->width, config->height, 0, config->format,
               GL_UNSIGNED_BYTE, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, config->wrap_s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, config->wrap_t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config->min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config->mag_filter);

  if (config->generate_mipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  return texture;
}

// opengl limits

struct OpenGLLimits {
  i32 max_uniform_buffer_bindings;
  i32 max_uniform_block_size;
  i32 max_vertex_uniform_blocks;
  i32 max_fragment_uniform_blocks;
  i32 max_combined_uniform_blocks;
  i32 max_combined_vertex_uniform_components;
  i32 max_vertex_attribs;
  i32 max_texture_units;
  i32 max_texture_size;
};

inline OpenGLLimits get_opengl_limits() {
  OpenGLLimits limits;
  glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &limits.max_uniform_buffer_bindings);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &limits.max_uniform_block_size);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &limits.max_vertex_uniform_blocks);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &limits.max_fragment_uniform_blocks);
  glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &limits.max_combined_uniform_blocks);
  glGetIntegerv(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, &limits.max_combined_vertex_uniform_components);

  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limits.max_vertex_attribs);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limits.max_texture_units);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.max_texture_size);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf(stderr, "get_opengl_limits: glGetIntegerv failed with error 0x%x\n", err);
  }

  return limits;
}

// framebuffers

inline u32 create_opengl_framebuffer() {
  u32 fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  return fbo;
}

// possibly may want to pass GL_COLOR_ATTACHMENTN as an argument
inline void opengl_attach_texture2d_to_framebuffer(u32 fbo, u32 texture) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
  }
}
