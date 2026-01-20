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

// Currently, we are only supporting 2D textures.
// When we need to support 1D and 3D textures, cubemaps, and texture arrays, we will
// abstract them.

// TODO consider how to manage samplers as together/separate from textures
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

inline void opengl_texture_buffer_data(const OpenGLTexture *texture, const u8 *data) {
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, texture->format, texture->width, texture->height, 0, texture->format, GL_UNSIGNED_BYTE,
               data);
}

inline void opengl_texture_resize(OpenGLTexture *texture, u32 height, u32 width) {
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, texture->format, width, height, 0, texture->format, GL_UNSIGNED_BYTE, NULL);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Framebuffers ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// Still developing my mental model around framebuffers and textures.
// It seems like the right mental model is to not have any dedicated "framebuffer" wrappers.
// A framebuffer is just OpenGL glue for structuring attachments.
// It seems like the better model is to have structs that have a dedicated functionality,
// and they all keep their own framebuffer if necessary, and specialize its use.

// TODO think about how to manage high dynamic range rendering - might be necessary to
// allow for more formats in texture creation.
struct OpenGLFramebuffer {
  u32 fbo;
  OpenGLTexture texture;
};

// The framebuffers created here are empty and incomplete.
// It is necessary to attach textures in a separate call.
inline OpenGLFramebuffer create_opengl_framebuffer() {
  OpenGLFramebuffer framebuffer;
  glGenFramebuffers(1, &framebuffer.fbo);
  return framebuffer;
}

// Possibly may want to pass GL_COLOR_ATTACHMENT as an argument
inline void opengl_framebuffer_attach_texture2d(OpenGLFramebuffer *framebuffer, const OpenGLTexture *texture) {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
  }

  framebuffer->texture = *texture;
}

// TODO need to do some crazy stuff with framebuffers and textures to understand the right
// ownership model.
inline void opengl_framebuffer_resize(OpenGLFramebuffer *framebuffer, u32 height, u32 width) {
  // Resize texture
  // Assuming no change of format
  opengl_texture_resize(&framebuffer->texture, height, width);
}

// Render Target
// A struct containing a texture to render to and an FBO to manage the texture in the OpenGLDriver
struct OpenGLRenderTarget {
  u32 fbo;
  OpenGLTexture texture;
};

inline OpenGLRenderTarget create_opengl_render_target(u32 height, u32 width, GLenum format, GLenum type) {
  OpenGLRenderTarget render_target;

  glGenFramebuffers(1, &render_target.fbo);
  render_target.texture = create_opengl_texture2d(height, width, format, type);

  glBindFramebuffer(GL_FRAMEBUFFER, render_target.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_target.texture.texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
  }

  return render_target;
}

//////////////// Post processing ping pong ////////////////

//  Struct for holding two texture attachment only framebuffers
struct OpenGLDoubleBuffer {
  OpenGLRenderTarget targets[2];
  u32 height;
  u32 width;
};

inline OpenGLDoubleBuffer create_opengl_double_buffer(u32 height, u32 width, GLenum format, GLenum type) {
  OpenGLDoubleBuffer double_buffer;

  double_buffer.targets[0] = create_opengl_render_target(height, width, format, type);
  double_buffer.targets[1] = create_opengl_render_target(height, width, format, type);

  double_buffer.height = 0;
  double_buffer.width = 0;

  return double_buffer;
}

inline void opengl_double_buffer_resize(OpenGLDoubleBuffer *double_buffer, u32 height, u32 width) {
  if (double_buffer->width == width && double_buffer->height == height) {
    return;
  }

  double_buffer->width = width;
  double_buffer->height = height;

  opengl_texture_resize(&double_buffer->targets[0].texture, height, width);
  opengl_texture_resize(&double_buffer->targets[1].texture, height, width);
}

// TODO come up with a scheme for describing post processing effects
// They will be material like. Write fluid sim and see what is necessary.
// First, do some simple post processing with kernel effects and stuff.
struct OpenGLPostProcessing {
  OpenGLDoubleBuffer double_buffer;

  u32 num_effects;
  u32 effects[MAX_NUM_POST_PROCESSING_EFFECTS];
};

// Starting with the render in texture, perform post processing
inline const OpenGLTexture *opengl_post_process(OpenGLPostProcessing *post_processing, const OpenGLTexture *texture) {

  if (post_processing->num_effects == 0) {
    return texture;
  }

  u32 src_index = 0; // should always be 0 or 1

  for (u32 i = 0; i < post_processing->num_effects; i++) {
    u32 dst_index = src_index ^ 1;

    const OpenGLRenderTarget *src = &post_processing->double_buffer.targets[src_index];
    const OpenGLRenderTarget *dst = &post_processing->double_buffer.targets[dst_index];

    glBindTexture(GL_TEXTURE_2D, src->texture.texture);
    glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
    // TODO actually draw stuff

    src_index ^= 1;
  }

  return &post_processing->double_buffer.targets[src_index].texture;
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

//////////////////// Open GL Limits ////////////////////

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

inline OpenGLLimits opengl_limits_get() {
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
