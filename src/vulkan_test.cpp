#include "common_shaders.h"
#include "glm/vec3.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include <cstdio>
#include <cstring>

int main() {
  VulkanContext context = create_vulkan_context();

  float positions[] = {0.0f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f, -0.5f, 0.5f, 0.0f};
  float normals[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  uint32_t position_normal_stride = 3 * sizeof(float);
  uint32_t positions_size = sizeof(positions);
  uint32_t normals_size = sizeof(normals);
  uint32_t total_size = positions_size + normals_size;

  VkBufferUsageFlags staging_buffer_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  VkMemoryPropertyFlags staging_buffer_memory_properties =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  VulkanBuffer staging_buffer =
      create_buffer(&context, staging_buffer_usage, total_size,
                    staging_buffer_memory_properties);
  write_to_vulkan_buffer(&context, positions, positions_size, 0,
                         staging_buffer);
  write_to_vulkan_buffer(&context, normals, normals_size, positions_size,
                         staging_buffer);

  VkBufferUsageFlags vertex_buffer_usage =
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkMemoryPropertyFlags vertex_buffer_memory_properties =
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VulkanBuffer vertex_buffer =
      create_buffer(&context, vertex_buffer_usage, total_size,
                    vertex_buffer_memory_properties);

  struct FloatUniformBuffer {
    float x;
  };

  VkBufferUsageFlags uniform_buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  VkMemoryPropertyFlags uniform_buffer_memory_properties =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  VulkanBuffer uniform_buffer =
      create_buffer(&context, uniform_buffer_usage, sizeof(FloatUniformBuffer),
                    uniform_buffer_memory_properties);

  VkDescriptorSetLayoutBinding ubo_layout_binding;
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  ubo_layout_binding.pImmutableSamplers = NULL;

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
  descriptor_set_layout_create_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_layout_create_info.pNext = NULL;
  descriptor_set_layout_create_info.flags = 0;
  descriptor_set_layout_create_info.bindingCount = 1;
  descriptor_set_layout_create_info.pBindings = &ubo_layout_binding;
  vkCreateDescriptorSetLayout(context.device,
                              &descriptor_set_layout_create_info, NULL,
                              &descriptor_set_layout);

  VkDescriptorPoolSize pool_size = {};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = 1;

  VkDescriptorPool descriptor_pool;
  VkDescriptorPoolCreateInfo descriptor_pool_create_info;
  descriptor_pool_create_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.pNext = NULL;
  descriptor_pool_create_info.flags = 0;
  descriptor_pool_create_info.maxSets = 1;
  descriptor_pool_create_info.poolSizeCount = 1;
  descriptor_pool_create_info.pPoolSizes = &pool_size;
  vkCreateDescriptorPool(context.device, &descriptor_pool_create_info, NULL,
                         &descriptor_pool);

  VkDescriptorSet descriptor_set;
  VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
  descriptor_set_allocate_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_allocate_info.pNext = NULL;
  descriptor_set_allocate_info.descriptorPool = descriptor_pool;
  descriptor_set_allocate_info.descriptorSetCount = 1;
  descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout;
  vkAllocateDescriptorSets(context.device, &descriptor_set_allocate_info,
                           &descriptor_set);

  VkDescriptorBufferInfo descriptor_buffer_info;
  descriptor_buffer_info.buffer = uniform_buffer.buffer;
  descriptor_buffer_info.offset = 0;
  descriptor_buffer_info.range = sizeof(FloatUniformBuffer);

  VkWriteDescriptorSet write_descriptor_set;
  write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set.pNext = NULL;
  write_descriptor_set.dstSet = descriptor_set;
  write_descriptor_set.dstBinding = 0;
  write_descriptor_set.dstArrayElement = 0;
  write_descriptor_set.descriptorCount = 1;
  write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write_descriptor_set.pImageInfo = NULL;
  write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
  write_descriptor_set.pTexelBufferView = NULL;
  vkUpdateDescriptorSets(context.device, 1, &write_descriptor_set, 0, NULL);

  VkBufferCopy copy_regions[2] = {};
  copy_regions[0].size = positions_size;
  copy_regions[0].dstOffset = 0;
  copy_regions[0].srcOffset = 0;
  copy_regions[1].size = normals_size;
  copy_regions[1].dstOffset = positions_size;
  copy_regions[1].srcOffset = positions_size;

  VkCommandBuffer single_use_command_buffer =
      begin_single_use_command_buffer(&context);

  vkCmdCopyBuffer(single_use_command_buffer, staging_buffer.buffer,
                  vertex_buffer.buffer, 2, copy_regions);
  end_single_use_command_buffer(&context, single_use_command_buffer);

  VkShaderModule vertex_shader_module = create_shader_module(
      context.device, simple_vert_spv, simple_vert_spv_size);
  VkShaderModule fragment_shader_module = create_shader_module(
      context.device, simple_frag_spv, simple_frag_spv_size);

  const size_t number_binding_descriptions = 2;
  VkVertexInputBindingDescription
      binding_descriptions[number_binding_descriptions];
  binding_descriptions[0].stride = position_normal_stride;
  binding_descriptions[0].binding = 0;
  binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  binding_descriptions[1].stride = position_normal_stride;
  binding_descriptions[1].binding = 1;
  binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  const size_t number_attribute_descriptions = 2;
  VkVertexInputAttributeDescription
      attribute_descriptions[number_attribute_descriptions];
  attribute_descriptions[0].binding = 0;
  attribute_descriptions[0].location = 0;
  attribute_descriptions[0].offset = 0;
  attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attribute_descriptions[1].binding = 1;
  attribute_descriptions[1].location = 1;
  attribute_descriptions[1].offset = 0;
  attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
  vertex_input_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state_create_info.pNext = NULL;
  vertex_input_state_create_info.flags = 0;
  vertex_input_state_create_info.vertexBindingDescriptionCount =
      number_binding_descriptions;
  vertex_input_state_create_info.pVertexBindingDescriptions =
      binding_descriptions;
  vertex_input_state_create_info.vertexAttributeDescriptionCount =
      number_attribute_descriptions;
  vertex_input_state_create_info.pVertexAttributeDescriptions =
      attribute_descriptions;

  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
  VkSampleCountFlagBits sample_count_flag = VK_SAMPLE_COUNT_1_BIT;
  VkBool32 blending_enabled = VK_FALSE;
  VkCullModeFlagBits cull_mode = VK_CULL_MODE_NONE;

  // TODO this is default
  VkPipelineLayoutCreateInfo pipeline_layout_create_info;
  pipeline_layout_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  pipeline_layout_create_info.pNext = NULL,
  pipeline_layout_create_info.flags = 0,
  pipeline_layout_create_info.setLayoutCount = 1,
  pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout,
  pipeline_layout_create_info.pushConstantRangeCount = 0,
  pipeline_layout_create_info.pPushConstantRanges = NULL;

  VkPipelineLayout pipeline_layout;
  VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_create_info,
                                  NULL, &pipeline_layout),
           "create_graphics_pipeline: Failed to vkCreatePipelineLayout");

  VkPipelineShaderStageCreateInfo shader_stage_create_infos[2];
  shader_stage_create_infos[0].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_infos[0].pNext = NULL;
  shader_stage_create_infos[0].flags = 0;
  shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stage_create_infos[0].module = vertex_shader_module;
  shader_stage_create_infos[0].pName = "main";
  shader_stage_create_infos[0].pSpecializationInfo = NULL;
  shader_stage_create_infos[1].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_infos[1].pNext = NULL;
  shader_stage_create_infos[1].flags = 0;
  shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stage_create_infos[1].module = fragment_shader_module;
  shader_stage_create_infos[1].pName = "main";
  shader_stage_create_infos[1].pSpecializationInfo = NULL;

  VkPipeline graphics_pipeline = create_graphics_pipeline(
      context.device, shader_stage_create_infos,
      /*stage_count*/ 2, &vertex_input_state_create_info, topology,
      context.swapchain_extent, polygon_mode, sample_count_flag,
      blending_enabled, context.render_pass, pipeline_layout,
      context.pipeline_cache, cull_mode);

  VkDeviceSize buffer_offsets[] = {0, positions_size};
  VkBuffer buffers[] = {vertex_buffer.buffer, vertex_buffer.buffer};

  VkOffset2D offset;
  offset.x = 0;
  offset.y = 0;
  ViewportState viewport_state =
      create_viewport_state(context.swapchain_extent, offset);

  if (!begin_frame(&context)) {
    printf("fuk\n");
  }
  VkClearValue clear_value;
  clear_value.color = {{1.0, 0.0, 0.0, 1.0}};
  VkCommandBuffer command_buffer = begin_command_buffer(&context);
  begin_render_pass(&context, command_buffer, clear_value,
                    viewport_state.scissor.offset);

  FloatUniformBuffer fub;
  fub.x = 0.4f;

  void *data;
  vkMapMemory(context.device, uniform_buffer.memory, 0, sizeof(fub), 0, &data);
  memcpy(data, &fub, sizeof(fub));
  vkUnmapMemory(context.device, uniform_buffer.memory);

  vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphics_pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 2, buffers, buffer_offsets);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
  vkCmdDraw(command_buffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(command_buffer);
  VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
  submit_and_present(&context, command_buffer);

  while (!glfwWindowShouldClose(context.window)) {
    uint8_t frame_index = context.current_frame % MAX_FRAMES_IN_FLIGHT;
    glfwPollEvents();
    context.current_frame++;
    (void)frame_index;
  }

  // cleanup
  vkDeviceWaitIdle(context.device);
  vkDestroyPipelineLayout(context.device, pipeline_layout, NULL);
  vkDestroyPipeline(context.device, graphics_pipeline, NULL);
  vkDestroyDescriptorPool(context.device, descriptor_pool, NULL);
  vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, NULL);
  vkDestroyShaderModule(context.device, vertex_shader_module, NULL);
  vkDestroyShaderModule(context.device, fragment_shader_module, NULL);
  destroy_vulkan_buffer(&context, uniform_buffer);
  destroy_vulkan_buffer(&context, staging_buffer);
  destroy_vulkan_buffer(&context, vertex_buffer);
  destroy_vulkan_context(&context);
  return 0;
}
