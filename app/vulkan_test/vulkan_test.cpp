#include "vulkan_test.h"
#include "compiled_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/glm.hpp"
#include "utils.h"
#include "vulkan_base.h"
#include <vulkan/vulkan_core.h>

int main() {
  VulkanContext context = create_vulkan_context("Tuke");
  init_vertex_layout_registry();
  VkDescriptorPool descriptor_pool = create_descriptor_pool(
      context.device, generated_pool_sizes, pool_size_count, max_sets);
  cache_shader_modules(context.shader_cache, generated_shader_specs,
                       num_generated_specs);
  ViewportState viewport_state =
      create_viewport_state_xy(context.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {
      {.color = {{1.0, 0.0, 0.0, 1.0}}}, {1.0f, 0.0f}};

  VulkanTexture textures[NUM_TEXTURES];
  load_vulkan_textures(&context, texture_names, NUM_TEXTURES, textures);
  VkSampler sampler = create_sampler(context.device);

  BufferUploadQueue buffer_upload_queue = new_buffer_upload_queue();
  const BufferHandle *triangle_vertices_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, triangle_vertices);
  const BufferHandle *square_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, square_vertices);
  const BufferHandle *unit_square_position_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, unit_square_positions);
  const BufferHandle *quad_position_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, quad_positions);
  const BufferHandle *unit_square_indices_slice =
      UPLOAD_INDEX_ARRAY(buffer_upload_queue, unit_square_indices);
  const BufferHandle *cube_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, cube_vertices);

  BufferManager buffer_manager = flush_buffers(&context, &buffer_upload_queue);
  VulkanBuffer *vertex_buffer = &buffer_manager.vertex_buffer;
  VulkanBuffer *index_buffer = &buffer_manager.index_buffer;

  UniformBufferManager ub_manager = new_uniform_buffer_manager();
  UniformWrite mvp_handle = push_uniform(&ub_manager, sizeof(MVPUniform));
  UniformWrite cube_model_handle = push_uniform(&ub_manager, sizeof(CubeModel));
  UniformWrite x_handle = push_uniform(&ub_manager, sizeof(f32));
  UniformBuffer global_uniform_buffer =
      create_uniform_buffer(&context, ub_manager.current_offset);

  // the uniform buffer will contain the layout: mvp | cube model | x
  // for simple.frag.in, x uniform is set/binding 0/0
  DescriptorSetBuilder x_set_builder = create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&x_set_builder, &global_uniform_buffer,
                                    x_handle.offset, x_handle.size, 0, 1,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, false);
  DescriptorSetHandle x_descriptor =
      build_descriptor_set(&x_set_builder, descriptor_pool);
  VkPipelineLayout x_pipeline_layout = create_pipeline_layout(
      context.device, &x_descriptor.descriptor_set_layout, 1);

  // instanced quad: the mvp uniform is set/binding 0/0, the float is 0/1
  DescriptorSetBuilder mvp_set_builder =
      create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &global_uniform_buffer,
                                    mvp_handle.offset, mvp_handle.size, 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &global_uniform_buffer,
                                    x_handle.offset, x_handle.size, 1, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_texture_descriptor_set(&mvp_set_builder, &textures[2], sampler, 2, 1,
                             VK_SHADER_STAGE_FRAGMENT_BIT);

  DescriptorSetHandle mvp_descriptor =
      build_descriptor_set(&mvp_set_builder, descriptor_pool);

  VkPipelineLayout mvp_pipeline_layout = create_pipeline_layout(
      context.device, &mvp_descriptor.descriptor_set_layout, 1);

  // cube descriptor set
  DescriptorSetBuilder cube_set_builder =
      create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(
      &cube_set_builder, &global_uniform_buffer, cube_model_handle.offset,
      cube_model_handle.size, 0, 1, VK_SHADER_STAGE_VERTEX_BIT, false);
  DescriptorSetHandle cube_descriptor =
      build_descriptor_set(&cube_set_builder, descriptor_pool);

  VkPipelineLayout cube_pipeline_layout = create_pipeline_layout(
      context.device, &cube_descriptor.descriptor_set_layout, 1);

  // create pipelines
  VkPipeline triangle_pipeline = create_default_graphics_pipeline(
      &context, simple_vert_spec.name, simple_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC3), x_pipeline_layout);

  VkPipeline square_pipeline = create_default_graphics_pipeline(
      &context, simple_vert_spec.name, square_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC3), x_pipeline_layout);

  VkPipeline instanced_quad_pipeline = create_default_graphics_pipeline(
      &context, instanced_quad_vert_spec.name, instanced_quad_frag_spec.name,
      get_vertex_layout(instanced_quad_vert_spec.vertex_layout_id),
      mvp_pipeline_layout);

  PipelineConfig cube_pipeline_config = create_default_graphics_pipeline_config(
      &context, cube_vert_spec.name, cube_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC3_VEC2), cube_pipeline_layout);
  cube_pipeline_config.cull_mode = VK_CULL_MODE_FRONT_BIT;
  VkPipeline cube_pipeline = create_graphics_pipeline(
      context.device, &cube_pipeline_config, context.pipeline_cache);

  RenderCall triangle_render_call;
  triangle_render_call.num_vertices = 3;
  triangle_render_call.instance_count = 1;
  triangle_render_call.graphics_pipeline = triangle_pipeline;
  triangle_render_call.num_vertex_buffers = 1;
  triangle_render_call.vertex_buffer_offsets[0] =
      triangle_vertices_slice->offset;
  triangle_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  triangle_render_call.pipeline_layout = x_pipeline_layout;
  triangle_render_call.descriptor_set = x_descriptor.descriptor_set;
  triangle_render_call.is_indexed = false;

  RenderCall square_render_call;
  square_render_call.num_vertices = 6;
  square_render_call.instance_count = 1;
  square_render_call.graphics_pipeline = square_pipeline;
  square_render_call.num_vertex_buffers = 1;
  square_render_call.vertex_buffer_offsets[0] = square_slice->offset;
  square_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  square_render_call.pipeline_layout = x_pipeline_layout;
  square_render_call.descriptor_set = x_descriptor.descriptor_set;
  square_render_call.is_indexed = false;

  RenderCall cube_render_call;
  cube_render_call.num_vertices = 36;
  cube_render_call.instance_count = 1;
  cube_render_call.graphics_pipeline = cube_pipeline;
  cube_render_call.num_vertex_buffers = 1;
  cube_render_call.vertex_buffer_offsets[0] = cube_slice->offset;
  cube_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  cube_render_call.pipeline_layout = cube_pipeline_layout;
  cube_render_call.descriptor_set = cube_descriptor.descriptor_set;
  cube_render_call.is_indexed = false;

  RenderCall instanced_render_call;
  instanced_render_call.num_indices = 6;
  instanced_render_call.instance_count = instanced_quad_count;
  instanced_render_call.graphics_pipeline = instanced_quad_pipeline;
  instanced_render_call.num_vertex_buffers = 2;
  instanced_render_call.vertex_buffer_offsets[0] =
      unit_square_position_slice->offset;
  instanced_render_call.vertex_buffer_offsets[1] = quad_position_slice->offset;
  instanced_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  instanced_render_call.vertex_buffers[1] = vertex_buffer->buffer;
  instanced_render_call.pipeline_layout = mvp_pipeline_layout;
  instanced_render_call.descriptor_set = mvp_descriptor.descriptor_set;
  instanced_render_call.index_buffer = index_buffer->buffer;
  instanced_render_call.index_buffer_offset = 0;
  instanced_render_call.is_indexed = true;

  MVPUniform mvp;
  mvp.projection = glm::mat4(1.0f);
  mvp.view = glm::mat4(1.0f);

  const glm::vec3 cube_translation_vector = {0.5f, -0.3f, 0.5f};
  const f32 root3 = 0.57735026919;
  const glm::vec3 cube_rotation_axis = {root3, root3, root3};

  while (!glfwWindowShouldClose(context.window)) {
    glfwPollEvents();
    float t = glfwGetTime();
    float sint = sinf(t);
    float x = fabs(sint);

    mvp.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 cube_model =
        glm::translate(glm::mat4(1.0f), cube_translation_vector);
    cube_model = glm::rotate(cube_model, t, cube_rotation_axis);
    cube_model = glm::scale(cube_model, {0.2f, 0.2f, 0.2f});

    if (!begin_frame(&context)) {
      fprintf(stderr, "Failed to begin frame\n");
      continue;
    }

    write_to_uniform_buffer(&global_uniform_buffer, &mvp, mvp_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &cube_model,
                            cube_model_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &x, x_handle);

    VkCommandBuffer command_buffer = begin_command_buffer(&context);
    begin_render_pass(&context, command_buffer, clear_values, NUM_ATTACHMENTS,
                      viewport_state.scissor.offset);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    render_mesh(command_buffer, &triangle_render_call);
    render_mesh(command_buffer, &square_render_call);
    render_mesh(command_buffer, &instanced_render_call);
    render_mesh(command_buffer, &cube_render_call);

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
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(context.device, &textures[i]);
  }

  destroy_descriptor_set_handle(context.device, &x_descriptor);
  destroy_descriptor_set_handle(context.device, &mvp_descriptor);
  destroy_descriptor_set_handle(context.device, &cube_descriptor);

  vkDestroyPipelineLayout(context.device, x_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, mvp_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, cube_pipeline_layout, NULL);
  vkDestroyPipeline(context.device, square_pipeline, NULL);
  vkDestroyPipeline(context.device, triangle_pipeline, NULL);
  vkDestroyPipeline(context.device, instanced_quad_pipeline, NULL);
  vkDestroyPipeline(context.device, cube_pipeline, NULL);

  destroy_uniform_buffer(&context, &global_uniform_buffer);

  vkDestroyDescriptorPool(context.device, descriptor_pool, NULL);

  destroy_buffer_manager(&buffer_manager);
  destroy_vulkan_context(&context);
  return 0;
}
