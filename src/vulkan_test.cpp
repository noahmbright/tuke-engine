#include "common_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "renderer.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include <cstdio>
#include <cstring>

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

struct MVPUniform {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

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

  //float unit_square_positions[] = {
  //    -0.5f,  0.5f, 0.0f,
  //    -0.5f, -0.5f, 0.0f,
  //     0.5f, -0.5f, 0.0f,
  //     0.5f,  0.5f, 0.0f,
  //};

  float unit_square_positions[] = {
      -0.5f,  0.5f, 0.0f,
      -0.5f, -0.5f, 0.0f,
       0.5f, -0.5f, 0.0f,

      -0.5f,  0.5f, 0.0f,
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

  uint32_t triangle_size = sizeof(triangle_vertices);
  uint32_t square_size = sizeof(square_vertices);
  uint32_t unit_square_positions_size = sizeof(unit_square_positions);
  uint32_t unit_square_indices_size = sizeof(unit_square_indices);
  uint32_t quad_positions_size = sizeof(quad_positions);
  uint32_t total_size = triangle_size + square_size +
                        unit_square_positions_size + quad_positions_size +
                        sizeof(unit_square_indices);

  VulkanBuffer vertex_buffer =
      create_buffer(&context, BUFFER_TYPE_VERTEX, total_size);
  VulkanBuffer index_buffer =
      create_buffer(&context, BUFFER_TYPE_INDEX, unit_square_indices_size);

  StagingArena staging_arena = create_staging_arena(&context, total_size);
  uint32_t triangle_offset = STAGE_ARRAY(
      &context, &staging_arena, triangle_vertices, vertex_buffer.buffer);
  uint32_t square_offset = STAGE_ARRAY(&context, &staging_arena,
                                       square_vertices, vertex_buffer.buffer);
  uint32_t unit_square_position_offset = STAGE_ARRAY(
      &context, &staging_arena, unit_square_positions, vertex_buffer.buffer);
  uint32_t unit_square_indices_offset = STAGE_ARRAY(
      &context, &staging_arena, unit_square_indices, vertex_buffer.buffer);
  flush_staging_arena(&context, &staging_arena);
  (void)unit_square_position_offset;
  (void)unit_square_indices_offset;

  VkDescriptorPoolSize x_pool_size = {};
  x_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  x_pool_size.descriptorCount = 1;

  VkDescriptorPoolSize mvp_pool_size = {};
  mvp_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  mvp_pool_size.descriptorCount = 1;

  VkDescriptorPool x_descriptor_pool =
      create_descriptor_pool(context.device, &x_pool_size, 1, 1);
  VkDescriptorPool mvp_descriptor_pool =
      create_descriptor_pool(context.device, &mvp_pool_size, 1, 1);

  UniformBuffer x_uniform_buffer =
      create_uniform_buffer(&context, sizeof(float), x_descriptor_pool);
  UniformBuffer mvp_uniform_buffer =
      create_uniform_buffer(&context, sizeof(MVPUniform), mvp_descriptor_pool);

  VkShaderModule vertex_shader_module = create_shader_module(
      context.device, simple_vert_spv, simple_vert_spv_size);
  VkShaderModule instanced_quad_vertex_shader_module = create_shader_module(
      context.device, instanced_quad_vert_spv, instanced_quad_vert_spv_size);
  VkShaderModule triangle_fragment_shader_module = create_shader_module(
      context.device, simple_frag_spv, simple_frag_spv_size);
  VkShaderModule square_fragment_shader_module = create_shader_module(
      context.device, square_frag_spv, square_frag_spv_size);
  VkShaderModule instanced_quad_fragment_shader_module = create_shader_module(
      context.device, instanced_quad_frag_spv, instanced_quad_frag_spv_size);

  ShaderStage vertex_shader_stage = create_shader_stage(
      vertex_shader_module, VK_SHADER_STAGE_VERTEX_BIT, "main");
  ShaderStage instanced_quad_vertex_shader_stage = create_shader_stage(
      instanced_quad_vertex_shader_module, VK_SHADER_STAGE_VERTEX_BIT, "main");
  ShaderStage triangle_fragment_shader_stage = create_shader_stage(
      triangle_fragment_shader_module, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
  ShaderStage square_fragment_shader_stage = create_shader_stage(
      square_fragment_shader_module, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
  ShaderStage instanced_quad_fragment_shader_stage =
      create_shader_stage(instanced_quad_fragment_shader_module,
                          VK_SHADER_STAGE_FRAGMENT_BIT, "main");

  GraphicsPipelineStages triangle_stages;
  triangle_stages.vertex_shader = vertex_shader_stage;
  triangle_stages.fragment_shader = triangle_fragment_shader_stage;
  GraphicsPipelineStages square_stages;
  square_stages.vertex_shader = vertex_shader_stage;
  square_stages.fragment_shader = square_fragment_shader_stage;
  GraphicsPipelineStages instanced_quad_stages;
  instanced_quad_stages.vertex_shader = instanced_quad_vertex_shader_stage;
  instanced_quad_stages.fragment_shader = instanced_quad_fragment_shader_stage;

  VkPipelineLayout x_pipeline_layout = create_pipeline_layout(
      context.device, &x_uniform_buffer.descriptor_set_layout, 1);
  VkPipelineLayout mvp_pipeline_layout = create_pipeline_layout(
      context.device, &mvp_uniform_buffer.descriptor_set_layout, 1);

  PipelineConfig triangle_pipeline_config =
      create_default_graphics_pipeline_config(&context, triangle_stages,
                                              VERTEX_LAYOUT_POSITION_NORMAL,
                                              x_pipeline_layout);
  VkPipeline triangle_graphics_pipeline = create_graphics_pipeline(
      context.device, &triangle_pipeline_config, context.pipeline_cache);

  PipelineConfig square_pipeline_config =
      create_default_graphics_pipeline_config(&context, square_stages,
                                              VERTEX_LAYOUT_POSITION_NORMAL,
                                              x_pipeline_layout);
  VkPipeline square_graphics_pipeline = create_graphics_pipeline(
      context.device, &square_pipeline_config, context.pipeline_cache);

  PipelineConfig instanced_quad_pipeline_config =
      create_default_graphics_pipeline_config(&context, instanced_quad_stages,
                                              VERTEX_LAYOUT_POSITION,
                                              mvp_pipeline_layout);
  VkPipeline instanced_quad_graphics_pipeline = create_graphics_pipeline(
      context.device, &instanced_quad_pipeline_config, context.pipeline_cache);

  VkOffset2D offset;
  offset.x = 0;
  offset.y = 0;
  ViewportState viewport_state =
      create_viewport_state(context.swapchain_extent, offset);

  VkClearValue clear_value;
  clear_value.color = {{1.0, 0.0, 0.0, 1.0}};

  while (!glfwWindowShouldClose(context.window)) {
    glfwPollEvents();
    float t = glfwGetTime();
    float sint = sinf(t);
    float x = fabs(sint);

    MVPUniform mvp;
    mvp.model = glm::mat4(1.0f);
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));
    mvp.model = glm::translate(mvp.model, glm::vec3(1.0f, 0.0f, 0.0f));

    if (!begin_frame(&context)) {
      printf("fuk\n");
      continue;
    }

    write_to_uniform_buffer(&x_uniform_buffer, &x, sizeof(x));
    write_to_uniform_buffer(&mvp_uniform_buffer, &mvp, sizeof(mvp));

    VkCommandBuffer command_buffer = begin_command_buffer(&context);
    begin_render_pass(&context, command_buffer, clear_value,
                      viewport_state.scissor.offset);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    // triangle
    render_mesh(command_buffer, 3, triangle_graphics_pipeline, triangle_offset,
                vertex_buffer.buffer, x_pipeline_layout,
                x_uniform_buffer.descriptor_set);
    // square
    render_mesh(command_buffer, 6, square_graphics_pipeline, square_offset,
                vertex_buffer.buffer, x_pipeline_layout,
                x_uniform_buffer.descriptor_set);
    // unit square
    render_mesh(command_buffer, 6, instanced_quad_graphics_pipeline,
                unit_square_position_offset, vertex_buffer.buffer,
                mvp_pipeline_layout, mvp_uniform_buffer.descriptor_set);

    vkCmdEndRenderPass(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer),
             "Failed to end command buffer");
    submit_and_present(&context, command_buffer);

    context.current_frame++;
    context.current_frame_index = context.current_frame % MAX_FRAMES_IN_FLIGHT;
  }

  // cleanup
  vkDeviceWaitIdle(context.device);
  vkDestroyPipelineLayout(context.device, x_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, mvp_pipeline_layout, NULL);
  vkDestroyPipeline(context.device, square_graphics_pipeline, NULL);
  vkDestroyPipeline(context.device, triangle_graphics_pipeline, NULL);
  vkDestroyPipeline(context.device, instanced_quad_graphics_pipeline, NULL);
  destroy_uniform_buffer(&context, &x_uniform_buffer);
  destroy_uniform_buffer(&context, &mvp_uniform_buffer);
  vkDestroyDescriptorPool(context.device, x_descriptor_pool, NULL);
  vkDestroyDescriptorPool(context.device, mvp_descriptor_pool, NULL);
  vkDestroyShaderModule(context.device, vertex_shader_module, NULL);
  vkDestroyShaderModule(context.device, triangle_fragment_shader_module, NULL);
  vkDestroyShaderModule(context.device, square_fragment_shader_module, NULL);
  vkDestroyShaderModule(context.device, instanced_quad_vertex_shader_module,
                        NULL);
  vkDestroyShaderModule(context.device, instanced_quad_fragment_shader_module,
                        NULL);
  destroy_vulkan_buffer(&context, index_buffer);
  destroy_vulkan_buffer(&context, staging_arena.buffer);
  destroy_vulkan_buffer(&context, vertex_buffer);
  destroy_vulkan_context(&context);
  return 0;
}
