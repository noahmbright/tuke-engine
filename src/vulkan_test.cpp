#include "common_shaders.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include <cstdio>
#include <cstring>

struct FloatUniformBuffer {
  float x;
};

void update_uniform_buffer(VulkanContext *context, VulkanBuffer uniform_buffer,
                           float x) {

  FloatUniformBuffer fub;
  fub.x = x;

  void *data;
  vkMapMemory(context->device, uniform_buffer.memory, 0, sizeof(fub), 0, &data);
  memcpy(data, &fub, sizeof(fub));
  vkUnmapMemory(context->device, uniform_buffer.memory);
}

void render_mesh(VkCommandBuffer command_buffer, uint32_t num_vertices,
                 VkPipeline graphics_pipeline, VkDeviceSize buffer_offset,
                 VkBuffer buffer, VkPipelineLayout pipeline_layout,
                 VkDescriptorSet descriptor_set) {

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphics_pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer, &buffer_offset);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
  vkCmdDraw(command_buffer, num_vertices, 1, 0, 0);
}

int main() {
  VulkanContext context = create_vulkan_context("Tuke");

  // clang-format off
  float triangle_vertices[] = {
      0.0f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 
      0.5f, 0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
      -0.5f, 0.5f, 0.0f,  0.0f, 0.0f, 1.0f
  };

  float square_vertices[] = {
      0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.33f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.67f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  };

  float unit_square_positions[] = {
      -0.5f,  0.5f, 0.0f,
      -0.5f, -0.5f, 0.0f,
       0.5f, -0.5f, 0.0f,
       0.5f,  0.5f, 0.0f,
  };

  uint16_t unit_square_indices[] = {
      0, 1, 2, 0, 2, 3,
  };

  float quad_positions[] = {
       0.5,  0.5,
      -0.5, -0.5
  };
  // clang-format on

  uint32_t stride = 6 * sizeof(float);
  uint32_t triangle_size = sizeof(triangle_vertices);
  uint32_t square_size = sizeof(square_vertices);
  uint32_t unit_square_positions_size = sizeof(unit_square_positions);
  uint32_t unit_square_indices_size = sizeof(unit_square_indices);
  uint32_t quad_positions_size = sizeof(quad_positions);
  uint32_t unit_square_indices_offset = triangle_size + square_size +
                                        unit_square_positions_size +
                                        quad_positions_size;
  uint32_t total_size = unit_square_indices_offset + unit_square_indices_size;

  VulkanBuffer staging_buffer =
      create_buffer(&context, BUFFER_TYPE_STAGING, total_size);
  VulkanBuffer vertex_buffer =
      create_buffer(&context, BUFFER_TYPE_VERTEX, total_size);
  VulkanBuffer uniform_buffer =
      create_buffer(&context, BUFFER_TYPE_UNIFORM, sizeof(FloatUniformBuffer));
  VulkanBuffer index_buffer =
      create_buffer(&context, BUFFER_TYPE_INDEX, unit_square_indices_size);

  write_to_vulkan_buffer(&context, triangle_vertices, triangle_size, 0,
                         staging_buffer);
  write_to_vulkan_buffer(&context, square_vertices, square_size, triangle_size,
                         staging_buffer);
  write_to_vulkan_buffer(&context, unit_square_positions,
                         unit_square_positions_size,
                         triangle_size + square_size, staging_buffer);
  write_to_vulkan_buffer(&context, unit_square_indices,
                         unit_square_indices_size, unit_square_indices_offset,
                         staging_buffer);

  VkDescriptorSetLayoutBinding ubo_layout_binding;
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  ubo_layout_binding.pImmutableSamplers = NULL;

  VkDescriptorPoolSize pool_size = {};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = 1;

  VkDescriptorSetLayout descriptor_set_layout =
      create_descriptor_set_layout(context.device, &ubo_layout_binding, 1);
  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(context.device, &pool_size, 1, 1);
  VkDescriptorSet descriptor_set = create_descriptor_set(
      context.device, descriptor_pool, descriptor_set_layout);
  update_uniform_descriptor_sets(context.device, uniform_buffer.buffer, 0,
                                 sizeof(FloatUniformBuffer), descriptor_set, 0);

  const uint32_t num_copy_regions = 4;
  VkBufferCopy copy_regions[num_copy_regions];
  copy_regions[0].size = triangle_size;
  copy_regions[0].dstOffset = 0;
  copy_regions[0].srcOffset = 0;
  copy_regions[1].size = square_size;
  copy_regions[1].dstOffset = triangle_size;
  copy_regions[1].srcOffset = triangle_size;
  copy_regions[2].size = unit_square_positions_size;
  copy_regions[2].dstOffset = triangle_size + square_size;
  copy_regions[2].srcOffset = triangle_size + square_size;
  copy_regions[3].size = quad_positions_size;
  copy_regions[3].dstOffset =
      triangle_size + square_size + unit_square_positions_size;
  copy_regions[3].srcOffset =
      triangle_size + square_size + unit_square_positions_size;

  VkBufferCopy index_copy_region;
  index_copy_region.dstOffset = 0;
  index_copy_region.size = unit_square_indices_size;
  index_copy_region.srcOffset = unit_square_indices_offset;

  VkCommandBuffer single_use_command_buffer =
      begin_single_use_command_buffer(&context);
  vkCmdCopyBuffer(single_use_command_buffer, staging_buffer.buffer,
                  vertex_buffer.buffer, num_copy_regions, copy_regions);
  vkCmdCopyBuffer(single_use_command_buffer, staging_buffer.buffer,
                  index_buffer.buffer, 1, &index_copy_region);
  end_single_use_command_buffer(&context, single_use_command_buffer);

  VkShaderModule vertex_shader_module = create_shader_module(
      context.device, simple_vert_spv, simple_vert_spv_size);
  VkShaderModule triangle_fragment_shader_module = create_shader_module(
      context.device, simple_frag_spv, simple_frag_spv_size);
  VkShaderModule square_fragment_shader_module = create_shader_module(
      context.device, square_frag_spv, square_frag_spv_size);

  ShaderStage vertex_shader_stage;
  vertex_shader_stage.module = vertex_shader_module;
  vertex_shader_stage.entry_point = "main";
  vertex_shader_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;

  ShaderStage triangle_fragment_shader_stage;
  triangle_fragment_shader_stage.module = triangle_fragment_shader_module;
  triangle_fragment_shader_stage.entry_point = "main";
  triangle_fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  ShaderStage square_fragment_shader_stage;
  square_fragment_shader_stage.module = square_fragment_shader_module;
  square_fragment_shader_stage.entry_point = "main";
  square_fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  const size_t number_binding_descriptions = 1;
  VkVertexInputBindingDescription
      binding_descriptions[number_binding_descriptions];
  binding_descriptions[0].stride = stride;
  binding_descriptions[0].binding = 0;
  binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  const size_t number_attribute_descriptions = 2;
  VkVertexInputAttributeDescription
      attribute_descriptions[number_attribute_descriptions];
  attribute_descriptions[0].binding = 0;
  attribute_descriptions[0].location = 0;
  attribute_descriptions[0].offset = 0;
  attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attribute_descriptions[1].binding = 0;
  attribute_descriptions[1].location = 1;
  attribute_descriptions[1].offset = 3 * sizeof(float);
  attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info =
      create_vertex_input_state(
          number_binding_descriptions, binding_descriptions,
          number_attribute_descriptions, attribute_descriptions);

  VkPipelineLayout pipeline_layout =
      create_pipeline_layout(context.device, &descriptor_set_layout, 1);

  ShaderStage triangle_shader_stages[] = {vertex_shader_stage,
                                          triangle_fragment_shader_stage};
  ShaderStage square_shader_stages[] = {vertex_shader_stage,
                                        square_fragment_shader_stage};

  PipelineConfig triangle_pipeline_config;
  triangle_pipeline_config.stages = triangle_shader_stages;
  triangle_pipeline_config.stage_count = 2;
  triangle_pipeline_config.vertex_input_state_create_info =
      &vertex_input_state_create_info;
  triangle_pipeline_config.render_pass = context.render_pass;
  triangle_pipeline_config.pipeline_layout = pipeline_layout;
  triangle_pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  triangle_pipeline_config.primitive_restart_enabled = VK_FALSE;
  triangle_pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
  triangle_pipeline_config.cull_mode = VK_CULL_MODE_NONE;
  triangle_pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  triangle_pipeline_config.sample_count_flag = VK_SAMPLE_COUNT_1_BIT;
  triangle_pipeline_config.blend_mode = BLEND_MODE_ALPHA;

  VkPipeline triangle_graphics_pipeline = create_graphics_pipeline(
      context.device, &triangle_pipeline_config, context.pipeline_cache);

  PipelineConfig square_pipeline_config;
  square_pipeline_config.stages = square_shader_stages;
  square_pipeline_config.stage_count = 2;
  square_pipeline_config.vertex_input_state_create_info =
      &vertex_input_state_create_info;
  square_pipeline_config.render_pass = context.render_pass;
  square_pipeline_config.pipeline_layout = pipeline_layout;
  square_pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  square_pipeline_config.primitive_restart_enabled = VK_FALSE;
  square_pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
  square_pipeline_config.cull_mode = VK_CULL_MODE_NONE;
  square_pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  square_pipeline_config.sample_count_flag = VK_SAMPLE_COUNT_1_BIT;
  square_pipeline_config.blend_mode = BLEND_MODE_ALPHA;

  VkPipeline square_graphics_pipeline = create_graphics_pipeline(
      context.device, &square_pipeline_config, context.pipeline_cache);

  VkOffset2D offset;
  offset.x = 0;
  offset.y = 0;
  ViewportState viewport_state =
      create_viewport_state(context.swapchain_extent, offset);

  while (!glfwWindowShouldClose(context.window)) {
    glfwPollEvents();
    float t = glfwGetTime();
    float x = fabs(sinf(t));

    if (!begin_frame(&context)) {
      printf("fuk\n");
      continue;
    }

    update_uniform_buffer(&context, uniform_buffer, x);

    VkCommandBuffer command_buffer = begin_command_buffer(&context);
    VkClearValue clear_value;
    clear_value.color = {{1.0, 0.0, 0.0, 1.0}};
    begin_render_pass(&context, command_buffer, clear_value,
                      viewport_state.scissor.offset);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    // triangle
    render_mesh(command_buffer, 3, triangle_graphics_pipeline, 0,
                vertex_buffer.buffer, pipeline_layout, descriptor_set);
    // square
    render_mesh(command_buffer, 6, square_graphics_pipeline, triangle_size,
                vertex_buffer.buffer, pipeline_layout, descriptor_set);

    vkCmdEndRenderPass(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer),
             "Failed to end command buffer");
    submit_and_present(&context, command_buffer);

    context.current_frame++;
    context.current_frame_index = context.current_frame % MAX_FRAMES_IN_FLIGHT;
  }

  // cleanup
  vkDeviceWaitIdle(context.device);
  vkDestroyPipelineLayout(context.device, pipeline_layout, NULL);
  vkDestroyPipeline(context.device, square_graphics_pipeline, NULL);
  vkDestroyPipeline(context.device, triangle_graphics_pipeline, NULL);
  vkDestroyDescriptorPool(context.device, descriptor_pool, NULL);
  vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, NULL);
  vkDestroyShaderModule(context.device, vertex_shader_module, NULL);
  vkDestroyShaderModule(context.device, triangle_fragment_shader_module, NULL);
  vkDestroyShaderModule(context.device, square_fragment_shader_module, NULL);
  destroy_vulkan_buffer(&context, uniform_buffer);
  destroy_vulkan_buffer(&context, index_buffer);
  destroy_vulkan_buffer(&context, staging_buffer);
  destroy_vulkan_buffer(&context, vertex_buffer);
  destroy_vulkan_context(&context);
  return 0;
}
