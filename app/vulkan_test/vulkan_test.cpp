#define GLM_ENABLE_EXPERIMENTAL

#include "vulkan_test.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "glfw_vulkan.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "utils.h"
#include "vulkan/vulkan_base.h"
#include "window.h"

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);
  VulkanContext ctx = create_vulkan_context("Disastrous Vulkan Test", window_info);
  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(ctx.device, generated_pool_sizes, pool_size_count, max_descriptor_sets);
  ViewportState viewport_state = create_viewport_state_xy(ctx.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {
      {.color = {{0.01, 0.01, 0.01, 1.0}}}, {.depthStencil = {.depth = 1.0f, .stencil = 0}}
  };

  STBHandle stbs[NUM_TEXTURES];
  VulkanImageData image_datas[NUM_TEXTURES];
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    stbs[i] = load_texture(texture_names[i]);
    image_datas[i].n_channels = stbs[i].n_channels;
    image_datas[i].data = stbs[i].data;
    image_datas[i].width = stbs[i].width;
    image_datas[i].height = stbs[i].height;
  }

  VulkanTexture textures[NUM_TEXTURES];
  load_vulkan_textures(&ctx, image_datas, NUM_TEXTURES, textures);

  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    free_stb_handle(&stbs[i]);
  }

  VkSampler sampler = create_sampler(ctx.device);

  ColorDepthFramebuffer offscreen_framebuffer =
      create_color_depth_framebuffer(&ctx, ctx.swapchain_extent, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_D32_SFLOAT);

  BufferUploadQueue buffer_upload_queue = create_buffer_upload_queue();
  const BufferHandle *triangle_vertices_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, triangle_vertices);
  const BufferHandle *square_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, square_vertices);
  const BufferHandle *unit_square_position_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, unit_square_positions);
  const BufferHandle *quad_position_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, quad_positions);
  const BufferHandle *unit_square_indices_slice = UPLOAD_INDEX_ARRAY(buffer_upload_queue, unit_square_indices);
  const BufferHandle *cube_slice = UPLOAD_VERTEX_ARRAY(buffer_upload_queue, cube_vertices);
  (void)unit_square_indices_slice;

  BufferManager buffer_manager = flush_buffers(&ctx, &buffer_upload_queue);
  VulkanBuffer *vertex_buffer = &buffer_manager.vertex_buffer;
  VulkanBuffer *index_buffer = &buffer_manager.index_buffer;

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite mvp_handle = push_uniform(&ub_manager, sizeof(MVPUniform));
  UniformWrite cube_model_handle = push_uniform(&ub_manager, sizeof(CubeModel));
  UniformWrite x_handle = push_uniform(&ub_manager, sizeof(UniformBufferObject));
  UniformWrite light_position_handle = push_uniform(&ub_manager, sizeof(LightPosition));
  UniformWrite camera_vp_handle = push_uniform(&ub_manager, sizeof(CameraVP));
  UniformBuffer global_uniform_buffer = create_uniform_buffer(&ctx, ub_manager.current_offset);

  // It is unacceptable that I need to think this hard about sets and bindings in the app.
  // Stateful builders are kind of ugly.
  // for simple.frag.in, x uniform is set/binding 0/0
  DescriptorSetBuilder simple_set_builder = create_descriptor_set_builder(&ctx);

  add_uniform_buffer_descriptor_set(
      &simple_set_builder, &global_uniform_buffer, x_handle.offset, x_handle.size, 0, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
      false
  );
  add_uniform_buffer_descriptor_set(
      &simple_set_builder, &global_uniform_buffer, camera_vp_handle.offset, camera_vp_handle.size, 1, 1,
      VK_SHADER_STAGE_VERTEX_BIT, false
  );
  add_uniform_buffer_descriptor_set(
      &simple_set_builder, &global_uniform_buffer, light_position_handle.offset, light_position_handle.size, 2, 1,
      VK_SHADER_STAGE_VERTEX_BIT, false
  );
  DescriptorSetHandle simple_descriptor = build_descriptor_set(&simple_set_builder, descriptor_pool);
  VkPipelineLayout x_pipeline_layout = create_pipeline_layout(ctx.device, &simple_descriptor.descriptor_set_layout, 1);

  // instanced quad: the mvp uniform is set/binding 0/0, the float is 0/1
  DescriptorSetBuilder mvp_set_builder = create_descriptor_set_builder(&ctx);
  add_uniform_buffer_descriptor_set(
      &mvp_set_builder, &global_uniform_buffer, mvp_handle.offset, mvp_handle.size, 0, 1, VK_SHADER_STAGE_VERTEX_BIT,
      false
  );
  add_uniform_buffer_descriptor_set(
      &mvp_set_builder, &global_uniform_buffer, x_handle.offset, x_handle.size, 1, 1, VK_SHADER_STAGE_VERTEX_BIT, false
  );
  add_image_descriptor_set(&mvp_set_builder, textures[2].image_view, sampler, 2, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

  DescriptorSetHandle mvp_descriptor = build_descriptor_set(&mvp_set_builder, descriptor_pool);

  VkPipelineLayout mvp_pipeline_layout = create_pipeline_layout(ctx.device, &mvp_descriptor.descriptor_set_layout, 1);

  // cube descriptor set
  DescriptorSetBuilder cube_set_builder = create_descriptor_set_builder(&ctx);
  add_uniform_buffer_descriptor_set(
      &cube_set_builder, &global_uniform_buffer, cube_model_handle.offset, cube_model_handle.size, 0, 1,
      VK_SHADER_STAGE_VERTEX_BIT, false
  );
  add_uniform_buffer_descriptor_set(
      &cube_set_builder, &global_uniform_buffer, light_position_handle.offset, light_position_handle.size, 1, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT, false
  );
  add_uniform_buffer_descriptor_set(
      &cube_set_builder, &global_uniform_buffer, camera_vp_handle.offset, camera_vp_handle.size, 2, 1,
      VK_SHADER_STAGE_VERTEX_BIT, false
  );
  DescriptorSetHandle cube_descriptor = build_descriptor_set(&cube_set_builder, descriptor_pool);

  VkPipelineLayout cube_pipeline_layout = create_pipeline_layout(ctx.device, &cube_descriptor.descriptor_set_layout, 1);

  // fullscreen quad descriptor set
  DescriptorSetBuilder fullscreen_quad_set_builder = create_descriptor_set_builder(&ctx);
  add_image_descriptor_set(
      &fullscreen_quad_set_builder, offscreen_framebuffer.color_image_view, sampler, 0, 1, VK_SHADER_STAGE_FRAGMENT_BIT
  );
  DescriptorSetHandle fullscreen_quad_descriptor = build_descriptor_set(&fullscreen_quad_set_builder, descriptor_pool);
  assert(fullscreen_quad_set_builder.layout_bindings[0].stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
  VkPipelineLayout fullscreen_quad_pipeline_layout =
      create_pipeline_layout(ctx.device, &fullscreen_quad_descriptor.descriptor_set_layout, 1);

  // create pipelines
  VkPipeline triangle_pipeline = shader_handles_to_graphics_pipeline(
      &ctx, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_SIMPLE_VERT, SHADER_HANDLE_COMMON_SIMPLE_FRAG,
      x_pipeline_layout
  );

  VkPipeline square_pipeline = shader_handles_to_graphics_pipeline(
      &ctx, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_SIMPLE_VERT, SHADER_HANDLE_COMMON_SQUARE_FRAG,
      x_pipeline_layout
  );

  VkPipeline instanced_quad_pipeline = shader_handles_to_graphics_pipeline(
      &ctx, offscreen_framebuffer.render_pass, SHADER_HANDLE_COMMON_INSTANCED_QUAD_VERT,
      SHADER_HANDLE_COMMON_INSTANCED_QUAD_FRAG, mvp_pipeline_layout
  );

  // Really need to pare this down. All this ceremony is so I can override cull mode
  const ShaderSpec *cube_vert_spec = generated_shader_specs[SHADER_HANDLE_COMMON_CUBE_VERT];
  const ShaderSpec *cube_frag_spec = generated_shader_specs[SHADER_HANDLE_COMMON_CUBE_FRAG];
  VkShaderModule cube_vert = create_shader_module(ctx.device, cube_vert_spec->spv, cube_vert_spec->spv_size);
  VkShaderModule cube_frag = create_shader_module(ctx.device, cube_frag_spec->spv, cube_frag_spec->spv_size);
  PipelineConfig cube_pipeline_config = {
      .vertex_shader = cube_vert,
      .fragment_shader = cube_frag,
      .stage_count = 2,
      .vertex_input_state_create_info = &generated_vulkan_vertex_layouts[cube_vert_spec->vertex_layout_id],
      .render_pass = offscreen_framebuffer.render_pass,
      .pipeline_layout = cube_pipeline_layout,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitive_restart_enabled = VK_FALSE,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = VK_CULL_MODE_FRONT_BIT,
      .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .sample_count_flag = VK_SAMPLE_COUNT_1_BIT,
      .blend_mode = BLEND_MODE_ALPHA,
  };
  VkPipeline cube_pipeline = create_graphics_pipeline(ctx.device, &cube_pipeline_config, ctx.pipeline_cache);
  vkDestroyShaderModule(ctx.device, cube_vert, NULL);
  vkDestroyShaderModule(ctx.device, cube_frag, NULL);

  VkPipeline fullscreen_quad_pipeline = shader_handles_to_graphics_pipeline(
      &ctx, ctx.render_pass, SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_VERT, SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_FRAG,
      fullscreen_quad_pipeline_layout
  );

  RenderCall triangle_render_call = {
      .num_vertices = 3,
      .instance_count = 1,
      .graphics_pipeline = triangle_pipeline,
      .num_vertex_buffers = 1,
      .vertex_buffer_offsets[0] = triangle_vertices_slice->offset,
      .vertex_buffers[0] = vertex_buffer->buffer,
      .pipeline_layout = x_pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets[0] = simple_descriptor.descriptor_set,
      .is_indexed = false,
  };

  RenderCall square_render_call = {
      .num_vertices = 6,
      .instance_count = 1,
      .graphics_pipeline = square_pipeline,
      .num_vertex_buffers = 1,
      .vertex_buffer_offsets[0] = square_slice->offset,
      .vertex_buffers[0] = vertex_buffer->buffer,
      .pipeline_layout = x_pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets[0] = simple_descriptor.descriptor_set,
      .is_indexed = false,
  };

  RenderCall cube_render_call = {
      .num_vertices = 36,
      .instance_count = 1,
      .graphics_pipeline = cube_pipeline,
      .num_vertex_buffers = 1,
      .vertex_buffer_offsets[0] = cube_slice->offset,
      .vertex_buffers[0] = vertex_buffer->buffer,
      .pipeline_layout = cube_pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets[0] = cube_descriptor.descriptor_set,
      .is_indexed = false,
  };

  RenderCall instanced_render_call = {
      .num_indices = 6,
      .instance_count = instanced_quad_count,
      .graphics_pipeline = instanced_quad_pipeline,
      .num_vertex_buffers = 2,
      .vertex_buffer_offsets[0] = unit_square_position_slice->offset,
      .vertex_buffer_offsets[1] = quad_position_slice->offset,
      .vertex_buffers[0] = vertex_buffer->buffer,
      .vertex_buffers[1] = vertex_buffer->buffer,
      .pipeline_layout = mvp_pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets[0] = mvp_descriptor.descriptor_set,
      .index_buffer = index_buffer->buffer,
      .index_buffer_offset = 0,
      .is_indexed = true,
  };

  RenderCall fullscreen_quad_render_call = {
      .num_vertices = 3,
      .instance_count = 1,
      .graphics_pipeline = fullscreen_quad_pipeline,
      .num_vertex_buffers = 0,
      .pipeline_layout = fullscreen_quad_pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets[0] = fullscreen_quad_descriptor.descriptor_set,
      .is_indexed = false,
  };

  MVPUniform mvp;
  mvp.projection = glm::mat4(1.0f);
  mvp.view = glm::mat4(1.0f);

  const glm::vec3 cube_translation_vector = {1.5f, -1.3f, 1.5f};
  const f32 root3 = 0.57735026919;
  const glm::vec3 cube_rotation_axis = {root3, root3, root3};
  (void)cube_rotation_axis;

  Camera camera = create_camera(CAMERA_TYPE_3D);
  camera.position = {0.0f, 0.0f, 5.0f};
  glm::mat4 camera_vp;
  Inputs inputs;
  init_inputs(&inputs);

  f32 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    i32 height, width;
    glfwGetWindowSize(window, &width, &height);
    update_key_inputs_glfw(&inputs, window);

    glfwPollEvents();
    f32 t = glfwGetTime();
    f32 dt = t - t0;
    t0 = t;
    f32 sint = sinf(t);
    UniformBufferObject ubo = {.x = fabs(sint)};

    mvp.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), cube_translation_vector);
    // cube_model = glm::rotate(cube_model, t, cube_rotation_axis);
    cube_model = glm::scale(cube_model, {0.2f, 0.2f, 0.2f});

    glm::vec4 light_position(2 * sint, sint, 0.5f, 0.0f);

    CameraMatrices camera_matrices = create_camera_matrices(&camera, width, height);
    camera_vp = camera_matrices.projection * camera_matrices.view;

    begin_frame(&ctx);

    // Should have a queue for these, something more automated
    write_to_uniform_buffer(&global_uniform_buffer, &mvp, mvp_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &cube_model, cube_model_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &ubo, x_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &light_position, light_position_handle);
    write_to_uniform_buffer(&global_uniform_buffer, &camera_vp, camera_vp_handle);

    VkCommandBuffer cmd = begin_command_buffer(&ctx);

    // TODO do this through pipeline barriers
    //      Do this per frame? Or just at init?
    // transition to use as render target
    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    begin_render_pass(
        &ctx, cmd, offscreen_framebuffer.render_pass, offscreen_framebuffer.framebuffer, clear_values, NUM_ATTACHMENTS,
        viewport_state
    );

    // Should have a queue for these, something more automated
    render_mesh(cmd, &triangle_render_call);
    render_mesh(cmd, &square_render_call);
    render_mesh(cmd, &instanced_render_call);
    render_mesh(cmd, &cube_render_call);

    vkCmdEndRenderPass(cmd);

    // transition to use as sampler
    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    begin_render_pass(
        &ctx, cmd, ctx.render_pass, ctx.framebuffers[ctx.image_index], clear_values, NUM_ATTACHMENTS, viewport_state
    );
    render_mesh(cmd, &fullscreen_quad_render_call);
    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");

    submit_and_present(&ctx, cmd);
    update_frame_index(&ctx);

    glm::vec2 movement_direction = inputs_to_direction(&inputs);
    f32 speed = 10.0f;
    camera_move_3d(&camera, dt * speed * movement_direction);
  }

  // This is all too manual. Need a deletion queue managed internally.
  vkDeviceWaitIdle(ctx.device);

  vkDestroySampler(ctx.device, sampler, NULL);
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(ctx.device, &textures[i]);
  }

  destroy_color_depth_framebuffer(ctx.device, &offscreen_framebuffer);
  destroy_descriptor_set_handle(ctx.device, &simple_descriptor);
  destroy_descriptor_set_handle(ctx.device, &mvp_descriptor);
  destroy_descriptor_set_handle(ctx.device, &cube_descriptor);
  destroy_descriptor_set_handle(ctx.device, &fullscreen_quad_descriptor);

  vkDestroyPipelineLayout(ctx.device, x_pipeline_layout, NULL);
  vkDestroyPipelineLayout(ctx.device, mvp_pipeline_layout, NULL);
  vkDestroyPipelineLayout(ctx.device, cube_pipeline_layout, NULL);
  vkDestroyPipelineLayout(ctx.device, fullscreen_quad_pipeline_layout, NULL);

  vkDestroyPipeline(ctx.device, square_pipeline, NULL);
  vkDestroyPipeline(ctx.device, triangle_pipeline, NULL);
  vkDestroyPipeline(ctx.device, instanced_quad_pipeline, NULL);
  vkDestroyPipeline(ctx.device, cube_pipeline, NULL);
  vkDestroyPipeline(ctx.device, fullscreen_quad_pipeline, NULL);

  destroy_uniform_buffer(&ctx, &global_uniform_buffer);
  vkDestroyDescriptorPool(ctx.device, descriptor_pool, NULL);
  destroy_buffer_manager(&buffer_manager);

  destroy_vulkan_context(&ctx);

  return 0;
}
