#include "generated_shader_utils.h"

#include "camera.h"
#include "glfw_vulkan.h"
#include "physics.h"
#include "pong.h"
#include "statistics.h"
#include "transform.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_base.h"
#include "vulkan/vulkan_core.h"
#include "window.h"

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/pong/field_background.jpg", "textures/girl_face.jpg",
    "textures/girl_face_normal_map.jpg"
};

void init_buffers(State *state) {
  VulkanContext *ctx = &state->ctx;

  // big vertex/index buffers
  BufferUploadQueue buffer_upload_queue = create_buffer_upload_queue();

  UPLOAD_VERTEX_ARRAY(buffer_upload_queue, paddle_vertices);
  UPLOAD_INDEX_ARRAY(buffer_upload_queue, unit_square_indices);

  state->buffer_manager = flush_buffers(ctx, &buffer_upload_queue);

  // uniform buffer
  // TODO can I come up with a scheme for coordinating UBOs and the location of
  // what data in what portion of the renderer maps to what data in the shaders?
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  state->uniform_writes.camera_vp = push_uniform(&ub_manager, sizeof(VPUniform));
  state->uniform_writes.arena_model = push_uniform(&ub_manager, sizeof(glm::mat4));
  state->uniform_writes.instance_data = push_uniform(&ub_manager, sizeof(InstanceDataUBO));
  state->uniform_buffer = create_uniform_buffer(ctx, ub_manager.current_offset);
}

void init_background_material(State *state) {
  init_program_spec(&state->ctx, state->ctx.render_pass, &pong_background_program_spec, &state->background_material);

  VulkanMaterial *mat = &state->background_material;
  VkBuffer ub = state->uniform_buffer.vulkan_buffer.buffer;

  VkDescriptorImageInfo image_info = {
      .sampler = state->sampler,
      .imageView = state->textures[TEXTURE_FIELD_BACKGROUND].image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkDescriptorBufferInfo vp_info = {
      .buffer = ub,
      .offset = state->uniform_writes.camera_vp.offset,
      .range = state->uniform_writes.camera_vp.size,
  };
  VkDescriptorBufferInfo model_info = {
      .buffer = ub,
      .offset = state->uniform_writes.arena_model.offset,
      .range = state->uniform_writes.arena_model.size,
  };

  // set 0 = PLACEHOLDER, set 1 = PONG_GLOBAL
  VkWriteDescriptorSet writes[3];
  writes[0] = fill_write(mat, 0, 1);
  writes[0].pImageInfo = &image_info;
  writes[1] = fill_write(mat, 1, 0);
  writes[1].pBufferInfo = &vp_info;
  writes[2] = fill_write(mat, 1, 1);
  writes[2].pBufferInfo = &model_info;
  vkUpdateDescriptorSets(state->ctx.device, 3, writes, 0, NULL);
}

void init_paddles_material(State *state) {
  init_program_spec(&state->ctx, state->ctx.render_pass, &pong_paddle_program_spec, &state->paddle_material);

  VulkanMaterial *mat = &state->paddle_material;
  VkBuffer ub = state->uniform_buffer.vulkan_buffer.buffer;

  VkDescriptorBufferInfo vp_info = {
      .buffer = ub,
      .offset = state->uniform_writes.camera_vp.offset,
      .range = state->uniform_writes.camera_vp.size,
  };
  VkDescriptorBufferInfo instance_info = {
      .buffer = ub,
      .offset = state->uniform_writes.instance_data.offset,
      .range = state->uniform_writes.instance_data.size,
  };

  // set 0 = PONG_GLOBAL
  VkWriteDescriptorSet writes[2];
  writes[0] = fill_write(mat, 0, 0);
  writes[0].pBufferInfo = &vp_info;
  writes[1] = fill_write(mat, 0, 2);
  writes[1].pBufferInfo = &instance_info;
  vkUpdateDescriptorSets(state->ctx.device, 2, writes, 0, NULL);
}

static void init_transforms(State *state) {
  Transform *transforms = state->transforms;

  transforms[ENTITY_LEFT_PADDLE] = create_transform(&state->positions[ENTITY_LEFT_PADDLE]);
  transforms[ENTITY_LEFT_PADDLE].scale = &paddle_scale0;

  transforms[ENTITY_RIGHT_PADDLE] = create_transform(&state->positions[ENTITY_RIGHT_PADDLE]);
  transforms[ENTITY_RIGHT_PADDLE].scale = &paddle_scale0;

  transforms[ENTITY_BALL] = create_transform(&state->positions[ENTITY_BALL]);
  transforms[ENTITY_BALL].scale = &ball_scale0;

  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    generate_transform(&state->transforms[i], &state->instance_data.model[i]);
  }

  write_to_uniform_buffer(&state->uniform_buffer, &state->instance_data, state->uniform_writes.instance_data);
}

State setup_state(const char *title) {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);

  State state = {
      .current_frame = 0,
      .arena_dimensions = arena_dimensions0,
      .game_mode = GAMEMODE_MAIN_MENU,
      .movement_mode = MOVEMENT_MODE_VERTICAL_ONLY,
      .pong_mode = PONG_MODE_BETWEEN_POINTS,
      .left_score = state.right_score = 0,
      .window = window,
      .ctx = create_vulkan_context(title, window_info),
  };

  VulkanContext *ctx = &state.ctx;

  STBHandle stbs[NUM_TEXTURES];
  u32 max_size = 0;
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    stbs[i] = load_texture(texture_names[i]);
    u32 texture_size = stbs[i].width * stbs[i].height * stbs[i].n_channels;
    max_size = (texture_size > max_size) ? texture_size : max_size;
  }

  VulkanBuffer staging_buffer = create_buffer(ctx, BUFFER_TYPE_STAGING, max_size);
  void *texture_data;
  VkResult result =
      vkMapMemory(ctx->device, staging_buffer.memory, 0, staging_buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    state.textures[i] = create_vulkan_texture(
        ctx, stbs[i].width, stbs[i].height, stbs[i].n_channels, stbs[i].data, staging_buffer, texture_data
    );
    free_stb_handle(&stbs[i]);
  }

  vkUnmapMemory(ctx->device, staging_buffer.memory);
  destroy_vulkan_buffer(ctx, staging_buffer);

  init_buffers(&state);

  // TODO this sampler never changes - look into wiring immutable samplers
  // All this stuff needs to be managed in the backend. I don't know how to
  // manage samplers at all.
  state.sampler = create_sampler(ctx->device);
  set_descriptor_set_layouts(&state.ctx, state.descriptor_set_layouts, NUM_DESCRIPTOR_SET_LAYOUTS);
  init_inputs(&state.inputs);

  state.clear_values[0].color = {{0.0, 1.0, 0.0, 1.0}};
  state.clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
  state.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0);

  init_background_material(&state);
  init_paddles_material(&state);

  state.positions[ENTITY_LEFT_PADDLE] = left_paddle_pos0;
  state.positions[ENTITY_RIGHT_PADDLE] = right_paddle_pos0;
  state.positions[ENTITY_BALL] = ball_pos0;

  state.velocities[ENTITY_LEFT_PADDLE] = glm::vec3(0.0f);
  state.velocities[ENTITY_RIGHT_PADDLE] = glm::vec3(0.0f);
  state.velocities[ENTITY_BALL] = glm::vec3(0.0f);

  state.scales[ENTITY_LEFT_PADDLE] = paddle_scale0;
  state.scales[ENTITY_RIGHT_PADDLE] = paddle_scale0;
  state.scales[ENTITY_BALL] = ball_scale0;

  state.left_paddle_speed = speed0;
  state.right_paddle_speed = speed0;
  state.ball_speed = speed0;

  state.time_since_last_powerup_draw = 0.0f;

  state.rngs.ball_direction = create_rng(0x69);
  state.rngs.powerup_spawn = create_rng(0x420);
  init_transforms(&state);

  glm::vec3 camera_pos{0.0f, 0.0f, 30.0f};
  state.camera = create_camera(CAMERA_TYPE_3D, camera_pos);
  state.camera.y_needs_inverted = true;

  // TODO how to get model onto GPU? uniform? something else? push constant?
  glm::mat4 arena_model = glm::scale(glm::mat4(1.0f), arena_dimensions0);

  // TODO make camera matrices only on camera movement
  // TODO buffer only on resize
  const CameraMatrices camera_matrices =
      create_camera_matrices(&state.camera, state.ctx.swapchain_extent.width, state.ctx.swapchain_extent.height);
  glm::mat4 camera_vp = camera_matrices.projection * camera_matrices.view;

  // uniform buffer structure: camera vp, background model, paddle model
  write_to_uniform_buffer(&state.uniform_buffer, &camera_vp, state.uniform_writes.camera_vp);
  write_to_uniform_buffer(&state.uniform_buffer, &arena_model, state.uniform_writes.arena_model);

  state.screen_shake.x_oscillator.phase = 0.0f;
  state.screen_shake.x_oscillator.omega = 10.0f;
  state.screen_shake.x_oscillator.decay_constant = 4.0f;
  state.screen_shake.x_oscillator.amplitude = 0.3f;

  state.screen_shake.y_oscillator.phase = 45.0f;
  state.screen_shake.y_oscillator.omega = 10.0f;
  state.screen_shake.y_oscillator.decay_constant = 4.0f;
  state.screen_shake.y_oscillator.amplitude = 0.3f;

  state.screen_shake.active = false;
  state.screen_shake.time_elapsed = 0.0f;
  state.screen_shake.cutoff_duration = 0.5f;

  init_alias_method(&state.powerup_alias_table, NUM_POWERUPS, powerup_likelihoods, 0x69420);
  log_alias_method(&state.powerup_alias_table);
  state.left_paddle_powerup_flags = 0;
  state.right_paddle_powerup_flags = 0;
  state.last_paddle_to_hit = LAST_PADDLE_NEITHER;

  state.left_paddle_cooldown = 0.0f;
  state.right_paddle_cooldown = 0.0f;

  for (u32 i = 0; i < MAX_POWERUPS; i++) {
  }

  return state;
}

void destroy_state(State *state) {
  VulkanContext *ctx = &state->ctx;

  vkDeviceWaitIdle(ctx->device);

  // texture and sampler
  vkDestroySampler(ctx->device, state->sampler, NULL);
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(ctx->device, &state->textures[i]);
  }

  destroy_vulkan_material(ctx->device, &state->background_material);
  destroy_vulkan_material(ctx->device, &state->paddle_material);

  destroy_buffer_manager(&state->buffer_manager);
  destroy_uniform_buffer(ctx, &state->uniform_buffer);
  destroy_vulkan_context(ctx);
}

void render(State *state) {
  VulkanContext *ctx = &state->ctx;

  begin_frame(ctx);

  VkCommandBuffer command_buffer = begin_command_buffer(ctx);
  begin_render_pass(
      ctx, command_buffer, ctx->render_pass, ctx->framebuffers[ctx->image_index], state->clear_values, 2,
      state->viewport_state
  );
  vkCmdSetViewport(command_buffer, 0, 1, &state->viewport_state.viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &state->viewport_state.scissor);

  VulkanMesh mesh = {
      .num_indices = 6,
      .instance_count = 1,
      .num_vertex_buffers = 1,
      .vertex_buffers = {state->buffer_manager.vertex_buffer.buffer},
      .vertex_buffer_offsets = {0},
      .index_buffer = state->buffer_manager.index_buffer.buffer,
  };

  render_mesh_material(command_buffer, &mesh, &state->background_material);

  mesh.instance_count = InstanceDataUBO_model_array_size;
  render_mesh_material(command_buffer, &mesh, &state->paddle_material);

  vkCmdEndRenderPass(command_buffer);
  VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
  submit_and_present(ctx, command_buffer);

  state->current_frame++;
  update_frame_index(ctx);
}

void process_inputs_paused(State *state) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    printf("unpausing\n");
    state->game_mode = GAMEMODE_PLAYING;
  }
}

void process_inputs_playing(State *state, f32 dt) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    printf("pausing\n");
    state->game_mode = GAMEMODE_PAUSED;
  }

  bool horizontal_enabled = (state->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED);
  glm::vec2 input_direction = inputs_to_direction(inputs);
  input_direction.x *= horizontal_enabled;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR) && state->pong_mode == PONG_MODE_BETWEEN_POINTS) {

    // TODO engine: convert to a branchless scheme
    f32 theta = 2 * PI * random_f32_xoroshiro128_plus(&state->rngs.ball_direction);
    const f32 half_pi = PI / 2.0f;
    const f32 epsilon = PI / 16.0f;
    const f32 top_min = half_pi - epsilon;
    const f32 top_max = half_pi + epsilon;
    const f32 bottom_min = 3 * half_pi - epsilon;
    const f32 bottom_max = 3 * half_pi + epsilon;

    while (interval_contains(theta, top_min, top_max) || interval_contains(theta, bottom_min, bottom_max)) {
      theta = random_f32_xoroshiro128_plus(&state->rngs.ball_direction);
    }

    const f32 x = cosf(theta);
    const f32 y = sinf(theta);
    state->velocities[ENTITY_BALL].x = state->ball_speed * x;
    state->velocities[ENTITY_BALL].y = state->ball_speed * y;

    state->pong_mode = PONG_MODE_LIVE_BALL;
  }

  // TODO Pong: replace with powerup
  if (key_pressed(inputs, INPUT_KEY_H)) {
    state->movement_mode = (state->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED)
                               ? MOVEMENT_MODE_VERTICAL_ONLY
                               : MOVEMENT_MODE_HORIZONTAL_ENABLED;
  }

  // TODO Pong: make the paddle scale over the course of the game
  if (input_direction.length() > EPSILON) {
    glm::vec3 movement = glm::vec3(input_direction.x, input_direction.y, 0.0f);
    state->positions[ENTITY_LEFT_PADDLE] += dt * state->left_paddle_speed * movement;
    state->transforms[ENTITY_LEFT_PADDLE].dirty = true;
  }
}

void process_inputs_main_menu(State *state) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR)) {
    printf("starting game\n");
    state->game_mode = GAMEMODE_PLAYING;
  }
}

void process_inputs(State *state, const f32 dt) {
  update_key_inputs_glfw(&state->inputs, state->window);

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

void handle_collisions(State *state, const f32 dt) {
  glm::vec3 *positions = state->positions;
  glm::vec3 *scales = state->scales;
  glm::vec3 *velocities = state->velocities;

  glm::vec3 ball_pos = positions[ENTITY_BALL];
  glm::vec3 ball_scale = scales[ENTITY_BALL];
  glm::vec3 ball_velocity = velocities[ENTITY_BALL];

  glm::vec3 left_paddle_pos = positions[ENTITY_LEFT_PADDLE];
  glm::vec3 left_paddle_scale = scales[ENTITY_LEFT_PADDLE];
  glm::vec3 left_paddle_velocity = velocities[ENTITY_LEFT_PADDLE];

  glm::vec3 right_paddle_pos = positions[ENTITY_RIGHT_PADDLE];
  glm::vec3 right_paddle_scale = scales[ENTITY_RIGHT_PADDLE];
  // glm::vec3 right_paddle_velocity = velocities[ENTITY_RIGHT_PADDLE];

  // TODO Pong: Maybe have this resize dynamically
  f32 arena_horizontal_boundary = arena_dimensions_x0 / 2.0f;
  f32 arena_vertical_boundary = arena_dimensions_y0 / 2.0f;

  // collisions with boundaries
  if (ball_pos.x + 0.5f * ball_scale.x > arena_horizontal_boundary) {
    state->left_score++;
    positions[ENTITY_BALL] = ball_pos0;
    state->pong_mode = PONG_MODE_BETWEEN_POINTS;
    state->velocities[ENTITY_BALL] = glm::vec3(0.0f);
  }
  if (ball_pos.x - 0.5f * ball_scale.x < -arena_horizontal_boundary) {
    state->right_score++;
    positions[ENTITY_BALL] = ball_pos0;
    state->pong_mode = PONG_MODE_BETWEEN_POINTS;
    state->velocities[ENTITY_BALL] = glm::vec3(0.0f);
  }

  if (ball_pos.y + 0.5f * ball_scale.y > arena_vertical_boundary) {
    state->velocities[ENTITY_BALL].y = -fabs(state->velocities[ENTITY_BALL].y);
    ball_pos.y = arena_vertical_boundary - 0.5f * ball_scale.y;
  }
  if (ball_pos.y - 0.5f * ball_scale.y < -arena_vertical_boundary) {
    state->velocities[ENTITY_BALL].y = fabs(state->velocities[ENTITY_BALL].y);
    ball_pos.y = -arena_vertical_boundary + 0.5f * ball_scale.y;
  }

  // paddle intersects with arena boundary
  // left paddle
  // vertical
  if (left_paddle_pos.y + 0.5f * left_paddle_scale.y > arena_vertical_boundary) {
    positions[ENTITY_LEFT_PADDLE].y = arena_vertical_boundary - 0.5f * left_paddle_scale.y;
  }

  if (left_paddle_pos.y - 0.5f * left_paddle_scale.y < -arena_vertical_boundary) {
    positions[ENTITY_LEFT_PADDLE].y = -arena_vertical_boundary + 0.5f * left_paddle_scale.y;
  }

  // horizontal
  if (left_paddle_pos.x + 0.5f * left_paddle_scale.x > arena_horizontal_boundary) {
    positions[ENTITY_LEFT_PADDLE].x = arena_horizontal_boundary - 0.5f * left_paddle_scale.x;
  }

  if (left_paddle_pos.x - 0.5f * left_paddle_scale.x < -arena_horizontal_boundary) {
    positions[ENTITY_LEFT_PADDLE].x = -arena_horizontal_boundary + 0.5f * left_paddle_scale.x;
  }

  // right paddle
  if (right_paddle_pos.y - 0.5f * right_paddle_scale.y < -arena_vertical_boundary) {
    positions[ENTITY_RIGHT_PADDLE].y = -arena_vertical_boundary + 0.5f * right_paddle_scale.y;
  }

  if (right_paddle_pos.y + 0.5f * right_paddle_scale.y > arena_vertical_boundary) {
    positions[ENTITY_RIGHT_PADDLE].y = arena_vertical_boundary - 0.5f * right_paddle_scale.y;
  }

  if (state->left_paddle_cooldown <= 0.0f) {
    // ball paddle collisions
    SweptAABBCollisionCheck left_collision_check = swept_aabb_collision(
        dt, left_paddle_pos, left_paddle_scale, left_paddle_velocity, ball_pos, ball_scale, ball_velocity
    );

    if (left_collision_check.did_collide) {
      state->left_paddle_cooldown = 1.0f;
      glm::vec3 normal = left_collision_check.normal;
      log_vec3(&normal);
      state->last_paddle_to_hit = LAST_PADDLE_LEFT;
      // state->screen_shake.active = true;

      // normal is from paddles's perspective
      if (left_collision_check.was_overlapping) {
        const glm::vec3 displacement = normal * left_collision_check.penetration_depth;
        state->positions[ENTITY_BALL] += displacement;
      } else {
        state->positions[ENTITY_BALL] += ball_velocity * left_collision_check.t;
      }

      state->velocities[ENTITY_BALL] = ball_velocity - 2.0f * glm::dot(ball_velocity, normal) * normal;
    }
  } else {
    state->left_paddle_cooldown -= dt;
  }

  if (aabb_collision(ball_pos, ball_scale, right_paddle_pos, right_paddle_scale)) {
    state->last_paddle_to_hit = LAST_PADDLE_RIGHT;
    state->velocities[ENTITY_BALL].x = -state->velocities[ENTITY_BALL].x;
    // state->screen_shake.active = true;
  }
}

void update_screen_shake(State *state, f32 dt) {

  i32 width = state->ctx.swapchain_extent.width;
  i32 height = state->ctx.swapchain_extent.height;

  CameraMatrices camera_matrices;
  state->screen_shake.time_elapsed += dt;

  if (state->screen_shake.time_elapsed > state->screen_shake.cutoff_duration) {
    camera_matrices = create_camera_matrices(&state->camera, width, height);
    state->screen_shake.time_elapsed = 0.0f;
    state->screen_shake.active = false;
  } else {
    f32 dx = evaluate_damped_harmonic_oscillator(state->screen_shake.x_oscillator, state->screen_shake.time_elapsed);
    f32 dy = evaluate_damped_harmonic_oscillator(state->screen_shake.y_oscillator, state->screen_shake.time_elapsed);

    camera_matrices = camera_matrices_with_offset(&state->camera, glm::vec3(dx, dy, 0.0f), width, height);
  }

  glm::mat4 camera_vp = camera_matrices.projection * camera_matrices.view;
  write_to_uniform_buffer(&state->uniform_buffer, &camera_vp, state->uniform_writes.camera_vp);
}

void update_game_state(State *state, const f32 dt) {

  if (state->game_mode == GAMEMODE_PAUSED || state->game_mode == GAMEMODE_MAIN_MENU) {
    return;
  }

  state->time_since_last_powerup_draw += dt;
  if (state->time_since_last_powerup_draw > powerup_draw_interval_in_sec) {
    state->time_since_last_powerup_draw -= powerup_draw_interval_in_sec;

    f32 prob_to_spawn = random_f32_xoroshiro128_plus(&state->rngs.powerup_spawn);
    if (prob_to_spawn < prob_powerup_spawns) {
      // u32 powerup_index = draw_alias_method(&state->powerup_alias_table);
    }
  }

  if (state->positions[ENTITY_BALL].y > state->positions[ENTITY_RIGHT_PADDLE].y) {
    state->velocities[ENTITY_RIGHT_PADDLE].y = cpu_speed0;
  } else {
    state->velocities[ENTITY_RIGHT_PADDLE].y = -cpu_speed0;
  }

  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    state->positions[i] += dt * state->velocities[i];
  }

  handle_collisions(state, dt);

  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    Transform *transform = &state->transforms[i];
    generate_transform(transform, &state->instance_data.model[i]);
  }

  write_to_uniform_buffer(&state->uniform_buffer, &state->instance_data, state->uniform_writes.instance_data);

  if (state->screen_shake.active) {
    update_screen_shake(state, dt);
  }
}
