#include "c_reflector_bringup.h"
#define GLM_ENABLE_EXPERIMENTAL

#include "generated_shader_utils.h"

#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/glm.hpp"
#include "glm/gtx/string_cast.hpp"
#include "tuke_engine.h"
#include "utils.h"
#include "vulkan_base.h"
#include "vulkan_test.h"
#include "window.h"

#include <stdalign.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

static const VkPipelineVertexInputStateCreateInfo empty_vertex_input_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = NULL,
};

glm::vec4 movement_direction_from_inputs(const Inputs *inputs) {
  glm::vec4 res{0.0f, 0.0f, 0.0f, 1.0f};

  if (inputs->key_inputs[INPUT_KEY_W]) {
    res.y = 1.0f;
  }

  if (inputs->key_inputs[INPUT_KEY_A]) {
    res.x = -1.0f;
  }

  if (inputs->key_inputs[INPUT_KEY_S]) {
    res.y = -1.0f;
  }

  if (inputs->key_inputs[INPUT_KEY_D]) {
    res.x = 1.0f;
  }

  return res;
}

int main() {
  VulkanContext context = create_vulkan_context("Tuke");
  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(context.device, generated_pool_sizes, pool_size_count, max_descriptor_sets);
  init_generated_shader_vk_modules(context.device);
  ViewportState viewport_state = create_viewport_state_xy(context.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {{.color = {{0.01, 0.01, 0.01, 1.0}}},
                                                      {.depthStencil = {.depth = 1.0f, .stencil = 0}}};

  VulkanTexture textures[NUM_TEXTURES];
  load_vulkan_textures(&context, texture_names, NUM_TEXTURES, textures);
  VkSampler sampler = create_sampler(context.device);

  ColorDepthFramebuffer offscreen_framebuffer =
      create_color_depth_framebuffer(&context, context.swapchain_extent, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_D32_SFLOAT);

  BufferUploadQueue buffer_upload_queue = new_buffer_upload_queue();
  const BufferHandle *triangle_vertices_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, triangle_vertices);
  const BufferHandle *square_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, square_vertices);
  const BufferHandle *unit_square_position_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, unit_square_positions);
  const BufferHandle *quad_position_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, quad_positions);
  const BufferHandle *unit_square_indices_slice = UPLOAD_INDEX_ARRAY(buffer_upload_queue, unit_square_indices);
  const BufferHandle *cube_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, cube_vertices);

  BufferManager buffer_manager = flush_buffers(&context, &buffer_upload_queue);
  VulkanBuffer *vertex_buffer = &buffer_manager.vertex_buffer;
  VulkanBuffer *index_buffer = &buffer_manager.index_buffer;

  UniformBufferManager ub_manager = new_uniform_buffer_manager();
  UniformWrite mvp_handle = push_uniform(&ub_manager, sizeof(MVPUniform));
  UniformWrite cube_model_handle = push_uniform(&ub_manager, sizeof(CubeModel));
  UniformWrite x_handle = push_uniform(&ub_manager, sizeof(UniformBufferObject));
  // FIXME manual padding
  // either need a new philosophy for managing UBs or a way to push up to next alignment
  push_uniform(&ub_manager, 12);
  UniformWrite light_position_handle = push_uniform(&ub_manager, sizeof(LightPosition));
  UniformWrite camera_vp_handle = push_uniform(&ub_manager, sizeof(CameraVP));
  UniformBuffer global_uniform_buffer = create_uniform_buffer(&context, ub_manager.current_offset);

  // for simple.frag.in, x uniform is set/binding 0/0
  DescriptorSetBuilder simple_set_builder = create_descriptor_set_builder(&context);

  add_uniform_buffer_descriptor_set(&simple_set_builder, &global_uniform_buffer, x_handle.offset, x_handle.size, 0, 1,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, false);
  add_uniform_buffer_descriptor_set(&simple_set_builder, &global_uniform_buffer, camera_vp_handle.offset,
                                    camera_vp_handle.size, 1, 1, VK_SHADER_STAGE_VERTEX_BIT, false);
  add_uniform_buffer_descriptor_set(&simple_set_builder, &global_uniform_buffer, light_position_handle.offset,
                                    light_position_handle.size, 2, 1, VK_SHADER_STAGE_VERTEX_BIT, false);
  DescriptorSetHandle simple__descriptor = build_descriptor_set(&simple_set_builder, descriptor_pool);
  VkPipelineLayout x_pipeline_layout =
      create_pipeline_layout(context.device, &simple__descriptor.descriptor_set_layout, 1);

  // instanced quad: the mvp uniform is set/binding 0/0, the float is 0/1
  DescriptorSetBuilder mvp_set_builder = create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &global_uniform_buffer, mvp_handle.offset, mvp_handle.size, 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_uniform_buffer_descriptor_set(&mvp_set_builder, &global_uniform_buffer, x_handle.offset, x_handle.size, 1, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_image_descriptor_set(&mvp_set_builder, textures[2].image_view, sampler, 2, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

  DescriptorSetHandle mvp_descriptor = build_descriptor_set(&mvp_set_builder, descriptor_pool);

  VkPipelineLayout mvp_pipeline_layout =
      create_pipeline_layout(context.device, &mvp_descriptor.descriptor_set_layout, 1);

  // cube descriptor set
  DescriptorSetBuilder cube_set_builder = create_descriptor_set_builder(&context);
  add_uniform_buffer_descriptor_set(&cube_set_builder, &global_uniform_buffer, cube_model_handle.offset,
                                    cube_model_handle.size, 0, 1, VK_SHADER_STAGE_VERTEX_BIT, false);
  add_uniform_buffer_descriptor_set(&cube_set_builder, &global_uniform_buffer, light_position_handle.offset,
                                    light_position_handle.size, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT, false);
  add_uniform_buffer_descriptor_set(&cube_set_builder, &global_uniform_buffer, camera_vp_handle.offset,
                                    camera_vp_handle.size, 2, 1, VK_SHADER_STAGE_VERTEX_BIT, false);
  DescriptorSetHandle cube_descriptor = build_descriptor_set(&cube_set_builder, descriptor_pool);

  VkPipelineLayout cube_pipeline_layout =
      create_pipeline_layout(context.device, &cube_descriptor.descriptor_set_layout, 1);

  // fullscreen quad descriptor set
  DescriptorSetBuilder fullscreen_quad_set_builder = create_descriptor_set_builder(&context);
  add_image_descriptor_set(&fullscreen_quad_set_builder, offscreen_framebuffer.color_image_view, sampler, 0, 1,
                           VK_SHADER_STAGE_FRAGMENT_BIT);
  DescriptorSetHandle fullscreen_quad_descriptor = build_descriptor_set(&fullscreen_quad_set_builder, descriptor_pool);
  assert(fullscreen_quad_set_builder.layout_bindings[0].stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
  VkPipelineLayout fullscreen_quad_pipeline_layout =
      create_pipeline_layout(context.device, &fullscreen_quad_descriptor.descriptor_set_layout, 1);

  // create pipelines
  VkPipeline triangle_pipeline =
      shader_handles_to_graphics_pipeline(&context, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_SIMPLE_VERT,
                                          SHADER_HANDLE_COMMON_SIMPLE_FRAG, x_pipeline_layout);

  VkPipeline square_pipeline =
      shader_handles_to_graphics_pipeline(&context, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_SIMPLE_VERT,
                                          SHADER_HANDLE_COMMON_SQUARE_FRAG, x_pipeline_layout);

  VkPipeline instanced_quad_pipeline = shader_handles_to_graphics_pipeline(
      &context, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_INSTANCED_QUAD_VERT,
      SHADER_HANDLE_COMMON_INSTANCED_QUAD_FRAG, mvp_pipeline_layout);

  PipelineConfig cube_pipeline_config = create_default_graphics_pipeline_config(
      offscreen_framebuffer.render_pass, shader_modules[SHADER_HANDLE_COMMON_CUBE_VERT],
      shader_modules[SHADER_HANDLE_COMMON_CUBE_FRAG],
      &generated_vulkan_vertex_layouts[common_cube_vert_shader_spec.vertex_layout_id], cube_pipeline_layout);
  cube_pipeline_config.cull_mode = VK_CULL_MODE_FRONT_BIT;
  VkPipeline cube_pipeline = create_graphics_pipeline(context.device, &cube_pipeline_config, context.pipeline_cache);

  VkPipeline fullscreen_quad_pipeline =
      shader_handles_to_graphics_pipeline(&context, context.render_pass, SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_VERT,
                                          SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_FRAG, fullscreen_quad_pipeline_layout);

  RenderCall triangle_render_call;
  triangle_render_call.num_vertices = 3;
  triangle_render_call.instance_count = 1;
  triangle_render_call.graphics_pipeline = triangle_pipeline;
  triangle_render_call.num_vertex_buffers = 1;
  triangle_render_call.vertex_buffer_offsets[0] = triangle_vertices_slice->offset;
  triangle_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  triangle_render_call.pipeline_layout = x_pipeline_layout;
  triangle_render_call.num_descriptor_sets = 1;
  triangle_render_call.descriptor_sets[0] = simple__descriptor.descriptor_set;
  triangle_render_call.is_indexed = false;

  RenderCall square_render_call;
  square_render_call.num_vertices = 6;
  square_render_call.instance_count = 1;
  square_render_call.graphics_pipeline = square_pipeline;
  square_render_call.num_vertex_buffers = 1;
  square_render_call.vertex_buffer_offsets[0] = square_slice->offset;
  square_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  square_render_call.pipeline_layout = x_pipeline_layout;
  square_render_call.num_descriptor_sets = 1;
  square_render_call.descriptor_sets[0] = simple__descriptor.descriptor_set;
  square_render_call.is_indexed = false;

  RenderCall cube_render_call;
  cube_render_call.num_vertices = 36;
  cube_render_call.instance_count = 1;
  cube_render_call.graphics_pipeline = cube_pipeline;
  cube_render_call.num_vertex_buffers = 1;
  cube_render_call.vertex_buffer_offsets[0] = cube_slice->offset;
  cube_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  cube_render_call.pipeline_layout = cube_pipeline_layout;
  cube_render_call.num_descriptor_sets = 1;
  cube_render_call.descriptor_sets[0] = cube_descriptor.descriptor_set;
  cube_render_call.is_indexed = false;

  RenderCall instanced_render_call;
  instanced_render_call.num_indices = 6;
  instanced_render_call.instance_count = instanced_quad_count;
  instanced_render_call.graphics_pipeline = instanced_quad_pipeline;
  instanced_render_call.num_vertex_buffers = 2;
  instanced_render_call.vertex_buffer_offsets[0] = unit_square_position_slice->offset;
  instanced_render_call.vertex_buffer_offsets[1] = quad_position_slice->offset;
  instanced_render_call.vertex_buffers[0] = vertex_buffer->buffer;
  instanced_render_call.vertex_buffers[1] = vertex_buffer->buffer;
  instanced_render_call.pipeline_layout = mvp_pipeline_layout;
  instanced_render_call.num_descriptor_sets = 1;
  instanced_render_call.descriptor_sets[0] = mvp_descriptor.descriptor_set;
  instanced_render_call.index_buffer = index_buffer->buffer;
  instanced_render_call.index_buffer_offset = 0;
  instanced_render_call.is_indexed = true;

  RenderCall fullscreen_quad_render_call;
  fullscreen_quad_render_call.num_vertices = 3;
  fullscreen_quad_render_call.instance_count = 1;
  fullscreen_quad_render_call.graphics_pipeline = fullscreen_quad_pipeline;
  fullscreen_quad_render_call.num_vertex_buffers = 0;
  fullscreen_quad_render_call.pipeline_layout = fullscreen_quad_pipeline_layout;
  fullscreen_quad_render_call.num_descriptor_sets = 1;
  fullscreen_quad_render_call.descriptor_sets[0] = fullscreen_quad_descriptor.descriptor_set;
  fullscreen_quad_render_call.is_indexed = false;

  MVPUniform mvp;
  mvp.projection = glm::mat4(1.0f);
  mvp.view = glm::mat4(1.0f);

  const glm::vec3 cube_translation_vector = {1.5f, -1.3f, 1.5f};
  const f32 root3 = 0.57735026919;
  const glm::vec3 cube_rotation_axis = {root3, root3, root3};

  Camera camera = new_camera(CameraType::Camera3D);
  camera.position = {0.0f, 0.0f, 5.0f};
  glm::mat4 camera_vp;
  Inputs inputs;
  init_inputs(&inputs);

  f32 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(context.window)) {
    i32 height, width;
    glfwGetWindowSize(context.window, &width, &height);
    update_key_inputs_glfw(&inputs, context.window);

    glfwPollEvents();
    f32 t = glfwGetTime();
    f32 dt = t - t0;
    t0 = t;
    f32 sint = sinf(t);
    f32 x = fabs(sint);

    mvp.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), cube_translation_vector);
    // cube_model = glm::rotate(cube_model, t, cube_rotation_axis);
    cube_model = glm::scale(cube_model, {0.2f, 0.2f, 0.2f});

    glm::vec4 light_position(2 * sint, sint, 0.5f, 0.0f);

    CameraMatrices camera_matrices = new_camera_matrices(&camera, width, height);
    camera_vp = camera_matrices.projection * camera_matrices.view;

    if (!begin_frame(&context)) {
      fprintf(stderr, "Failed to begin frame\n");
      continue;
    }

    write_to_uniform_buffer(&global_uniform_buffer, &mvp, mvp_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &cube_model, cube_model_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &x, x_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &light_position, light_position_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &camera_vp, camera_vp_handle);

    VkCommandBuffer command_buffer = begin_command_buffer(&context);

    // transition to use as render target
    transition_image_layout(command_buffer, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    begin_render_pass(&context, command_buffer, offscreen_framebuffer.render_pass, offscreen_framebuffer.framebuffer,
                      clear_values, NUM_ATTACHMENTS, viewport_state.scissor.offset);

    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    render_mesh(command_buffer, &triangle_render_call);
    render_mesh(command_buffer, &square_render_call);
    render_mesh(command_buffer, &instanced_render_call);
    render_mesh(command_buffer, &cube_render_call);

    vkCmdEndRenderPass(command_buffer);

    // transition to use as sampler
    transition_image_layout(command_buffer, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    begin_render_pass(&context, command_buffer, context.render_pass, context.framebuffers[context.image_index],
                      clear_values, NUM_ATTACHMENTS, viewport_state.scissor.offset);
    render_mesh(command_buffer, &fullscreen_quad_render_call);
    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");

    submit_and_present(&context, command_buffer);

    context.current_frame++;
    context.current_frame_index = context.current_frame % MAX_FRAMES_IN_FLIGHT;
    glm::vec4 movement_direction = movement_direction_from_inputs(&inputs);
    f32 speed = 10.0f;
    move_camera_3d(&camera, dt, speed * movement_direction);

#if 0
    printf("offscreen image addr %p\n",
           (void *)offscreen_framebuffer.color_image);
    printf("framebuffer image addr %p\n",
           (void *)context.framebuffers[context.image_index]);

    if (context.current_frame % 120 == 0) {
      printf("dt %f\n", dt);

      log_camera(&camera);

      printf("movement direction %f %f %f\n", movement_direction.x,
             movement_direction.y, movement_direction.z);

      printf("projection: \n%s\n",
             glm::to_string(camera_matrices.projection).c_str());
      printf("view: \n%s\n", glm::to_string(camera_matrices.view).c_str());
    }
#endif
  }

  // cleanup
  vkDeviceWaitIdle(context.device);

  vkDestroySampler(context.device, sampler, NULL);
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(context.device, &textures[i]);
  }

  free_generated_shader_vk_modules(context.device);

  destroy_color_depth_framebuffer(&context, &offscreen_framebuffer);
  destroy_descriptor_set_handle(context.device, &simple__descriptor);
  destroy_descriptor_set_handle(context.device, &mvp_descriptor);
  destroy_descriptor_set_handle(context.device, &cube_descriptor);
  destroy_descriptor_set_handle(context.device, &fullscreen_quad_descriptor);

  vkDestroyPipelineLayout(context.device, x_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, mvp_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, cube_pipeline_layout, NULL);
  vkDestroyPipelineLayout(context.device, fullscreen_quad_pipeline_layout, NULL);

  vkDestroyPipeline(context.device, square_pipeline, NULL);
  vkDestroyPipeline(context.device, triangle_pipeline, NULL);
  vkDestroyPipeline(context.device, instanced_quad_pipeline, NULL);
  vkDestroyPipeline(context.device, cube_pipeline, NULL);
  vkDestroyPipeline(context.device, fullscreen_quad_pipeline, NULL);

  destroy_uniform_buffer(&context, &global_uniform_buffer);
  vkDestroyDescriptorPool(context.device, descriptor_pool, NULL);
  destroy_buffer_manager(&buffer_manager);
  destroy_vulkan_context(&context);

  return 0;
}
