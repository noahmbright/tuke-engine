#include "pong.h"
#include "compiled_shaders.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_base.h"
#include "window.h"

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/pong/field_background.jpg",
    "textures/girl_face.jpg", "textures/girl_face_normal_map.jpg"};

void init_buffers(State *state) {
  VulkanContext *ctx = &state->context;

  // big vertex/index buffers
  BufferUploadQueue buffer_upload_queue = new_buffer_upload_queue();

  const BufferHandle *unit_quad_vertices_slice =
      UPLOAD_VERTEX_ARRAY(buffer_upload_queue, paddle_vertices);
  const BufferHandle *quad_inices_slice =
      UPLOAD_INDEX_ARRAY(buffer_upload_queue, unit_square_indices);

  state->buffer_manager = flush_buffers(ctx, &buffer_upload_queue);

  // uniform buffer
  // TODO can I come up with a scheme for coordinating UBOs and the location of
  // what data in what portion of the renderer maps to what data in the shaders?
  UniformBufferManager ub_manager = new_uniform_buffer_manager();
  state->uniform_writes.camera_vp =
      push_uniform(&ub_manager, sizeof(VPUniform));
  state->uniform_writes.arena_model =
      push_uniform(&ub_manager, sizeof(glm::mat4));
  state->uniform_writes.instance_data =
      push_uniform(&ub_manager, sizeof(InstanceDataUBO));
  state->uniform_buffer = create_uniform_buffer(ctx, ub_manager.current_offset);
}

// currently, global VP is premultiplied proj * view
void init_descriptor_sets(State *state) {
  VulkanContext *ctx = &state->context;

  // global VP
  DescriptorSetBuilder global_vp_builder = create_descriptor_set_builder(ctx);
  // global vp
  add_uniform_buffer_descriptor_set(&global_vp_builder, &state->uniform_buffer,
                                    state->uniform_writes.camera_vp.offset,
                                    state->uniform_writes.camera_vp.size, 0, 1,
                                    VK_SHADER_STAGE_VERTEX_BIT, false);
  state->descriptor_set_handles[DESCRIPTOR_HANDLE_GLOBAL_VP] =
      build_descriptor_set(&global_vp_builder, state->descriptor_pool);

  // TODO background vs arena, in game vs overlay
  DescriptorSetBuilder background_builder = create_descriptor_set_builder(ctx);
  // background model
  add_uniform_buffer_descriptor_set(&background_builder, &state->uniform_buffer,
                                    state->uniform_writes.arena_model.offset,
                                    state->uniform_writes.arena_model.size, 0,
                                    1, VK_SHADER_STAGE_VERTEX_BIT, false);

  add_texture_descriptor_set(
      &background_builder, &state->textures[TEXTURE_GIRL_FACE], state->sampler,
      1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

  state->descriptor_set_handles[DESCRIPTOR_HANDLE_BACKGROUND] =
      build_descriptor_set(&background_builder, state->descriptor_pool);

  // paddles and ball
  DescriptorSetBuilder paddle_ball_builder = create_descriptor_set_builder(ctx);
  add_uniform_buffer_descriptor_set(&paddle_ball_builder,
                                    &state->uniform_buffer,
                                    state->uniform_writes.instance_data.offset,
                                    state->uniform_writes.instance_data.size, 0,
                                    1, VK_SHADER_STAGE_VERTEX_BIT, false);

  state->descriptor_set_handles[DESCRIPTOR_HANDLE_PADDLES_AND_BALL] =
      build_descriptor_set(&paddle_ball_builder, state->descriptor_pool);
}

void init_background_material(State *state) {

  Material *mat = &state->background_material;

  DescriptorSetHandle *vp_handle =
      &state->descriptor_set_handles[DESCRIPTOR_HANDLE_GLOBAL_VP];

  DescriptorSetHandle *background_handle =
      &state->descriptor_set_handles[DESCRIPTOR_HANDLE_BACKGROUND];

  VkDescriptorSetLayout layouts[2] = {vp_handle->descriptor_set_layout,
                                      background_handle->descriptor_set_layout};

  state->background_material.pipeline_layout =
      create_pipeline_layout(state->context.device, layouts, 2);

  state->background_material.pipeline = create_default_graphics_pipeline(
      &state->context, background_vert_spec.name, background_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC2),
      state->background_material.pipeline_layout);

  mat->render_call.instance_count = 1;
  mat->render_call.graphics_pipeline = mat->pipeline;
  mat->render_call.pipeline_layout = mat->pipeline_layout;
  mat->render_call.num_descriptor_sets = 2;
  mat->render_call.descriptor_sets[0] = vp_handle->descriptor_set;
  mat->render_call.descriptor_sets[1] = background_handle->descriptor_set;
  mat->render_call.num_vertex_buffers = 1;
  mat->render_call.vertex_buffer_offsets[0] = 0;
  mat->render_call.vertex_buffers[0] =
      state->buffer_manager.vertex_buffer.buffer;
  mat->render_call.index_buffer_offset = 0;
  mat->render_call.num_indices = 6;
  mat->render_call.index_buffer = state->buffer_manager.index_buffer.buffer;
  mat->render_call.is_indexed = true;
}

void init_paddles_material(State *state) {

  Material *mat = &state->paddle_material;

  DescriptorSetHandle *vp_handle =
      &state->descriptor_set_handles[DESCRIPTOR_HANDLE_GLOBAL_VP];

  DescriptorSetHandle *paddle_handle =
      &state->descriptor_set_handles[DESCRIPTOR_HANDLE_PADDLES_AND_BALL];

  VkDescriptorSetLayout layouts[2] = {vp_handle->descriptor_set_layout,
                                      paddle_handle->descriptor_set_layout};

  state->paddle_material.pipeline_layout =
      create_pipeline_layout(state->context.device, layouts, 2);

  state->paddle_material.pipeline = create_default_graphics_pipeline(
      &state->context, paddle_vert_spec.name, paddle_frag_spec.name,
      get_vertex_layout(VERTEX_LAYOUT_VEC3_VEC2),
      state->paddle_material.pipeline_layout);

  // TODO bump to 2 and 3
  mat->render_call.instance_count = InstanceDataUBO_model_array_size;
  mat->render_call.graphics_pipeline = mat->pipeline;
  mat->render_call.pipeline_layout = mat->pipeline_layout;
  mat->render_call.num_descriptor_sets = 2;
  mat->render_call.descriptor_sets[0] = vp_handle->descriptor_set;
  mat->render_call.descriptor_sets[1] = paddle_handle->descriptor_set;
  mat->render_call.num_vertex_buffers = 1;
  mat->render_call.vertex_buffer_offsets[0] = 0;
  mat->render_call.vertex_buffers[0] =
      state->buffer_manager.vertex_buffer.buffer;
  mat->render_call.index_buffer_offset = 0;
  mat->render_call.num_indices = 6;
  mat->render_call.index_buffer = state->buffer_manager.index_buffer.buffer;
  mat->render_call.is_indexed = true;
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

  state.descriptor_pool = create_descriptor_pool(
      ctx->device, generated_pool_sizes, pool_size_count, max_sets);

  cache_shader_module(ctx->shader_cache, paddle_vert_spec);
  cache_shader_module(ctx->shader_cache, paddle_frag_spec);
  cache_shader_module(ctx->shader_cache, background_frag_spec);
  cache_shader_module(ctx->shader_cache, background_vert_spec);

  init_descriptor_sets(&state);

  state.clear_values[0].color = {{0.0, 1.0, 0.0, 1.0}};
  state.clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
  state.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0);

  init_background_material(&state);
  init_paddles_material(&state);

  state.left_paddle_position = left_paddle_pos0;
  state.right_paddle_position = right_paddle_pos0;
  state.ball_position = ball_pos0;

  state.left_paddle_speed = 5.0f;
  state.right_paddle_speed = 5.0f;
  state.ball_speed = 5.0f;

  state.left_paddle_direction = glm::vec3(0.0f);
  state.right_paddle_direction = glm::vec3(0.0f);
  state.ball_direction = glm::vec3(0.0f);

  state.instance_data.model[0] = left_paddle_model0;
  state.instance_data.model[1] = right_paddle_model0;
  state.instance_data.model[2] = ball_model0;
  write_to_uniform_buffer(&state.uniform_buffer, &state.instance_data,
                          state.uniform_writes.instance_data);

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

  destroy_material(ctx, &state->background_material);
  destroy_material(ctx, &state->paddle_material);

  vkDestroyDescriptorPool(ctx->device, state->descriptor_pool, NULL);

  destroy_buffer_manager(&state->buffer_manager);
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
  begin_render_pass(ctx, command_buffer, state->clear_values, 2,
                    state->viewport_state.scissor.offset);
  vkCmdSetViewport(command_buffer, 0, 1, &state->viewport_state.viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &state->viewport_state.scissor);

  render_mesh(command_buffer, &state->background_material.render_call);
  render_mesh(command_buffer, &state->paddle_material.render_call);

  vkCmdEndRenderPass(command_buffer);
  VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
  submit_and_present(ctx, command_buffer);

  ctx->current_frame++;
  ctx->current_frame_index = ctx->current_frame % MAX_FRAMES_IN_FLIGHT;
}

void process_inputs_paused(State *state) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_Q)) {
    printf("unpausing\n");
    state->game_mode = GAMEMODE_PLAYING;
  }
}

void process_inputs_playing(State *state, f32 dt) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_Q)) {
    printf("pausing\n");
    state->game_mode = GAMEMODE_PAUSED;
  }

  glm::vec3 input_direction(0.0f);

  if (key_held(inputs, INPUT_KEY_W)) {
    input_direction.y += 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_A)) {
    input_direction.x -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_S)) {
    input_direction.y -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_D)) {
    input_direction.x += 1.0f;
  }

  // TODO make the paddle scale over the course of the game
  // TODO add a Transform struct so I don't need to multiply the infrequently
  // changing scale and frequently changing translations
  if (input_direction.length() > EPSILON) {
    state->left_paddle_position +=
        dt * state->left_paddle_speed * input_direction;

    const glm::mat4 left_paddle_translated =
        glm::translate(glm::mat4(1.0f), state->left_paddle_position);
    const glm::mat4 left_paddle_model =
        glm::scale(left_paddle_translated, paddle_scale0);
    state->instance_data.model[0] = left_paddle_model;

    write_to_uniform_buffer(&state->uniform_buffer, &state->instance_data,
                            state->uniform_writes.instance_data);
  }
}

void process_inputs_main_menu(State *state) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR)) {
    printf("starting game\n");
    state->game_mode = GAMEMODE_PLAYING;
  }
}

void process_inputs(State *state, f32 dt) {
  update_key_inputs_glfw(&state->inputs, state->context.window);

  switch (state->game_mode) {
  case GAMEMODE_PAUSED: {
    process_inputs_paused(state);
    break;
  }
  case GAMEMODE_PLAYING: {
    process_inputs_playing(state, dt);
    break;
  }
  case GAMEMODE_MAIN_MENU: {
    process_inputs_main_menu(state);
    break;
  }
  }
}

void write_uniforms(State *state) {}
