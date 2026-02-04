#pragma once

#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "tuke_engine.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

// https://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/
//
// Billboards in the graphics sense. A quad, always facing the screen.
// Intended to live in world space. But could just as well be used in
// screen space as a special case.
//
// Worldspace billboards are things like health bars over an enemy's head,
// or interaction prompts over an object. These need to be moved into the
// camera's view space and then projected into clip space the same as
// any other object. View * projection is still passed as a uniform.
//
// Billboards have different model matrices. Strictly speaking, a model
// matrix is not computed at all. The vertex shader for a world space
// billboard will assemble the locations of the corners of the billboard
// using the center of the billboard and the sizes of its edges. To get
// the rotation right, the camera's right and up vectors can be used
// to transform into view space.
//
// The shaders for a billboard need center position, height/width, and a
// rotation per instance. Each instance will share the camera that they
// are viewed through, so camera right and up are passed as uniforms.
//
//
// TODO: Do these links belong here or in the particles file? If that ever exists
// Articles about blending and order:
// https://stackoverflow.com/a/62538277
//  Explanation that GPUs respect draw order.
//
// https://wikis.khronos.org/opengl/Primitive_Assembly#Primitive_order
//  Within a draw or a multidraw's sub-draw, if the drawing command is an Instanced Rendering command,
//  then all primitives in one instance are ordered before the primitives with larger gl_InstanceID values.

// OpenGL Rendering technique/shader
// TODO make a union when Vulkan supported
struct BillboardShader {
  u32 program;
  u32 camera_up_right_ubo;
  u32 vp_ubo;
  u32 vbo;
  u32 vao;
};

struct Billboard {
  glm::vec3 center_pos;

  // x = width, y = height
  glm::vec2 size;

  // CCW Rotation in radians (degrees?)
  f32 rotation;
};

struct BillboardManager {

  Billboard *billboards;
  BillboardShader shader;

  u32 size;     // Number of active billboards in the buffer
  u32 capacity; // Number of billboards we have room for
};

// Create a new BillboardManager with heap allocated memory.
// capacity is the number of bullboards to store. NOT the size in bytes.
// Pass 0 in case this is a screen space billboard manager with no camera.
// It is expected this vp_ubo is already bound to UNIFORM_BUFFER_LABEL_CAMERA_VP in all other shaders
// that use vp_ubo.
// Also, only works with OpenGL, without a more expanded UBO abstraction.
inline BillboardManager create_billboard_manager(u32 capacity, u32 vp_ubo) {

  BillboardManager billboard_manager;
  billboard_manager.billboards = (Billboard *)malloc(capacity * sizeof(Billboard));
  billboard_manager.capacity = capacity;
  billboard_manager.size = 0;

  // if OpenGL
  u32 program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_BILLBOARD_VERT, SHADER_HANDLE_COMMON_BILLBOARD_FRAG);

  u32 camera_up_right_ubo = create_opengl_ubo(sizeof(CameraUpRight), GL_DYNAMIC_DRAW);
  opengl_bind_ubo_to_block(program, camera_up_right_ubo, UNIFORM_BUFFER_LABEL_BILLBOARD_CAMERA_VECTORS,
                           "CameraUpRight");
  opengl_bind_ubo_to_block(program, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  billboard_manager.shader.program = program;
  billboard_manager.shader.camera_up_right_ubo = camera_up_right_ubo;
  billboard_manager.shader.vp_ubo = vp_ubo;
  billboard_manager.shader.vao = create_vao();
  billboard_manager.shader.vbo = allocate_vbo(capacity * sizeof(Billboard), GL_DYNAMIC_DRAW);

  init_opengl_vertex_layout(VERTEX_LAYOUT_BINDING0INSTANCE_VEC3_VEC2_FLOAT, billboard_manager.shader.vao,
                            &billboard_manager.shader.vbo, 1, 0);

  return billboard_manager;
}

inline void destroy_billboard_manager(BillboardManager *billboard_manager) {
  free(billboard_manager->billboards);
  billboard_manager->billboards = NULL;
}

// This assumes that the caller has already configured the VP matrix.
inline void render_billboards_opengl(const BillboardManager *billboard_manager, glm::mat4 view) {
  glUseProgram(billboard_manager->shader.program);
  glBindVertexArray(billboard_manager->shader.vao);

  // Camera up/right uniform.
  // There is always 1 single camera, so number of billboards is irrelevant in computing size to buffer.
  CameraUpRight camera_up_right;
  camera_up_right.right = glm::vec3(view[0][0], view[1][0], view[2][0]);
  camera_up_right.up = glm::vec3(view[0][1], view[1][1], view[2][1]);
  glBindBuffer(GL_UNIFORM_BUFFER, billboard_manager->shader.camera_up_right_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUpRight), &camera_up_right);

  // Buffer instance data.
  glBindBuffer(GL_ARRAY_BUFFER, billboard_manager->shader.vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, billboard_manager->size * sizeof(Billboard), billboard_manager->billboards);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 8, billboard_manager->size);
}

inline void clear_billboard_manager(BillboardManager *billboard_manager) { billboard_manager->size = 0; }

inline void push_billboard(BillboardManager *billboard_manager, Billboard billboard) {
  assert(billboard_manager->size < billboard_manager->capacity);
  billboard_manager->billboards[billboard_manager->size++] = billboard;
}
