#pragma once
#include "c_reflector_bringup.h"

// TODO need to include vulkan base after generated headers because opengl cruft
#include "opengl_base.h"
#include "vulkan_base.h"

u32 link_shader_program(const char *vertex_shader_source, const char *fragment_shader_source);

VkPipeline shader_handles_to_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                               ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle,
                                               VkPipelineLayout pipeline_layout);

// inline helpers
// init generated modules
inline void init_generated_shader_vk_modules(VkDevice device) {
  for (uint32_t i = 0; i < NUM_SHADER_HANDLES; i++) {
    shader_modules[i] =
        create_shader_module(device, generated_shader_specs[i]->spv, generated_shader_specs[i]->spv_size);
  }
}

// destroy generated modules
inline void free_generated_shader_vk_modules(VkDevice device) {
  for (u32 i = 0; i < NUM_SHADER_HANDLES; i++) {
    vkDestroyShaderModule(device, shader_modules[i], NULL);
  }
}

inline void init_opengl_vertex_layout(VertexLayoutID vertex_layout_id, GLuint vao, GLuint *vbos, u32 num_vbos,
                                      GLuint ebo) {
  if (generated_opengl_vertex_array_initializers[vertex_layout_id]) {
    generated_opengl_vertex_array_initializers[vertex_layout_id](vao, vbos, num_vbos, ebo);
  }
}

inline u32 shader_handles_to_opengl_program(ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle) {
  return link_shader_program(generated_shader_specs[vertex_shader_handle]->opengl_glsl,
                             generated_shader_specs[fragment_shader_handle]->opengl_glsl);
}

inline OpenGLMesh create_opengl_mesh_with_vertex_layout(const f32 *arr, f32 num_f32s, u32 num_vertices,
                                                        VertexLayoutID vertex_layout_id, u32 draw_mode) {
  OpenGLMesh opengl_mesh = create_opengl_mesh(arr, num_f32s, num_vertices, draw_mode);
  init_opengl_vertex_layout(vertex_layout_id, opengl_mesh.vao, &opengl_mesh.vbo, 1, 0);
  return opengl_mesh;
}
