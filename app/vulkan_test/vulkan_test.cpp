#include "vulkan_test.h"
#include "common_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "hashmap.h"
#include "utils.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"

// TODO find decent abstraction for this
void render_mesh(VkCommandBuffer command_buffer, u32 num_vertices,
                 u32 instance_count, VkPipeline graphics_pipeline,
                 VkDeviceSize buffer_offset, VkBuffer buffer,
                 VkPipelineLayout pipeline_layout,
                 VkDescriptorSet descriptor_set) {

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphics_pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer, &buffer_offset);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &descriptor_set, 0, NULL);

  u32 first_vertex = 0;
  u32 first_instance = 0;
  vkCmdDraw(command_buffer, num_vertices, instance_count, first_vertex,
            first_instance);
}

struct MVPUniform {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

int main() {
  VulkanContext context = create_vulkan_context("Tuke");

  u32 instance_count = 6;

  const u32 num_textures = 1;
  const char *texture_names[num_textures] = {"textures/generic_girl.jpg"};
  VulkanTexture texture =
      load_vulkan_textures(&context, texture_names, num_textures);
  VkSampler sampler = create_sampler(context.device);

  // TODO find a way to stage all of these in a single call
  // TODO find a way to store offsets
  u32 triangle_size = sizeof(triangle_vertices);
  u32 square_size = sizeof(square_vertices);
  u32 unit_square_positions_size = sizeof(unit_square_positions);
  u32 unit_square_indices_size = sizeof(unit_square_indices);
  u32 quad_positions_size = sizeof(quad_positions);
  u32 total_size = triangle_size + square_size + unit_square_positions_size +
                   quad_positions_size + sizeof(unit_square_indices);

  VulkanBuffer vertex_buffer =
      create_buffer(&context, BUFFER_TYPE_VERTEX, total_size);
  VulkanBuffer index_buffer =
      create_buffer(&context, BUFFER_TYPE_INDEX, unit_square_indices_size);

  // TODO clean up buffering to several destinations
  StagingArena staging_arena = create_staging_arena(&context, total_size);
  u32 triangle_offset = STAGE_ARRAY(&context, &staging_arena, triangle_vertices,
                                    vertex_buffer.buffer);
  u32 square_offset = STAGE_ARRAY(&context, &staging_arena, square_vertices,
                                  vertex_buffer.buffer);
  u32 unit_square_position_offset = STAGE_ARRAY(
      &context, &staging_arena, unit_square_positions, vertex_buffer.buffer);
  u32 quad_positions_offset = STAGE_ARRAY(&context, &staging_arena,
                                          quad_positions, vertex_buffer.buffer);

  u32 unit_square_indices_offset =
      stage_data_explicit(&context, &staging_arena, unit_square_indices,
                          sizeof(unit_square_indices), index_buffer.buffer, 0);
  flush_staging_arena(&context, &staging_arena);
  (void)unit_square_indices_offset;

  // TODO eventually will want an abstraction over pools, potentially using
  // shader reflection data
  VkDescriptorPoolSize mvp_pool_sizes[2];
  mvp_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  mvp_pool_sizes[0].descriptorCount = 3;
  mvp_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  mvp_pool_sizes[1].descriptorCount = 1;

  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(context.device, mvp_pool_sizes, 2, 3);

  UniformBuffer x_uniform_buffer =
      create_uniform_buffer(&context, sizeof(float) + sizeof(MVPUniform));

  // TODO output some const string pound defines or something for names, and
  // cache a list of names or something
  cache_shader_module(context.shader_cache, simple_vert_spv_spec);
  cache_shader_module(context.shader_cache, simple_frag_spv_spec);
  cache_shader_module(context.shader_cache, square_frag_spv_spec);
  cache_shader_module(context.shader_cache, instanced_quad_frag_spv_spec);
  cache_shader_module(context.shader_cache, instanced_quad_vert_spv_spec);

  // the uniform buffer will contain the layout mvp | x
  // for the simple.frag.in, x uniform is set/binding 0/0
  DescriptorSetBuilder x_set_builder = create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&x_set_builder, &x_uniform_buffer,
                                    sizeof(MVPUniform), sizeof(float), 0, 1,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, false);
  VkDescriptorSet x_descriptor_set =
      build_descriptor_set(&x_set_builder, descriptor_pool);

  VkPipelineLayout x_pipeline_layout = create_pipeline_layout(
      context.device, &x_set_builder.descriptor_set_layout, 1);

  // instanced quad: the mvp uniform is set/binding 0,0, the float is 0/1
  DescriptorSetBuilder mvp_set_builder =
      create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &x_uniform_buffer, 0,
                                    sizeof(MVPUniform), 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &x_uniform_buffer,
                                    sizeof(MVPUniform), sizeof(float), 1, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_texture_descriptor_set(&mvp_set_builder, &texture, sampler, 2, 1,
                             VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDescriptorSet mvp_descriptor_set =
      build_descriptor_set(&mvp_set_builder, descriptor_pool);

  VkPipelineLayout mvp_pipeline_layout = create_pipeline_layout(
      context.device, &mvp_set_builder.descriptor_set_layout, 1);

  VertexLayoutBuilder instance_vertex_layout_builder =
      create_vertex_layout_builder();
  push_vertex_binding(&instance_vertex_layout_builder, 0, 5 * sizeof(float),
                      VK_VERTEX_INPUT_RATE_VERTEX);
  push_vertex_binding(&instance_vertex_layout_builder, 1, 2 * sizeof(float),
                      VK_VERTEX_INPUT_RATE_INSTANCE);
  push_vertex_attribute(&instance_vertex_layout_builder, 0, 0,
                        VK_FORMAT_R32G32B32_SFLOAT, 0);
  push_vertex_attribute(&instance_vertex_layout_builder, 1, 1,
                        VK_FORMAT_R32G32_SFLOAT, 0);
  push_vertex_attribute(&instance_vertex_layout_builder, 2, 0,
                        VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float));

  VkPipelineVertexInputStateCreateInfo instanced_quad_vertex_input_state =
      build_vertex_input_state(&instance_vertex_layout_builder);

  VkPipelineVertexInputStateCreateInfo postion_normal_vertex_layout =
      get_common_vertex_input_state(&context, VERTEX_LAYOUT_POSITION_NORMAL);

  VkPipeline triangle_pipeline = create_default_graphics_pipeline(
      &context, simple_vert_spv_spec.name, simple_frag_spv_spec.name,
      &postion_normal_vertex_layout, x_pipeline_layout);

  VkPipeline square_pipeline = create_default_graphics_pipeline(
      &context, simple_vert_spv_spec.name, square_frag_spv_spec.name,
      &postion_normal_vertex_layout, x_pipeline_layout);

  VkPipeline instanced_quad_pipeline = create_default_graphics_pipeline(
      &context, instanced_quad_vert_spv_spec.name,
      instanced_quad_frag_spv_spec.name, &instanced_quad_vertex_input_state,
      mvp_pipeline_layout);

  ViewportState viewport_state =
      create_viewport_state_xy(context.swapchain_extent, 0, 0);

  VkClearValue clear_value;
  clear_value.color = {{1.0, 0.0, 0.0, 1.0}};

  MVPUniform mvp;
  mvp.projection = glm::mat4(1.0f);
  mvp.view = glm::mat4(1.0f);

  while (!glfwWindowShouldClose(context.window)) {
    glfwPollEvents();
    float t = glfwGetTime();
    float sint = sinf(t);
    float x = fabs(sint);

    mvp.model = glm::mat4(1.0f);
    mvp.model = glm::scale(mvp.model, glm::vec3(0.5f));
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));

    if (!begin_frame(&context)) {
      fprintf(stderr, "Failed to begin frame\n");
      continue;
    }

    write_to_uniform_buffer(&x_uniform_buffer, &mvp, 0, sizeof(MVPUniform));
    write_to_uniform_buffer(&x_uniform_buffer, &x, sizeof(MVPUniform),
                            sizeof(x));

    VkCommandBuffer command_buffer = begin_command_buffer(&context);
    begin_render_pass(&context, command_buffer, clear_value,
                      viewport_state.scissor.offset);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    // triangle
    render_mesh(command_buffer, 3, 1, triangle_pipeline, triangle_offset,
                vertex_buffer.buffer, x_pipeline_layout, x_descriptor_set);
    // square
    render_mesh(command_buffer, 6, 1, square_pipeline, square_offset,
                vertex_buffer.buffer, x_pipeline_layout, x_descriptor_set);
    // unit square
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      instanced_quad_pipeline);

    VkDeviceSize offset0 = unit_square_position_offset;
    VkDeviceSize offset1 = quad_positions_offset;
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer.buffer,
                           &offset0);
    vkCmdBindVertexBuffers(command_buffer, 1, 1, &vertex_buffer.buffer,
                           &offset1);
    vkCmdBindIndexBuffer(command_buffer, index_buffer.buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            mvp_pipeline_layout, 0, 1, &mvp_descriptor_set, 0,
                            NULL);
    vkCmdDrawIndexed(command_buffer, 6, instance_count, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer),
             "Failed to end command buffer");
    submit_and_present(&context, command_buffer);

    context.current_frame++;
    context.current_frame_index = context.current_frame % MAX_FRAMES_IN_FLIGHT;
  }

  // cleanup
  vkDeviceWaitIdle(context.device);

  vkDestroySampler(context.device, sampler, NULL);
  destroy_vulkan_texture(context.device, &texture);

  destroy_descriptor_set_builder(&x_set_builder);
  destroy_descriptor_set_builder(&mvp_set_builder);

  vkDestroyPipelineLayout(context.device, x_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, mvp_pipeline_layout, NULL);
  vkDestroyPipeline(context.device, square_pipeline, NULL);
  vkDestroyPipeline(context.device, triangle_pipeline, NULL);
  vkDestroyPipeline(context.device, instanced_quad_pipeline, NULL);

  destroy_uniform_buffer(&context, &x_uniform_buffer);

  vkDestroyDescriptorPool(context.device, descriptor_pool, NULL);

  destroy_vulkan_buffer(&context, index_buffer);
  destroy_vulkan_buffer(&context, staging_arena.buffer);
  destroy_vulkan_buffer(&context, vertex_buffer);
  destroy_vulkan_context(&context);
  return 0;
}
