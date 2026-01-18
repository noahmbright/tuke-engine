#pragma once

#include "glad/gl.h"
#include "tuke_engine.h"
#include <OpenGL/OpenGL.h>

#define MAX_NUM_VBOS (4)
#define MAX_NUM_POST_PROCESSING_EFFECTS (4)

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

inline u32 create_opengl_ubo(u32 size, u32 draw_mode) {
  u32 ubo;
  glGenBuffers(1, &ubo);
  assert(ubo != 0 && "Tried to bind null UBO");

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, size, NULL, draw_mode);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  return ubo;
}

//////////////////// Textures ////////////////////

// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
struct OpenGLTexture {
  u32 texture; // the u32 handle the driver returns to us
  u32 height;
  u32 width;

  // Internal format is how the data gets stored on the GPU. This could involve a
  // conversion between the CPU format and GPU format, which comes with a performance impact.
  // Keeping formats compatible is best for performance. Some WebGL implementations
  // don't even support all conversions.
  GLenum internal_format;

  // Format specifes the layout of the data on the CPU.
  GLenum format;

  // Type specifies the way the fields in the texture are laid out.
  // A standard choice is GL_UNSIGNED_BYTE.
  // If format == GL_RGBA, then using GL_UNSIGNED_BYTE would mean
  // that R, G, B, and A are each an unsigned byte in the GPU's memory.
  GLenum type;
};

// This constructor doesn't allow for configuring wrapping and min/mag settings. If needed,
// bind and set outside the caller, or write a new constructor.
// Currently, there is no need to provide specialized format and internal_format. Use the
// same format parameter for both.
inline OpenGLTexture create_opengl_texture2d(u32 height, u32 width, GLenum format, GLenum type) {
  OpenGLTexture res;
  res.height = height;
  res.width = width;
  res.format = format;
  res.internal_format = format;
  res.type = type;

  u32 texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 0, res.internal_format, width, height, 0, format, type, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  res.texture = texture;

  // FIXME: I hate that I am doing this if true thing
  // FIXME: Actually understand mipmaps, and when I use them
  if (true) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  return res;
}

inline void buffer_data_to_opengl_texture2d(const OpenGLTexture *texture, const u8 *data) {
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, texture->format, texture->width, texture->height, 0, texture->format, GL_UNSIGNED_BYTE,
               data);
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

//////////////// Framebuffers ////////////////

// TODO WTF am I doing?
// Need to build the ping pong abstraction. Need to see how FBs are actually used.
struct OpenGLFramebuffer {
  u32 fbo;
  OpenGLTexture texture;
};

// The framebuffers created here are empty, incomplete.
// It is necessary to attach textures in a separate call.
inline OpenGLFramebuffer create_opengl_framebuffer() {
  OpenGLFramebuffer framebuffer;
  glGenFramebuffers(1, &framebuffer.fbo);
  return framebuffer;
}

// possibly may want to pass GL_COLOR_ATTACHMENT as an argument
inline void opengl_framebuffer_attach_texture2d(OpenGLFramebuffer *framebuffer, const OpenGLTexture *texture) {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
  }

  framebuffer->texture = *texture;
}

inline void opengl_framebuffer_resize(OpenGLFramebuffer *framebuffer, u32 height, u32 width) {
  // Resize texture
  // Assuming no change of format
  OpenGLTexture *texture = &framebuffer->texture;
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, texture->format, width, height, 0, texture->format, GL_UNSIGNED_BYTE, NULL);
}

//////////////// Post processing ping pong ////////////////

struct OpenGLPostProcessing {
  OpenGLFramebuffer ping;
  OpenGLFramebuffer pong;
  bool is_ping;

  u32 height;
  u32 width;

  u32 num_effects;
  u32 effects[MAX_NUM_POST_PROCESSING_EFFECTS];
};

inline OpenGLPostProcessing create_opengl_post_processing() {
  OpenGLPostProcessing post_processing;

  post_processing.is_ping = true;
  post_processing.ping = create_opengl_framebuffer();
  post_processing.pong = create_opengl_framebuffer();
  post_processing.num_effects = 0;
  post_processing.height = 0;
  post_processing.width = 0;

  memset(&post_processing.effects, 0, sizeof(post_processing.effects));

  return post_processing;
}

inline void resize_opengl_post_processing(OpenGLPostProcessing *post_processing, u32 height, u32 width) {
  if (post_processing->width == width && post_processing->height == height) {
    return;
  }

  post_processing->width = width;
  post_processing->height = height;

  // opengl_framebuffer_resize(&post_processing->ping, height, width);
  //  opengl_framebuffer_resize(&post_processing->pong, height, width);
}

//////////////// Meshes and Materials ////////////////

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
  OpenGLTexture texture;
  u32 uniform;
  GLenum primitive;
};

inline OpenGLMaterial create_opengl_material(u32 program) {
  OpenGLMaterial material;
  material.program = program;
  material.uniform = 0;
  material.primitive = GL_TRIANGLES;
  memset(&material.texture, 0, sizeof(OpenGLTexture));

  return material;
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
  glBindTexture(GL_TEXTURE_2D, material.texture.texture);
  glBindBuffer(GL_UNIFORM_BUFFER, material.uniform);
  glUseProgram(material.program);
  glBindVertexArray(opengl_mesh->vao);
  glDrawArrays(GL_TRIANGLES, 0, opengl_mesh->num_vertices);
}

inline void draw_opengl_mesh_instanced(const OpenGLMesh *opengl_mesh, OpenGLMaterial material, u32 num_instances) {
  glBindTexture(GL_TEXTURE_2D, material.texture.texture);
  glBindBuffer(GL_UNIFORM_BUFFER, material.uniform);
  glUseProgram(material.program);
  glBindVertexArray(opengl_mesh->vao);
  glDrawArraysInstanced(material.primitive, 0, opengl_mesh->num_vertices, num_instances);
}
