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
    [MAIN_MENU_UI_LOGO] = vec2(0.0f, -0.5f),
    [MAIN_MENU_UI_STORY] = vec2(0.0f, 0.0),
    [MAIN_MENU_UI_1V1] = vec2(0.0f, 0.3f),
    [MAIN_MENU_UI_OPTIONS] = vec2(0.0f, 0.6f),
};

static const StringArray texture_paths[NUM_TEXTURES] = {
    TEX("textures/generic_girl.jpg"),
    TEX("textures/pong/field_background.jpg"),
    TEX("textures/girl_face.jpg"),
    TEX("textures/girl_face_normal_map.jpg"),
    {.paths = {"textures/generic_girl.jpg", "textures/girl_face.jpg"}, .num_paths = 2},
};

void init_buffers(Renderer *r) {
  VulkanContext *ctx = &r->ctx;
  r->buffer_manager = create_buffer_manager();
  VulkanMesh *mesh =
      UPLOAD_ARRAYS(r->buffer_manager, paddle_vertices, unit_square_indices, ARRAY_SIZE(unit_square_indices));
  flush_buffers(ctx, &r->buffer_manager);
  r->mesh = *mesh;
}

void init_background_material(Renderer *r) {
  init_program_spec(&r->ctx, r->ctx.render_pass, NULL, &pong_background_program_spec, &r->background_mat);

  VkDescriptorImageInfo image_info = {
      .sampler = r->ctx.samplers[SAMPLER_LINEAR_CLAMP],
      .imageView = r->textures[TEXTURE_FIELD_BACKGROUND].image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  DescriptorWrite writes[] = {
      {
          .set_id = LAYOUT_ID_PLACEHOLDER,
          .binding = BINDING_PLACEHOLDER_TEX,
          .image_info = image_info,
      },
  };
  update_vulkan_material(&r->ctx, writes, ARRAY_SIZE(writes), &r->background_mat);
}

State setup_state(const char *title) {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);

  State state = {
      .current_frame = 0,
      .game_mode = GAMEMODE_MAIN_MENU,
      .window = window,
      .menu_state = {.intro_active = true, .selected_option = MAIN_MENU_UI_STORY},
  };

  state.playing_state = {
      .left_score = 0,
      .right_score = 0,
      .pong_mode = PONG_MODE_BETWEEN_POINTS,
      .arena_dimensions = arena_dimensions0,
      .movement_mode = MOVEMENT_MODE_VERTICAL_ONLY,

      .screen_shake.x_oscillator.phase = 0.0f,
      .screen_shake.x_oscillator.omega = 10.0f,
      .screen_shake.x_oscillator.decay_constant = 4.0f,
      .screen_shake.x_oscillator.amplitude = 0.3f,

      .screen_shake.y_oscillator.phase = 45.0f,
      .screen_shake.y_oscillator.omega = 10.0f,
      .screen_shake.y_oscillator.decay_constant = 4.0f,
      .screen_shake.y_oscillator.amplitude = 0.3f,

      .screen_shake.active = false,
      .screen_shake.time_elapsed = 0.0f,
      .screen_shake.cutoff_duration = 0.5f,

      .left_paddle_powerup_flags = 0,
      .right_paddle_powerup_flags = 0,
      .last_paddle = PADDLE_NEITHER,

      .left_paddle_cooldown = 0.0f,
      .right_paddle_cooldown = 0.0f,

      .lpaddle_pos = left_paddle_pos0,
      .rpaddle_pos = right_paddle_pos0,
      .ball_pos = ball_pos0,

      .lpaddle_vel = Vec3(0.0f),
      .rpaddle_vel = Vec3(0.0f),
      .ball_vel = Vec3(0.0f),

      .lpaddle_size = paddle_scale0,
      .rpaddle_size = paddle_scale0,
      .ball_size = ball_scale0,

      .lpaddle_speed = speed0,
      .rpaddle_speed = speed0,
      .ball_speed = speed0,

      .time_since_last_powerup_draw = 0.0f,

      .rngs.ball_direction = create_rng(0x69),
      .rngs.powerup_spawn = create_rng(0x420),

      .camera = create_camera(CAMERA_TYPE_3D, vec3(0.0f, 2.0f, 30.0f)),
  };
  state.playing_state.camera.y_needs_inverted = true,

  init_alias_table(&state.playing_state.powerup_alias_table, NUM_POWERUP_TYPES, powerup_likelihoods, 0x69420);
  log_alias_table(&state.playing_state.powerup_alias_table);

  // Renderer
  state.renderer = {
      .ctx = create_vulkan_context(title, window_info),
      .clear_values[0].color = {{0.0, 1.0, 0.0, 1.0}},
      .clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0},
  };
  VulkanContext *ctx = &state.renderer.ctx;
  load_vulkan_textures(ctx, texture_paths, NUM_TEXTURES, state.renderer.textures);
  init_buffers(&state.renderer);
  state.renderer.viewport_state = create_viewport_state_xy(ctx->swapchain_extent, 0, 0),

  set_descriptor_set_layouts(ctx, state.renderer.descriptor_set_layouts, NUM_DESCRIPTOR_SET_LAYOUTS);
  init_inputs(&state.inputs);
  init_background_material(&state.renderer);
  init_program_spec(ctx, ctx->render_pass, NULL, &pong_paddle_program_spec, &state.renderer.paddle_mat);

  // UI material
  PipelineConfig ui_conf = vulkan_pipeline_config();
  ui_conf.depth_test = false;
  init_program_spec(ctx, ctx->render_pass, &ui_conf, &pong_main_menu_program_spec, &state.renderer.main_menu_mat);
  init_program_spec(ctx, ctx->render_pass, &ui_conf, &common_ui_quad_program_spec, &state.renderer.ui_mat);

  VkDescriptorImageInfo image_info = {
      .sampler = ctx->samplers[SAMPLER_LINEAR_CLAMP],
      .imageView = state.renderer.textures[TEXTURE_UI].image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  DescriptorWrite writes[] = {
      {
          .set_id = LAYOUT_ID_UI,
          .binding = BINDING_UI_TEX,
          .image_info = image_info,
      },
  };
  update_vulkan_material(ctx, writes, ARRAY_SIZE(writes), &state.renderer.ui_mat);
  state.renderer.ui_buffer = create_streaming_buffer(ctx, sizeof(state.ui_elements));
  // End renderer

  return state;
}

void destroy_state(State *state) {
  VulkanContext *ctx = &state->renderer.ctx;
  vkDeviceWaitIdle(ctx->device);

  // texture and sampler
  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(ctx->device, &state->renderer.textures[i]);
  }

  destroy_vulkan_material(ctx->device, &state->renderer.background_mat);
  destroy_vulkan_material(ctx->device, &state->renderer.paddle_mat);
  destroy_vulkan_material(ctx->device, &state->renderer.main_menu_mat);
  destroy_vulkan_material(ctx->device, &state->renderer.ui_mat);

  destroy_buffer_manager(&state->renderer.buffer_manager);
  destroy_uniform_buffer(ctx, &state->renderer.uniform_buffer);
  destroy_streaming_buffer(ctx, &state->renderer.ui_buffer);
  destroy_vulkan_context(ctx);
}

void render_menu(State *s, VkCommandBuffer cmd) {
  Renderer *r = &s->renderer;
  MenuState *ms = &s->menu_state;
  VulkanMesh mesh = {.vertex_count = 6};
  Vec2 res = vec2(s->window_width, s->window_height);

  switch (ms->menu_type) {
  case MENU_TYPE_MAIN: {
    f32 selector_size = ms->intro_active ? 0.0f : 0.05f;
    Vec4 data0 = vec4(ms->selector_pos.x, ms->selector_pos.y, selector_size, 0.0f);
    ShaderToy toy = {.res = res, .t = (f32)ms->anim_t, .data0 = data0};
    push_constants_material(cmd, &r->main_menu_mat, &toy);
    render_mesh(cmd, &mesh, &r->main_menu_mat);

    u32 instance_count = ms->intro_active ? 0 : NUM_MENU_UIS;
    write_streaming_buffer(&r->ui_buffer, s->ui_elements, 0, sizeof(s->ui_elements));
    render_mesh_instanced(cmd, &mesh, &r->ui_mat, instance_count, &r->ui_buffer);
    break;
  }
  case MENU_TYPE_1V1: {
    u32 portraits_processed = 0;
    u32 row_idx = 0;
    const u32 max_stride = (NUM_CHARACTERS + PLAYER_SELECT_ROWS - 1) / PLAYER_SELECT_ROWS;
    while (portraits_processed < NUM_CHARACTERS) {
      u32 portraits_left = NUM_CHARACTERS - portraits_processed;
      u32 num_row_portraits = max_stride < portraits_left ? max_stride : portraits_left;
      printf("left: %u, max %u, processing %u portraits\n", portraits_left, max_stride, num_row_portraits);

      f32 aspect = 1.0f;
      f32 height = PLAYER_PORTRAIT_WIDTH * aspect;
      f32 y = PORTRAIT_Y_OFFSET + row_idx * height;

      for (u32 i = 0; i < num_row_portraits; i++) {
        f32 x = PLAYER_PORTRAIT_WIDTH * (-(f32)(num_row_portraits) / 2.0f + 0.5f + i);
        u32 ui_idx = portraits_processed + i;
        s->ui_elements[ui_idx] = {
            .center = vec2(x, y),
            .size = vec2(PLAYER_PORTRAIT_WIDTH, height),
            .tex_id = ui_idx & 1,
        };
        printf("populated ui %u with center (%f, %f at row %u)\n", ui_idx, x, y, row_idx);
      }
      portraits_processed += num_row_portraits;
      row_idx++;
    }
    u32 instance_count = NUM_CHARACTERS;
    write_streaming_buffer(&r->ui_buffer, s->ui_elements, 0, sizeof(UiElement) * instance_count);
    render_mesh_instanced(cmd, &mesh, &r->ui_mat, instance_count, &r->ui_buffer);
    break;
  }
  case MENU_TYPE_OPTIONS: {
    break;
  }
  default: {
    return;
  }
  }
}

void render(State *s) {
  Renderer *r = &s->renderer;
  VulkanContext *ctx = &r->ctx;
  begin_frame(ctx);
  VkCommandBuffer cmd = begin_command_buffer(ctx);
  begin_render_pass(
      ctx, cmd, ctx->render_pass, ctx->framebuffers[ctx->image_index], r->clear_values, 2, r->viewport_state
  );

  switch (s->game_mode) {
  case GAMEMODE_PLAYING:
  case GAMEMODE_PAUSED: {
    PlayingState *ps = &s->playing_state;

    i32 width = r->ctx.swapchain_extent.width;
    i32 height = r->ctx.swapchain_extent.height;
    f32 aspect = (f32)width / (f32)height;
    CameraMatrices camera_matrices;
    if (ps->screen_shake.active) {
      f32 dx = evaluate_damped_harmonic_oscillator(ps->screen_shake.x_oscillator, ps->screen_shake.time_elapsed);
      f32 dy = evaluate_damped_harmonic_oscillator(ps->screen_shake.y_oscillator, ps->screen_shake.time_elapsed);
      camera_matrices = camera_matrices_offset(&ps->camera, vec3(dx, dy, 0.0f), aspect);
    } else {
      camera_matrices = create_camera_matrices(&ps->camera, aspect);
    }

    ModelVP model_vp;
    mult_m4(&camera_matrices.projection, &camera_matrices.view, &model_vp.vp);

    // Arena
    model_vp.model = mat4();
    scale_m4(arena_dimensions0, &model_vp.model);
    push_constants_material(cmd, &r->background_mat, &model_vp);
    render_mesh(cmd, &r->mesh, &r->background_mat);

    // Left paddle
    model_vp.model = make_ts_mat(ps->lpaddle_pos, ps->lpaddle_size);
    push_constants_material(cmd, &r->paddle_mat, &model_vp);
    render_mesh(cmd, &r->mesh, &r->paddle_mat);

    // Right paddle
    model_vp.model = make_ts_mat(ps->rpaddle_pos, ps->rpaddle_size);
    push_constants_material(cmd, &r->paddle_mat, &model_vp);
    render_mesh(cmd, &r->mesh, &r->paddle_mat);

    // Ball
    model_vp.model = make_ts_mat(ps->ball_pos, ps->ball_size);
    push_constants_material(cmd, &r->paddle_mat, &model_vp);
    render_mesh(cmd, &r->mesh, &r->paddle_mat);

    for (u32 i = 0; i < MAX_POWERUPS; i++) {
      if (ps->powerups[i].time_remaining > 0.0f) {
        model_vp.model = make_ts_mat(ps->powerups[i].pos, powerup_size);
        push_constants_material(cmd, &r->paddle_mat, &model_vp);
        render_mesh(cmd, &r->mesh, &r->paddle_mat);
      }
    }

    // UI
    f32 portrait_aspect = 1.0;
    f32 portrait_w = 0.4f;
    f32 portrait_h = portrait_w * portrait_aspect;
    f32 portrait_y = -1.0f + portrait_h / 2.0f;
    u32 instance_count = 2;
    VulkanMesh mesh = {.vertex_count = 6};

    s->ui_elements[0] = {
        .tex_id = 0,
        .center = vec2(-1.0 + portrait_w / 2.0, portrait_y),
        .size = vec2(portrait_w, portrait_h),
    };

    s->ui_elements[1] = {
        .tex_id = 1,
        .center = vec2(1.0 - portrait_w / 2.0, portrait_y),
        .size = vec2(portrait_w, portrait_h),
    };

    write_streaming_buffer(&r->ui_buffer, s->ui_elements, 0, sizeof(UiElement) * instance_count);
    render_mesh_instanced(cmd, &mesh, &r->ui_mat, instance_count, &r->ui_buffer);

    break;
  }
  case GAMEMODE_MAIN_MENU: {
    render_menu(s, cmd);
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
  PlayingState *ps = &st->playing_state;

  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    printf("pausing\n");
    st->game_mode = GAMEMODE_PAUSED;
  }

  bool horizontal_enabled = (ps->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED);
  Vec2 input_direction = inputs_to_direction(inputs);
  input_direction.x *= horizontal_enabled;

  if (key_pressed(inputs, INPUT_KEY_SPACEBAR) && ps->pong_mode == PONG_MODE_BETWEEN_POINTS) {
    // TODO engine: convert to a branchless scheme
    f32 theta = 2 * PI * random_f32_xoroshiro128_plus(&ps->rngs.ball_direction);
    const f32 half_pi = PI / 2.0f;
    const f32 epsilon = PI / 16.0f;
    const f32 top_min = half_pi - epsilon;
    const f32 top_max = half_pi + epsilon;
    const f32 bottom_min = 3 * half_pi - epsilon;
    const f32 bottom_max = 3 * half_pi + epsilon;

    while (interval_contains(theta, top_min, top_max) || interval_contains(theta, bottom_min, bottom_max)) {
      theta = random_f32_xoroshiro128_plus(&ps->rngs.ball_direction);
    }

    const f32 x = cosf(theta);
    const f32 y = sinf(theta);
    ps->ball_vel.x = ps->ball_speed * x;
    ps->ball_vel.y = ps->ball_speed * y;

    ps->pong_mode = PONG_MODE_LIVE_BALL;
  }

  // TODO replace with powerup
  if (key_pressed(inputs, INPUT_KEY_H)) {
    ps->movement_mode = (ps->movement_mode == MOVEMENT_MODE_HORIZONTAL_ENABLED) ? MOVEMENT_MODE_VERTICAL_ONLY
                                                                                : MOVEMENT_MODE_HORIZONTAL_ENABLED;
  }

  // TODO make the paddle scale over the course of the game
  if (len_v2(input_direction) > EPSILON) {
    Vec3 movement = Vec3(input_direction.x, input_direction.y, 0.0f);
    inc_v3(&ps->lpaddle_pos, scale_v3(movement, dt * ps->lpaddle_speed));
  }
}

void press_menu_ui(State *st) {
  switch (st->menu_state.selected_option) {
  case MAIN_MENU_UI_STORY:
    st->game_mode = GAMEMODE_PLAYING;
    break;
  case MAIN_MENU_UI_1V1:
    st->menu_state.menu_type = MENU_TYPE_1V1;
    break;
  case MAIN_MENU_UI_OPTIONS:
    break;

  case MAIN_MENU_UI_LOGO:
  default:
    return;
  }
}

void process_inputs_main_menu(State *st) {
  const Inputs *inputs = &st->inputs;
  MenuState *ms = &st->menu_state;

  switch (ms->menu_type) {
  case MENU_TYPE_MAIN: {
    if (key_pressed(inputs, INPUT_KEY_SPACEBAR)) {
      ms->intro_active = false;
    }

    if (ms->intro_active) {
      return;
    }

    f32 x0 = (f32)inputs->curr_cursor_x / (f32)st->window_width * 2.0f - 1.0f;
    f32 y0 = (f32)inputs->curr_cursor_y / (f32)st->window_height * 2.0f - 1.0f;
    if (mouse_moved(inputs)) {
      for (u32 i = MAIN_MENU_UI_STORY; i < NUM_MENU_UIS; i++) {
        bool hit = intersect_ui(&st->ui_elements[i], x0, y0);
        if (hit) {
          st->menu_state.selected_option = (MainMenuUI)i;

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
      case MAIN_MENU_UI_STORY:
        ms->selected_option = MAIN_MENU_UI_1V1;
        break;
      case MAIN_MENU_UI_1V1:
        ms->selected_option = MAIN_MENU_UI_OPTIONS;
        break;
      default:
        break;
      }
    } else if (key_pressed(inputs, INPUT_KEY_UP_ARROW)) {
      printf("up\n");
      switch (ms->selected_option) {
      case MAIN_MENU_UI_1V1:
        ms->selected_option = MAIN_MENU_UI_STORY;
        break;
      case MAIN_MENU_UI_OPTIONS:
        ms->selected_option = MAIN_MENU_UI_1V1;
        break;
      default:
        break;
      }
    } else if (key_pressed(inputs, INPUT_KEY_ENTER)) {
      press_menu_ui(st);
    }
    break;
  }

  case MENU_TYPE_1V1: {
    if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
      ms->menu_type = MENU_TYPE_MAIN;
    }
    break;
  }
  default:
    return;
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

void handle_collisions(PlayingState *ps, const f32 dt) {
  f32 arena_horizontal_boundary = arena_dimensions_x0 / 2.0f;
  f32 arena_vertical_boundary = arena_dimensions_y0 / 2.0f;

  // collisions with boundaries
  if (ps->ball_pos.x + 0.5f * ps->ball_size.x > arena_horizontal_boundary) {
    ps->left_score++;
    ps->ball_pos = ball_pos0;
    ps->pong_mode = PONG_MODE_BETWEEN_POINTS;
    ps->ball_vel = Vec3(0.0f);
  }
  if (ps->ball_pos.x - 0.5f * ps->ball_size.x < -arena_horizontal_boundary) {
    ps->right_score++;
    ps->ball_pos = ball_pos0;
    ps->pong_mode = PONG_MODE_BETWEEN_POINTS;
    ps->ball_vel = Vec3(0.0f);
  }

  if (ps->ball_pos.y + 0.5f * ps->ball_size.y > arena_vertical_boundary) {
    ps->ball_vel.y = -fabs(ps->ball_vel.y);
    ps->ball_pos.y = arena_vertical_boundary - 0.5f * ps->ball_size.y;
  }
  if (ps->ball_pos.y - 0.5f * ps->ball_size.y < -arena_vertical_boundary) {
    ps->ball_vel.y = fabs(ps->ball_vel.y);
    ps->ball_pos.y = -arena_vertical_boundary + 0.5f * ps->ball_size.y;
  }

  // paddle intersects with arena boundary
  // left paddle
  // vertical
  if (ps->lpaddle_pos.y + 0.5f * ps->lpaddle_size.y > arena_vertical_boundary) {
    ps->lpaddle_pos.y = arena_vertical_boundary - 0.5f * ps->lpaddle_size.y;
  }

  if (ps->lpaddle_pos.y - 0.5f * ps->lpaddle_size.y < -arena_vertical_boundary) {
    ps->lpaddle_pos.y = -arena_vertical_boundary + 0.5f * ps->lpaddle_size.y;
  }

  // horizontal
  if (ps->lpaddle_pos.x + 0.5f * ps->lpaddle_size.x > arena_horizontal_boundary) {
    ps->lpaddle_pos.x = arena_horizontal_boundary - 0.5f * ps->lpaddle_size.x;
  }

  if (ps->lpaddle_pos.x - 0.5f * ps->lpaddle_size.x < -arena_horizontal_boundary) {
    ps->lpaddle_pos.x = -arena_horizontal_boundary + 0.5f * ps->lpaddle_size.x;
  }

  // right paddle
  if (ps->rpaddle_pos.y - 0.5f * ps->rpaddle_size.y < -arena_vertical_boundary) {
    ps->rpaddle_pos.y = -arena_vertical_boundary + 0.5f * ps->rpaddle_size.y;
  }

  if (ps->rpaddle_pos.y + 0.5f * ps->rpaddle_size.y > arena_vertical_boundary) {
    ps->rpaddle_pos.y = arena_vertical_boundary - 0.5f * ps->rpaddle_size.y;
  }

  if (ps->left_paddle_cooldown <= 0.0f) {
    // ball paddle collisions
    SweptAABBCollisionCheck left_collision_check = swept_aabb_collision(
        dt, ps->lpaddle_pos, ps->lpaddle_size, ps->lpaddle_vel, ps->ball_pos, ps->ball_size, ps->ball_vel
    );

    if (left_collision_check.did_collide) {
      ps->left_paddle_cooldown = 1.0f;
      Vec3 normal = left_collision_check.normal;
      log_v3(normal);
      ps->last_paddle = PADDLE_LEFT;
      // state->screen_shake.active = true;

      // normal is from paddles's perspective
      if (left_collision_check.was_overlapping) {
        const Vec3 displacement = scale_v3(normal, left_collision_check.penetration_depth);
        inc_v3(&ps->ball_pos, displacement);
      } else {
        inc_v3(&ps->ball_pos, scale_v3(ps->ball_vel, left_collision_check.t));
      }

      f32 asdf = 2.0f * dot_v3(ps->ball_vel, normal);
      ps->ball_vel = sub_v3(ps->ball_vel, scale_v3(normal, asdf));
    }
  } else {
    ps->left_paddle_cooldown -= dt;
  }

  if (aabb_collision(ps->ball_pos, ps->ball_size, ps->rpaddle_pos, ps->rpaddle_size)) {
    ps->last_paddle = PADDLE_RIGHT;
    ps->ball_vel.x = -ps->ball_vel.x;
    // state->screen_shake.active = true;
  }

  for (u32 i = 0; i < MAX_POWERUPS; i++) {
    bool active = ps->powerups[i].time_remaining > 0.0f;
    if (active && aabb_collision(ps->ball_pos, ps->ball_size, ps->powerups[i].pos, powerup_size)) {
      printf("Last paddle %u took powerup in index %u\n", ps->last_paddle, i);
      ps->powerups[i].time_remaining = 0.0f;
      ps->paddle_powerups[ps->last_paddle][ps->powerups[i].type].time_left += powerup_time0;
    }
  }
}

void update_game_state(State *s, const f32 dt) {

  switch (s->game_mode) {
  case GAMEMODE_PAUSED:
    return;
  case GAMEMODE_MAIN_MENU: {
    Vec2 logo_size = vec2(1.0f, 0.3f);
    Vec2 option_size = vec2(MENU_UI_WIDTH, 0.2f);
    s->menu_state.selector_pos = vec2(-1.1 * MENU_UI_WIDTH, menu_ui_pos[s->menu_state.selected_option].y);
    s->ui_elements[MAIN_MENU_UI_LOGO] = {.center = vec2(0.0f, -0.5f), .rotation = 0.0f, .size = logo_size, .tex_id = 0};
    s->ui_elements[MAIN_MENU_UI_STORY] = {
        .center = menu_ui_pos[MAIN_MENU_UI_STORY], .rotation = 0.0f, .size = option_size, .tex_id = 1
    };
    s->ui_elements[MAIN_MENU_UI_1V1] = {
        .center = menu_ui_pos[MAIN_MENU_UI_1V1], .rotation = 0.0f, .size = option_size, .tex_id = 0
    };
    s->ui_elements[MAIN_MENU_UI_OPTIONS] = {
        .center = menu_ui_pos[MAIN_MENU_UI_OPTIONS], .rotation = 0.0f, .size = option_size, .tex_id = 1
    };
    return;
  }
  case GAMEMODE_PLAYING: {
    PlayingState *ps = &s->playing_state;

    // Powerup
    u32 unused_idx = MAX_POWERUPS;
    for (u32 i = 0; i < MAX_POWERUPS; i++) {
      ps->powerups[i].time_remaining -= dt;
      if (ps->powerups[i].time_remaining < 0.0) {
        unused_idx = i;
      }
    }

    ps->time_since_last_powerup_draw += dt * (ps->pong_mode == PONG_MODE_LIVE_BALL);
    if (ps->time_since_last_powerup_draw > powerup_draw_interval_in_sec) {
      ps->time_since_last_powerup_draw -= powerup_draw_interval_in_sec;

      f32 prob_to_spawn = random_f32_xoroshiro128_plus(&ps->rngs.powerup_spawn);
      if (unused_idx != MAX_POWERUPS && prob_to_spawn < prob_powerup_spawns) {
        u32 powerup_idx = draw_alias_table(&ps->powerup_alias_table);
        f32 x = ps->arena_dimensions.x * (-0.5f + random_f32_xoroshiro128_plus(&ps->rngs.powerup_spawn));
        f32 y = ps->arena_dimensions.y * (-0.5f + random_f32_xoroshiro128_plus(&ps->rngs.powerup_spawn));
        ps->powerups[unused_idx].pos = vec3(x, y, z0);
        ps->powerups[unused_idx].type = (PowerUpType)powerup_idx;
        ps->powerups[unused_idx].time_remaining = powerup_time0;
        printf("Spawning powerup %u at (%f , %f) in index %u\n", powerup_idx, x, y, unused_idx);
      }
    }

    // AI
    if (ps->ball_pos.y > ps->rpaddle_pos.y) {
      ps->rpaddle_vel.y = cpu_speed0;
    } else {
      ps->rpaddle_vel.y = -cpu_speed0;
    }

    inc_v3(&ps->lpaddle_pos, scale_v3(ps->lpaddle_vel, dt));
    inc_v3(&ps->rpaddle_pos, scale_v3(ps->rpaddle_vel, dt));
    inc_v3(&ps->ball_pos, scale_v3(ps->ball_vel, dt));

    handle_collisions(ps, dt);

    if (ps->screen_shake.active) {
      ps->screen_shake.time_elapsed += dt;
      if (ps->screen_shake.time_elapsed > ps->screen_shake.cutoff_duration) {
        ps->screen_shake.time_elapsed = 0.0f;
        ps->screen_shake.active = false;
      }
    }

    break;
  }
  }
}
