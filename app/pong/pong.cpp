#include "assets.h"
#include "generated_shader_utils.h"

#include "camera.h"
#include "glfw_vulkan.h"
#include "linalg.h"
#include "physics.h"
#include "pong.h"
#include "shaders.h"
#include "statistics.h"
#include "tuke_engine.h"
#include "vulkan_base.h"
#include "window.h"

static const f32 MENU_UI_WIDTH = 0.7f;

static const Vec2 menu_ui_pos[NUM_MENU_UIS] = {
    [MENU_UI_LOGO] = vec2(0.0f, -0.5f),
    [MENU_UI_STORY] = vec2(0.0f, 0.0),
    [MENU_UI_1V1] = vec2(0.0f, 0.3f),
    [MENU_UI_OPTIONS] = vec2(0.0f, 0.6f),
};

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/pong/field_background.jpg", "textures/girl_face.jpg",
    "textures/girl_face_normal_map.jpg"
};

void init_buffers(State *state) {
  VulkanContext *ctx = &state->ctx;

  // big vertex/index buffers
  // This should not be managed by the state raw - need a renderer or something
  // This is also not very clear - I forget why one mesh handles unit square indices and paddle vertices
  state->buffer_manager = create_buffer_manager();
  VulkanMesh *mesh =
      UPLOAD_ARRAYS(state->buffer_manager, paddle_vertices, unit_square_indices, ARRAY_SIZE(unit_square_indices));
  flush_buffers(ctx, &state->buffer_manager);
  state->mesh = *mesh;

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  VkDescriptorBufferInfo *camera_vp = push_uniform(&ub_manager, sizeof(VPUniform));
  VkDescriptorBufferInfo *arena_model = push_uniform(&ub_manager, sizeof(ModelUniform));
  VkDescriptorBufferInfo *instance_data = push_uniform(&ub_manager, sizeof(InstanceDataUBO));
  state->uniform_buffer = finalize_ub(ctx, &ub_manager);
  state->uniform_writes.camera_vp = *camera_vp;
  state->uniform_writes.arena_model = *arena_model;
  state->uniform_writes.instance_data = *instance_data;
}

void init_background_material(State *state) {
  init_program_spec(&state->ctx, state->ctx.render_pass, NULL, &pong_background_program_spec, &state->background_mat);

  VkDescriptorImageInfo image_info = {
      .sampler = state->sampler,
      .imageView = state->textures[TEXTURE_FIELD_BACKGROUND].image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  DescriptorWrite writes[] = {
      {
          .set_id = LAYOUT_ID_PLACEHOLDER,
          .binding = BINDING_PLACEHOLDER_TEX,
          .image_info = image_info,
      },
      {
          .set_id = LAYOUT_ID_PONG_GLOBAL,
          .binding = BINDING_PONG_GLOBAL_VP,
          .buffer_info = state->uniform_writes.camera_vp,
      },
      {
          .set_id = LAYOUT_ID_PONG_GLOBAL,
          .binding = BINDING_PONG_GLOBAL_BACKGROUND_MODEL,
          .buffer_info = state->uniform_writes.arena_model,
      },
  };
  update_vulkan_material(&state->ctx, writes, ARRAY_SIZE(writes), &state->background_mat);
}

void init_paddles_material(State *state) {
  init_program_spec(&state->ctx, state->ctx.render_pass, NULL, &pong_paddle_program_spec, &state->paddle_mat);
  const UniformWrites *ws = &state->uniform_writes;
  DescriptorWrite writes[] = {
      {
          .set_id = LAYOUT_ID_PONG_GLOBAL,
          .binding = BINDING_PONG_GLOBAL_VP,
          .buffer_info = ws->camera_vp,
      },
      {
          .set_id = LAYOUT_ID_PONG_GLOBAL,
          .binding = BINDING_PONG_GLOBAL_INSTANCE_DATA,
          .buffer_info = ws->instance_data,
      },
  };
  update_vulkan_material(&state->ctx, writes, ARRAY_SIZE(writes), &state->paddle_mat);
}

static void init_transforms(State *state) {
  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    Mat4 *m = &state->instance_data.model[i];
    *m = mat4();
    scale_m4(state->scales[i], m);
    translate_m4(state->positions[i], m);
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
      .menu_state = {.intro_active = true, .selected_option = MENU_UI_STORY},
  };

  VulkanContext *ctx = &state.ctx;
  load_vulkan_textures(ctx, texture_names, NUM_TEXTURES, state.textures);
  init_buffers(&state);

  state.sampler = create_sampler(ctx->device);
  set_descriptor_set_layouts(&state.ctx, state.descriptor_set_layouts, NUM_DESCRIPTOR_SET_LAYOUTS);
  init_inputs(&state.inputs);

  state.clear_values[0].color = {{0.0, 1.0, 0.0, 1.0}};
  state.clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
  state.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0);

  init_background_material(&state);
  init_paddles_material(&state);
  PipelineConfig ui_conf = vulkan_pipeline_config();
  ui_conf.depth_test = false;
  init_program_spec(ctx, ctx->render_pass, &ui_conf, &pong_main_menu_program_spec, &state.main_menu_mat);
  init_program_spec(ctx, ctx->render_pass, &ui_conf, &common_ui_quad_program_spec, &state.ui_mat);

  state.ui_buffer = create_streaming_buffer(ctx, sizeof(state.ui_elements));

  // End renderer

  state.positions[ENTITY_LEFT_PADDLE] = left_paddle_pos0;
  state.positions[ENTITY_RIGHT_PADDLE] = right_paddle_pos0;
  state.positions[ENTITY_BALL] = ball_pos0;

  state.velocities[ENTITY_LEFT_PADDLE] = Vec3(0.0f);
  state.velocities[ENTITY_RIGHT_PADDLE] = Vec3(0.0f);
  state.velocities[ENTITY_BALL] = Vec3(0.0f);

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

  Vec3 camera_pos{0.0f, 0.0f, 30.0f};
  state.camera = create_camera(CAMERA_TYPE_3D, camera_pos);
  state.camera.y_needs_inverted = true;

  // TODO how to get model onto GPU? uniform? something else? push constant?
  Mat4 arena_model = mat4();
  scale_m4(arena_dimensions0, &arena_model);
  f32 aspect = f32(state.ctx.swapchain_extent.width) / f32(state.ctx.swapchain_extent.height);
  Mat4 vp = make_camera_vp(&state.camera, aspect);

  // uniform buffer structure: camera vp, background model, paddle model
  write_to_uniform_buffer(&state.uniform_buffer, &vp, state.uniform_writes.camera_vp);
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

  destroy_vulkan_material(ctx->device, &state->background_mat);
  destroy_vulkan_material(ctx->device, &state->paddle_mat);
  destroy_vulkan_material(ctx->device, &state->main_menu_mat);
  destroy_vulkan_material(ctx->device, &state->ui_mat);

  destroy_buffer_manager(&state->buffer_manager);
  destroy_uniform_buffer(ctx, &state->uniform_buffer);
  destroy_streaming_buffer(ctx, &state->ui_buffer);
  destroy_vulkan_context(ctx);
}

void render(State *s) {
  VulkanContext *ctx = &s->ctx;
  begin_frame(ctx);
  VkCommandBuffer cmd = begin_command_buffer(ctx);
  begin_render_pass(
      ctx, cmd, ctx->render_pass, ctx->framebuffers[ctx->image_index], s->clear_values, 2, s->viewport_state
  );

  switch (s->game_mode) {
  case GAMEMODE_PLAYING:
  case GAMEMODE_PAUSED:
    render_mesh(cmd, &s->mesh, &s->background_mat);
    render_mesh_instanced(cmd, &s->mesh, &s->paddle_mat, InstanceDataUBO_model_array_size, NULL);
    break;
  case GAMEMODE_MAIN_MENU: {
    VulkanMesh mesh = {.vertex_count = 6};
    Vec2 res = vec2(s->window_width, s->window_height);
    f32 selector_size = s->menu_state.intro_active ? 0.0f : 0.05f;
    Vec4 data0 = vec4(s->menu_state.selector_pos.x, s->menu_state.selector_pos.y, selector_size, 0.0f);
    ShaderToy st = {.res = res, .t = (f32)s->menu_state.anim_t, .data0 = data0};
    push_constants_material(cmd, &s->main_menu_mat, &st);
    render_mesh(cmd, &mesh, &s->main_menu_mat);

    u32 instance_count = s->menu_state.intro_active ? 0 : NUM_MENU_UIS;
    write_streaming_buffer(&s->ui_buffer, s->ui_elements, 0, sizeof(s->ui_elements));
    render_mesh_instanced(cmd, &mesh, &s->ui_mat, instance_count, &s->ui_buffer);
    break;
  }
  default:
    assert(false);
  }

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
  submit_and_present(ctx, cmd);

  s->current_frame++;
  update_frame_index(ctx);
}

static bool intersect_ui(const UiElement *ui, f32 x0, f32 y0) {
  Vec2 w = scale_v2(ui->size, 0.5);
  Vec2 cen = ui->center;
  f32 s = sinf(ui->rotation);
  f32 c = cosf(ui->rotation);
  f32 x = c * x0 - s * y0;
  f32 y = s * x0 + c * y0;
  f32 x_min = cen.x - w.x;
  f32 x_max = cen.x + w.x;
  f32 y_min = cen.y - w.y;
  f32 y_max = cen.y + w.y;
  bool in_x = x_min < x && x < x_max;
  bool in_y = y_min < y && y < y_max;
  return in_x && in_y;
}

void process_inputs_paused(State *state) {
  const Inputs *inputs = &state->inputs;

  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    printf("unpausing\n");
    state->game_mode = GAMEMODE_PLAYING;
  }
}

void process_inputs_playing(State *st, f32 dt) {
  const Inputs *inputs = &st->inputs;

  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    printf("pausing\n");
    st->game_mode = GAMEMODE_PAUSED;
  }

  bool horizontal_enabled = (st->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED);
  Vec2 input_direction = inputs_to_direction(inputs);
  input_direction.x *= horizontal_enabled;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR) && st->pong_mode == PONG_MODE_BETWEEN_POINTS) {

    // TODO engine: convert to a branchless scheme
    f32 theta = 2 * PI * random_f32_xoroshiro128_plus(&st->rngs.ball_direction);
    const f32 half_pi = PI / 2.0f;
    const f32 epsilon = PI / 16.0f;
    const f32 top_min = half_pi - epsilon;
    const f32 top_max = half_pi + epsilon;
    const f32 bottom_min = 3 * half_pi - epsilon;
    const f32 bottom_max = 3 * half_pi + epsilon;

    while (interval_contains(theta, top_min, top_max) || interval_contains(theta, bottom_min, bottom_max)) {
      theta = random_f32_xoroshiro128_plus(&st->rngs.ball_direction);
    }

    const f32 x = cosf(theta);
    const f32 y = sinf(theta);
    st->velocities[ENTITY_BALL].x = st->ball_speed * x;
    st->velocities[ENTITY_BALL].y = st->ball_speed * y;

    st->pong_mode = PONG_MODE_LIVE_BALL;
  }

  // TODO replace with powerup
  if (key_pressed(inputs, INPUT_KEY_H)) {
    st->movement_mode = (st->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED) ? MOVEMENT_MODE_VERTICAL_ONLY
                                                                                : MOVEMENT_MODE_HORIZONTAL_ENABLED;
  }

  // TODO make the paddle scale over the course of the game
  if (len_v2(input_direction) > EPSILON) {
    Vec3 movement = Vec3(input_direction.x, input_direction.y, 0.0f);
    inc_v3(&st->positions[ENTITY_LEFT_PADDLE], scale_v3(movement, dt * st->left_paddle_speed));
  }
}

void press_menu_ui(State *st) {
  switch (st->menu_state.selected_option) {
  case MENU_UI_STORY:
    st->game_mode = GAMEMODE_PLAYING;
    break;
  case MENU_UI_1V1:
    break;
  case MENU_UI_OPTIONS:
    break;

  case MENU_UI_LOGO:
  default:
    return;
  }
}

void process_inputs_main_menu(State *st) {
  const Inputs *inputs = &st->inputs;
  MenuState *ms = &st->menu_state;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR)) {
    ms->intro_active = false;
  }

  if (ms->intro_active) {
    return;
  }

  f32 x0 = (f32)inputs->curr_cursor_x / (f32)st->window_width * 2.0f - 1.0f;
  f32 y0 = (f32)inputs->curr_cursor_y / (f32)st->window_height * 2.0f - 1.0f;
  if (mouse_moved(inputs)) {
    for (u32 i = MENU_UI_STORY; i < NUM_MENU_UIS; i++) {
      bool hit = intersect_ui(&st->ui_elements[i], x0, y0);
      if (hit) {
        st->menu_state.selected_option = (MenuUI)i;

        break;
      }
    }
  }

  if (lclick_pressed(inputs)) {
    bool hit = intersect_ui(&st->ui_elements[ms->selected_option], x0, y0);
    if (hit) {
      press_menu_ui(st);
    }
  } else if (key_pressed(inputs, INPUT_KEY_DOWN_ARROW)) {
    printf("down\n");
    switch (ms->selected_option) {
    case MENU_UI_STORY:
      ms->selected_option = MENU_UI_1V1;
      break;
    case MENU_UI_1V1:
      ms->selected_option = MENU_UI_OPTIONS;
      break;
    default:
      break;
    }
  } else if (key_pressed(inputs, INPUT_KEY_UP_ARROW)) {
    printf("up\n");
    switch (ms->selected_option) {
    case MENU_UI_1V1:
      ms->selected_option = MENU_UI_STORY;
      break;
    case MENU_UI_OPTIONS:
      ms->selected_option = MENU_UI_1V1;
      break;
    default:
      break;
    }
  } else if (key_pressed(inputs, INPUT_KEY_ENTER)) {
    press_menu_ui(st);
  }
}

void process_inputs(State *st, const f32 dt) {
  glfwGetFramebufferSize(st->window, &st->window_width, &st->window_height);
  update_inputs_glfw(&st->inputs, st->window);

  switch (st->game_mode) {
  case GAMEMODE_PAUSED: {
    process_inputs_paused(st);
    break;
  }
  case GAMEMODE_PLAYING: {
    process_inputs_playing(st, dt);
    break;
  }
  case GAMEMODE_MAIN_MENU: {
    process_inputs_main_menu(st);
    st->menu_state.anim_t += dt;
    st->menu_state.intro_active = st->menu_state.anim_t < INTRO_DURATION;
    break;
  }
  }
}

void handle_collisions(State *st, const f32 dt) {
  Vec3 *positions = st->positions;
  Vec3 *scales = st->scales;
  Vec3 *velocities = st->velocities;

  Vec3 ball_pos = positions[ENTITY_BALL];
  Vec3 ball_scale = scales[ENTITY_BALL];
  Vec3 ball_velocity = velocities[ENTITY_BALL];

  Vec3 left_paddle_pos = positions[ENTITY_LEFT_PADDLE];
  Vec3 left_paddle_scale = scales[ENTITY_LEFT_PADDLE];
  Vec3 left_paddle_velocity = velocities[ENTITY_LEFT_PADDLE];

  Vec3 right_paddle_pos = positions[ENTITY_RIGHT_PADDLE];
  Vec3 right_paddle_scale = scales[ENTITY_RIGHT_PADDLE];
  // Vec3 right_paddle_velocity = velocities[ENTITY_RIGHT_PADDLE];

  // TODO Maybe have this resize dynamically
  f32 arena_horizontal_boundary = arena_dimensions_x0 / 2.0f;
  f32 arena_vertical_boundary = arena_dimensions_y0 / 2.0f;

  // collisions with boundaries
  if (ball_pos.x + 0.5f * ball_scale.x > arena_horizontal_boundary) {
    st->left_score++;
    positions[ENTITY_BALL] = ball_pos0;
    st->pong_mode = PONG_MODE_BETWEEN_POINTS;
    st->velocities[ENTITY_BALL] = Vec3(0.0f);
  }
  if (ball_pos.x - 0.5f * ball_scale.x < -arena_horizontal_boundary) {
    st->right_score++;
    positions[ENTITY_BALL] = ball_pos0;
    st->pong_mode = PONG_MODE_BETWEEN_POINTS;
    st->velocities[ENTITY_BALL] = Vec3(0.0f);
  }

  if (ball_pos.y + 0.5f * ball_scale.y > arena_vertical_boundary) {
    st->velocities[ENTITY_BALL].y = -fabs(st->velocities[ENTITY_BALL].y);
    ball_pos.y = arena_vertical_boundary - 0.5f * ball_scale.y;
  }
  if (ball_pos.y - 0.5f * ball_scale.y < -arena_vertical_boundary) {
    st->velocities[ENTITY_BALL].y = fabs(st->velocities[ENTITY_BALL].y);
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

  if (st->left_paddle_cooldown <= 0.0f) {
    // ball paddle collisions
    SweptAABBCollisionCheck left_collision_check = swept_aabb_collision(
        dt, left_paddle_pos, left_paddle_scale, left_paddle_velocity, ball_pos, ball_scale, ball_velocity
    );

    if (left_collision_check.did_collide) {
      st->left_paddle_cooldown = 1.0f;
      Vec3 normal = left_collision_check.normal;
      log_v3(normal);
      st->last_paddle_to_hit = LAST_PADDLE_LEFT;
      // state->screen_shake.active = true;

      // normal is from paddles's perspective
      if (left_collision_check.was_overlapping) {
        const Vec3 displacement = scale_v3(normal, left_collision_check.penetration_depth);
        inc_v3(&st->positions[ENTITY_BALL], displacement);
      } else {
        inc_v3(&st->positions[ENTITY_BALL], scale_v3(ball_velocity, left_collision_check.t));
      }

      f32 asdf = 2.0f * dot_v3(ball_velocity, normal);
      st->velocities[ENTITY_BALL] = sub_v3(ball_velocity, scale_v3(normal, asdf));
    }
  } else {
    st->left_paddle_cooldown -= dt;
  }

  if (aabb_collision(ball_pos, ball_scale, right_paddle_pos, right_paddle_scale)) {
    st->last_paddle_to_hit = LAST_PADDLE_RIGHT;
    st->velocities[ENTITY_BALL].x = -st->velocities[ENTITY_BALL].x;
    // state->screen_shake.active = true;
  }
}

void update_screen_shake(State *state, f32 dt) {

  i32 width = state->ctx.swapchain_extent.width;
  i32 height = state->ctx.swapchain_extent.height;

  CameraMatrices camera_matrices;
  state->screen_shake.time_elapsed += dt;

  if (state->screen_shake.time_elapsed > state->screen_shake.cutoff_duration) {
    camera_matrices = create_camera_matrices(&state->camera, f32(width) / f32(height));
    state->screen_shake.time_elapsed = 0.0f;
    state->screen_shake.active = false;
  } else {
    f32 dx = evaluate_damped_harmonic_oscillator(state->screen_shake.x_oscillator, state->screen_shake.time_elapsed);
    f32 dy = evaluate_damped_harmonic_oscillator(state->screen_shake.y_oscillator, state->screen_shake.time_elapsed);

    Vec3 offset = vec3(dx, dy, 0.0f);
    camera_matrices = camera_matrices_with_offset(&state->camera, offset, f32(width) / f32(height));
  }

  Mat4 camera_vp;
  mult_m4(&camera_matrices.projection, &camera_matrices.view, &camera_vp);
  write_to_uniform_buffer(&state->uniform_buffer, &camera_vp, state->uniform_writes.camera_vp);
}

void update_game_state(State *st, const f32 dt) {

  if (st->game_mode == GAMEMODE_PAUSED) {
    return;
  }

  if (st->game_mode == GAMEMODE_MAIN_MENU) {
    Vec2 logo_size = vec2(1.0f, 0.3f);
    Vec2 option_size = vec2(MENU_UI_WIDTH, 0.2f);
    st->menu_state.selector_pos = vec2(-1.1 * MENU_UI_WIDTH, menu_ui_pos[st->menu_state.selected_option].y);
    st->ui_elements[MENU_UI_LOGO] = {.center = vec2(0.0f, -0.5f), .rotation = 0.0f, .size = logo_size};
    st->ui_elements[MENU_UI_STORY] = {.center = menu_ui_pos[MENU_UI_STORY], .rotation = 0.0f, .size = option_size};
    st->ui_elements[MENU_UI_1V1] = {.center = menu_ui_pos[MENU_UI_1V1], .rotation = 0.0f, .size = option_size};
    st->ui_elements[MENU_UI_OPTIONS] = {.center = menu_ui_pos[MENU_UI_OPTIONS], .rotation = 0.0f, .size = option_size};
    return;
  }

  st->time_since_last_powerup_draw += dt;
  if (st->time_since_last_powerup_draw > powerup_draw_interval_in_sec) {
    st->time_since_last_powerup_draw -= powerup_draw_interval_in_sec;

    f32 prob_to_spawn = random_f32_xoroshiro128_plus(&st->rngs.powerup_spawn);
    if (prob_to_spawn < prob_powerup_spawns) {
      // u32 powerup_index = draw_alias_method(&state->powerup_alias_table);
    }
  }

  if (st->positions[ENTITY_BALL].y > st->positions[ENTITY_RIGHT_PADDLE].y) {
    st->velocities[ENTITY_RIGHT_PADDLE].y = cpu_speed0;
  } else {
    st->velocities[ENTITY_RIGHT_PADDLE].y = -cpu_speed0;
  }

  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    inc_v3(&st->positions[i], scale_v3(st->velocities[i], dt));
  }

  handle_collisions(st, dt);

  for (u32 i = 0; i < NUM_ENTITIES; i++) {
    Mat4 *m = &st->instance_data.model[i];
    *m = mat4();
    scale_m4(st->scales[i], m);
    translate_m4(st->positions[i], m);
  }

  write_to_uniform_buffer(&st->uniform_buffer, &st->instance_data, st->uniform_writes.instance_data);

  if (st->screen_shake.active) {
    update_screen_shake(st, dt);
  }
}
