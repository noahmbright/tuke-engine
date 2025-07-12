#include "pong.h"
#include "pong_shaders.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/pong/field_background.jpg"};

void init_buffers(State *state) {

  VulkanContext *ctx = &state->context;
  state->vertex_buffer = create_buffer(ctx, BUFFER_TYPE_VERTEX, total_size);
  state->index_buffer =
      create_buffer(ctx, BUFFER_TYPE_INDEX, sizeof(unit_square_indices));

  StagingArena staging_arena = create_staging_arena(ctx, total_size);

  // TODO making errors with offsets and staging

  // paddle vertices go in the first sizeof(...) bytes of the vertex buffer
  u32 paddle_vertices_offset = stage_data_explicit(
      ctx, &staging_arena, paddle_vertices, sizeof(paddle_vertices),
      state->vertex_buffer.buffer, 0 /*dst_offset*/);
  (void)paddle_vertices_offset;

  // unit_square_indices go in the first sizeof(...) bytes of the index_buffer
  u32 index_data_offset = stage_data_explicit(
      ctx, &staging_arena, unit_square_indices, sizeof(unit_square_indices),
      state->index_buffer.buffer, 0 /*dst_offset*/);
  (void)index_data_offset;

  flush_staging_arena(ctx, &staging_arena);

  // TODO how to reuse this staging buffer?
  destroy_vulkan_buffer(ctx, staging_arena.buffer);

  state->uniform_buffer = create_uniform_buffer(ctx, sizeof(MVPUniform));
}

State setup_state(const char *title) {

  State state;

  state.arena_dimensions = arena_dimensions0;

  state.context = create_vulkan_context(title);
  VulkanContext *ctx = &state.context;

  load_vulkan_textures(ctx, texture_names, NUM_TEXTURES, state.textures);
  init_buffers(&state);

  state.sampler = create_sampler(ctx->device);
  u32 max_sets = 1;
  u32 pool_size_count = 2;
  VkDescriptorPoolSize mvp_pool_sizes[2];
  mvp_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  mvp_pool_sizes[0].descriptorCount = 1;
  mvp_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  mvp_pool_sizes[1].descriptorCount = 1;

  state.descriptor_pool = create_descriptor_pool(ctx->device, mvp_pool_sizes,
                                                 pool_size_count, max_sets);

  cache_shader_module(ctx->shader_cache, paddle_vert_spv_spec);
  cache_shader_module(ctx->shader_cache, paddle_frag_spv_spec);
  // anticipating binding 0 per vertex, position, uv, normal
  // binding 1 per instance, vec3 position, float texture ID
  state.vertex_builder = create_vertex_layout_builder();
  push_vertex_binding(&state.vertex_builder, 0, 5 * sizeof(f32),
                      VK_VERTEX_INPUT_RATE_VERTEX);
  push_vertex_attribute(&state.vertex_builder, 0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                        0);
  push_vertex_attribute(&state.vertex_builder, 1, 0, VK_FORMAT_R32G32_SFLOAT,
                        3 * sizeof(f32));
  state.vertex_input_state = build_vertex_input_state(&state.vertex_builder);

  state.set_builder = create_descriptor_set_builder(ctx);
  add_uniform_buffer_descriptor_set(&state.set_builder, &state.uniform_buffer,
                                    0, sizeof(MVPUniform), 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  add_texture_descriptor_set(&state.set_builder,
                             &state.textures[TEXTURE_FIELD_BACKGROUND],
                             state.sampler, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
  state.descriptor_set =
      build_descriptor_set(&state.set_builder, state.descriptor_pool);
  state.descriptor_set_layout = state.set_builder.descriptor_set_layout;
  state.pipeline_layout =
      create_pipeline_layout(ctx->device, &state.descriptor_set_layout, 1);

  state.pipeline = create_default_graphics_pipeline(
      ctx, paddle_vert_spv_spec.name, paddle_frag_spv_spec.name,
      &state.vertex_input_state, state.pipeline_layout);

  state.clear_value.color = {{0.0, 1.0, 0.0, 1.0}};
  state.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0);

  state.render_call.instance_count = 1;
  state.render_call.graphics_pipeline = state.pipeline;
  state.render_call.pipeline_layout = state.pipeline_layout;
  state.render_call.descriptor_set = state.descriptor_set;
  state.render_call.num_vertex_buffers = 1;
  state.render_call.vertex_buffer_offsets[0] = 0;
  state.render_call.vertex_buffers[0] = state.vertex_buffer.buffer;
  state.render_call.index_buffer_offset = 0;
  state.render_call.num_indices = 6;
  state.render_call.index_buffer = state.index_buffer.buffer;
  state.render_call.is_indexed = true;

  return state;
}

void destroy_state(State *state) {
  VulkanContext *ctx = &state->context;

  vkDeviceWaitIdle(ctx->device);

  // texture and sampler
  vkDestroySampler(ctx->device, state->sampler, NULL);
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(ctx->device, &state->textures[i]);
  }

  destroy_descriptor_set_builder(&state->set_builder);
  vkDestroyPipelineLayout(ctx->device, state->pipeline_layout, NULL);
  vkDestroyPipeline(ctx->device, state->pipeline, NULL);

  vkDestroyDescriptorPool(ctx->device, state->descriptor_pool, NULL);

  destroy_vulkan_buffer(ctx, state->vertex_buffer);
  destroy_vulkan_buffer(ctx, state->index_buffer);
  destroy_uniform_buffer(ctx, &state->uniform_buffer);

  destroy_vulkan_context(ctx);
}

void render(State *state) {
  VulkanContext *ctx = &state->context;

  if (!begin_frame(ctx)) {
    fprintf(stderr, "Failed to begin frame\n");
    return;
  }

  VkCommandBuffer command_buffer = begin_command_buffer(ctx);
  begin_render_pass(ctx, command_buffer, state->clear_value,
                    state->viewport_state.scissor.offset);
  vkCmdSetViewport(command_buffer, 0, 1, &state->viewport_state.viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &state->viewport_state.scissor);

  render_mesh(command_buffer, &state->render_call);

  vkCmdEndRenderPass(command_buffer);
  VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
  submit_and_present(ctx, command_buffer);

  ctx->current_frame++;
  ctx->current_frame_index = ctx->current_frame % MAX_FRAMES_IN_FLIGHT;
}
