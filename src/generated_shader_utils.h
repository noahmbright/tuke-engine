#pragma once
#include "c_reflector_bringup.h"

// TODO need to include vulkan base after generated headers because opengl cruft
#include "vulkan_base.h"

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

inline void init_opengl_vertex_layout(ShaderHandle shader_handle, GLuint vao, GLuint *vbos, u32 num_vbos, GLuint ebo) {
  assert(generated_shader_specs[shader_handle]->stage == SHADER_STAGE_VERTEX);
  generated_opengl_vertex_array_initializers[generated_shader_specs[shader_handle]->vertex_layout_id](vao, vbos,
                                                                                                      num_vbos, ebo);
}

u32 link_shader_program(const char *vertex_shader_source, const char *fragment_shader_source);
VkPipeline shader_handles_to_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                               ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle,
                                               VkPipelineLayout pipeline_layout);

inline u32 shader_handles_to_opengl_program(ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle) {
  return link_shader_program(generated_shader_specs[vertex_shader_handle]->opengl_glsl,
                             generated_shader_specs[fragment_shader_handle]->opengl_glsl);
}
