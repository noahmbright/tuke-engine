#include "pong.h"
#include "compiled_shaders.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include "window.h"

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

  state->uniform_buffer = create_uniform_buffer(ctx, 3 * MAT4_SIZE);
}

// currently, global VP is premultiplied proj * view
void init_descriptor_sets(State *state) {
  VulkanContext *ctx = &state->context;

  // TODO background vs arena, in game vs overlay
  // background has global VP, sampler, background model
  DescriptorSetBuilder background_builder = create_descriptor_set_builder(ctx);
  // global vp
  add_uniform_buffer_descriptor_set(&background_builder, &state->uniform_buffer,
                                    0, MAT4_SIZE, 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);

  // background model
  add_uniform_buffer_descriptor_set(&background_builder, &state->uniform_buffer,
                                    MAT4_SIZE, MAT4_SIZE, 1, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);

  add_texture_descriptor_set(
      &background_builder, &state->textures[TEXTURE_FIELD_BACKGROUND],
      state->sampler, 2, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

  state->descriptor_set_handles[DESCRIPTOR_HANDLE_BACKGROUND] =
      build_descriptor_set(&background_builder, state->descriptor_pool);

  // paddles/ball have global VP, array of textures?
  DescriptorSetBuilder paddle_ball_builder = create_descriptor_set_builder(ctx);
  // global vp
  add_uniform_buffer_descriptor_set(&paddle_ball_builder,
                                    &state->uniform_buffer, 0, MAT4_SIZE, 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);

  // position transform
  add_uniform_buffer_descriptor_set(
      &paddle_ball_builder, &state->uniform_buffer, 2 * MAT4_SIZE, MAT4_SIZE, 1,
      1, VK_SHADER_STAGE_VERTEX_BIT, false);

  add_texture_descriptor_set(
      &paddle_ball_builder, &state->textures[TEXTURE_GENERIC_GIRL],
      state->sampler, 2, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

  state->descriptor_set_handles[DESCRIPTOR_HANDLE_PADDLES_AND_BALL] =
      build_descriptor_set(&paddle_ball_builder, state->descriptor_pool);
}

State setup_state(const char *title) {
  init_vertex_layout_registry();

  State state;
  state.arena_dimensions = arena_dimensions0;
  init_inputs(&state.inputs);
  state.game_mode = GAMEMODE_MAIN_MENU;
  state.left_score = state.right_score = 0;

  state.context = create_vulkan_context(title);
  VulkanContext *ctx = &state.context;

  load_vulkan_textures(ctx, texture_names, NUM_TEXTURES, state.textures);
  init_buffers(&state);

  // TODO this sampler never changes - look into wiring immutable samplers
  state.sampler = create_sampler(ctx->device);

  // TODO will want to add some kind of accumulation of resources into
  // python compiler
  // pool sizes are not for individual resources, but places that a resource
  // needs to bind
  const u32 max_sets = 3;
  const u32 pool_size_count = 2;
  VkDescriptorPoolSize mvp_pool_sizes[pool_size_count];
  mvp_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  mvp_pool_sizes[0].descriptorCount = 3;
  mvp_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  mvp_pool_sizes[1].descriptorCount = 2;
  state.descriptor_pool = create_descriptor_pool(ctx->device, mvp_pool_sizes,
                                                 pool_size_count, max_sets);

  cache_shader_module(ctx->shader_cache, paddle_vert_spec);
  cache_shader_module(ctx->shader_cache, paddle_frag_spec);
  cache_shader_module(ctx->shader_cache, background_frag_spec);
  cache_shader_module(ctx->shader_cache, background_vert_spec);

  init_descriptor_sets(&state);

  DescriptorSetHandle *background_handle =
      &state.descriptor_set_handles[DESCRIPTOR_HANDLE_BACKGROUND];
  state.pipeline_layout = create_pipeline_layout(
      ctx->device, &background_handle->descriptor_set_layout, 1);

  state.pipeline = create_default_graphics_pipeline(
      ctx, background_vert_spec.name, background_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC2), state.pipeline_layout);

  state.clear_value.color = {{0.0, 1.0, 0.0, 1.0}};
  state.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0);

  state.render_call.instance_count = 1;
  state.render_call.graphics_pipeline = state.pipeline;
  state.render_call.pipeline_layout = state.pipeline_layout;
  state.render_call.descriptor_set = background_handle->descriptor_set;
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

  for (u32 i = 0; i < NUM_DESCRIPTOR_HANDLES; i++) {
    destroy_descriptor_set_handle(ctx->device,
                                  &state->descriptor_set_handles[i]);
  }

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

void process_inputs(State *state) {
  Inputs *inputs = &state->inputs;
  update_key_inputs_glfw(inputs, state->context.window);

  switch (state->game_mode) {

  case GAMEMODE_PAUSED: {
    if (key_pressed(inputs, INPUT_KEY_Q)) {
      printf("unpausing\n");
      state->game_mode = GAMEMODE_PLAYING;
    }

    break;
  } // paused

  case GAMEMODE_PLAYING: {
    if (key_pressed(inputs, INPUT_KEY_Q)) {
      printf("pausing\n");
      state->game_mode = GAMEMODE_PAUSED;
    }

    break;
  } // playing

  case GAMEMODE_MAIN_MENU: {
    if (key_pressed(inputs, INPUT_KEY_SPACEBAR)) {
      printf("starting game\n");
      state->game_mode = GAMEMODE_PLAYING;
    }

    break;
  } // main_menu

  } // switch(state->game_mode)
}
