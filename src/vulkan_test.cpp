#include "common_shaders.h"
#include "glm/vec3.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include <cstdio>
#include <cstring>

int main() {
  fprintf(stderr, "starting\n");
  VulkanContext context = create_vulkan_context();
  fprintf(stderr, "created context\n");

  uint64_t frame_count = 0;
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
  pipeline_layout_create_info.setLayoutCount = 0,
  pipeline_layout_create_info.pSetLayouts = NULL,
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

  fprintf(stderr, "waiting for fences...\n");
  vkWaitForFences(context.device, 1,
                  &context.frame_sync_objects[frame_count].in_flight_fence,
                  VK_TRUE, UINT64_MAX);
  fprintf(stderr, "waited for fences\n");
  vkResetFences(context.device, 1,
                &context.frame_sync_objects[frame_count].in_flight_fence);
  fprintf(stderr, "reset fences\n");

  uint32_t image_index;
  vkAcquireNextImageKHR(
      context.device, context.swapchain, UINT64_MAX,
      context.frame_sync_objects[frame_count].image_available_semaphore,
      VK_NULL_HANDLE, &image_index);
  fprintf(stderr, "Acquired next image\n");
  VkCommandBuffer command_buffer =
      context.graphics_command_buffers[image_index];
  VkFramebuffer framebuffer = context.framebuffers[image_index];

  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = NULL;
  command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_begin_info.pInheritanceInfo = NULL;

  VkRenderPassBeginInfo render_pass_begin_info;
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.pNext = NULL;
  render_pass_begin_info.renderPass = context.render_pass;
  render_pass_begin_info.framebuffer = framebuffer;
  render_pass_begin_info.renderArea.offset.x = 0.0f;
  render_pass_begin_info.renderArea.offset.y = 0.0f;
  render_pass_begin_info.renderArea.extent = context.swapchain_extent;
  render_pass_begin_info.clearValueCount = 1;
  VkClearValue clear_value;
  float clear_value_array[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  memcpy(&clear_value.color, clear_value_array, 4);
  render_pass_begin_info.pClearValues = &clear_value;

  VkViewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)context.swapchain_extent.width;
  viewport.height = (float)context.swapchain_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.extent = context.swapchain_extent;
  scissor.offset.x = 0;
  scissor.offset.y = 0;

  vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
  fprintf(stderr, "begun command buffer");
  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphics_pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 2, buffers, buffer_offsets);
  vkCmdDraw(command_buffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(command_buffer);
  vkEndCommandBuffer(command_buffer);
  fprintf(stderr, "ended command buffer\n");

  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores =
      &context.frame_sync_objects[frame_count].image_available_semaphore;
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores =
      &context.frame_sync_objects[frame_count].render_finished_semaphore;

  vkQueueSubmit(context.graphics_queue, 1, &submit_info,
                context.frame_sync_objects[frame_count].in_flight_fence);
  fprintf(stderr, "submitted\n");

  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &context.frame_sync_objects[frame_count].render_finished_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &context.swapchain;
  present_info.pImageIndices = &image_index;
  present_info.pResults = NULL;

  vkQueuePresentKHR(context.graphics_queue, &present_info);
  fprintf(stderr, "presented\n");

  while (!glfwWindowShouldClose(context.window)) {
    uint8_t frame_index = frame_count % MAX_FRAMES_IN_FLIGHT;
    glfwPollEvents();
    frame_count++;
    (void)frame_index;
  }

  // cleanup
  vkDeviceWaitIdle(context.device);
  vkDestroyPipelineLayout(context.device, pipeline_layout, NULL);
  vkDestroyPipeline(context.device, graphics_pipeline, NULL);
  vkDestroyShaderModule(context.device, vertex_shader_module, NULL);
  vkDestroyShaderModule(context.device, fragment_shader_module, NULL);
  destroy_vulkan_buffer(&context, staging_buffer);
  destroy_vulkan_buffer(&context, vertex_buffer);
  destroy_vulkan_context(&context);
  return 0;
}
