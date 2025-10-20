#include "generated_shader_utils.h"

VkShaderModule shader_modules[NUM_SHADER_HANDLES] = {0};

VkPipeline shader_handles_to_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                               ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle,
                                               VkPipelineLayout pipeline_layout) {

  VkShaderModule vertex_shader = shader_modules[vertex_shader_handle];
  VkShaderModule fragment_shader = shader_modules[fragment_shader_handle];
  assert(vertex_shader != VK_NULL_HANDLE);
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state =
      &generated_vulkan_vertex_layouts[generated_shader_specs[vertex_shader_handle]->vertex_layout_id];

  return create_default_graphics_pipeline(context, render_pass, vertex_shader, fragment_shader, vertex_input_state,
                                          pipeline_layout);
}
