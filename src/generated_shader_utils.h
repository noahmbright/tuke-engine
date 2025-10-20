#pragma once
#include "c_reflector_bringup.h"

// TODO need to include vulkan base after generated headers because opengl cruft
#include "vulkan_base.h"

inline void init_generated_shader_vk_modules(VkDevice device) {
  for (uint32_t i = 0; i < NUM_SHADER_HANDLES; i++) {
    shader_modules[i] =
        create_shader_module(device, generated_shader_specs[i]->spv, generated_shader_specs[i]->spv_size);
  }
}

inline void free_generated_shader_vk_modules(VkDevice device) {
  for (u32 i = 0; i < NUM_SHADER_HANDLES; i++) {
    vkDestroyShaderModule(device, shader_modules[i], NULL);
  }
}

VkPipeline shader_handles_to_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                               ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle,
                                               VkPipelineLayout pipeline_layout);
