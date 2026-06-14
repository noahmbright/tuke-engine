#pragma once

// TODO
// Particles
// Dashes
// Parries
// Shield
// Cooldown timer on overlay - ready icon?
//  Equipped weapon in overlay
// Weapon wheel
// EVENTUALLY need some serialization scheme for things like weapon unlocks

#include "billboard_manager.h"
#include "generated_shader_utils.h"
#include "glm/common.hpp"
#include "opengl_base.h"
#include "physics.h"
#include "topdown.h"
#include "tuke_engine.h"
#include "window.h"
#include <OpenGL/OpenGL.h>

#define MAX_NUM_BULLETS (512)
#define MAX_NUM_ENEMIES (8)

enum Uniforms {
  UNIFORM_PLAYER_MODEL,
  UNIFORM_PLAYER_FRAG,
  UNIFORM_OVERLAY,

  NUM_UNIFORMS,
};

const f32 BULLET_HELL_ARENA_WIDTH = 10.0f;
const f32 BULLET_HELL_ARENA_HEIGHT = 8.0f;
const f32 BULLET_HELL_ARENA_HALF_WIDTH = BULLET_HELL_ARENA_WIDTH / 2.0;
const f32 BULLET_HELL_ARENA_HALF_HEIGHT = BULLET_HELL_ARENA_HEIGHT / 2.0;

// clang-format off
const f32 arena_vertices[] = {
  // x,y,z                                                               u, v
  -BULLET_HELL_ARENA_HALF_WIDTH, -BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  0.0f, 0.0f, // BL
  -BULLET_HELL_ARENA_HALF_WIDTH,  BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  0.0f, 1.0f, // TL
   BULLET_HELL_ARENA_HALF_WIDTH,  BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  1.0f, 1.0f, // TR

   BULLET_HELL_ARENA_HALF_WIDTH,  BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  1.0f, 1.0f, // TR
   BULLET_HELL_ARENA_HALF_WIDTH, -BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  1.0f, 0.0f, // BR
  -BULLET_HELL_ARENA_HALF_WIDTH, -BULLET_HELL_ARENA_HALF_HEIGHT,  0.0f,  0.0f, 0.0f, // BL
};
// clang-format on

enum BulletPattern {
  BULLET_PATTERN_LINEAR,
  BULLET_PATTERN_SPIRAL,
};

// What shapes of bullets will I support?
// For now, start with quads
// Will I end up having specializations for each bullet based on update rules?
// And then have arrays per bullet with update rule in the bullet manager?
struct Bullet {
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 size;
  f64 t0; // Spawn time
  BulletPattern pattern;
};

struct BulletRenderData {
  glm::vec2 pos;
  f32 size;
};

struct BulletManager {
  Bullet bullets[MAX_NUM_BULLETS];
  BulletRenderData render_data[MAX_NUM_BULLETS];
  u32 num_live_bullets;
};

// How will I choose the right pool and right index in that pool to allocate the new bullet?
// How will I efficiently copy the data into the array from the caller?
//      Can return a pointer to the next bullet and populate in the caller.
// How can I ensure that I never overrun this buffer? Can I avoid adding a branch every call?
// How do I have an API for every type of bullet? Do I always need to copy from the caller?
inline void spawn_bullet(BulletManager *bullet_manager, Bullet bullet) {
  bullet_manager->bullets[bullet_manager->num_live_bullets++] = bullet;
}

// An okay set of guides on FSMs for game AI, lots of OOP dogma, bad code, but introduces transition tables
// http://www.ai-junkie.com/architecture/state_driven/tut_state1.html
//
// Game programming patterns on state. Also OOP, but on_entry, on_exit notions useful.
// Heirarchical state machines, pushdown automata, state stacks, behavior trees, and planning systems also exist.
// https://gameprogrammingpatterns.com/state.html

struct Enemy {
  glm::vec2 position;
};

struct EnemyRenderData {
  glm::vec2 pos;
  f32 size;
};

struct EnemyManager {
  Enemy enemies[MAX_NUM_ENEMIES];
  EnemyRenderData render_data[MAX_NUM_ENEMIES];
  u32 num_live_enemies;
};

enum PlayerState {
  PLAYER_STATE_NORMAL,
  PLAYER_STATE_SPEED_BOOST,
  PLAYER_STATE_SLOW_MOTION,
  PLAYER_STATE_AIMING_DASH,
  PLAYER_STATE_DASHING,
};

enum PlayerAbility {
  PLAYER_ABILITY_NONE,
  PLAYER_ABILITY_SPEED_BOOST,
  PLAYER_ABILITY_DASH,
  PLAYER_ABILITY_SLOW_MOTION,
};

struct PlayerDash {
  f32 cooldown_left;
  f32 time_dilation_factor; // for aiming in slow motion
};

struct PlayerSpeedBoost {
  f32 boost_factor;
};

struct PlayerSlowMotion {
  f32 time_dilation_factor;
};

// player_pos is the position within the bullet hell arena
// The arena's center is (0.0, 0.0)
struct Player {
  glm::vec3 pos;
  glm::vec3 size;
  u32 current_health;
  u32 max_health;
  f32 invincibility_time;

  PlayerState state;
  PlayerAbility ability;
  u32 unlocked_abilities_mask;

  PlayerDash dash;
  PlayerSpeedBoost speed_boost;
  PlayerSlowMotion slow_motion;
};

struct BulletHellSceneData {
  Player player;

  BulletManager *bullet_manager;
  EnemyManager enemy_manager;
  f64 bullet_spawn_time;

  BillboardManager billboard_manager;

  Camera camera;
  u32 vp_ubo;

  GLMesh player_mesh;
  GLMaterial player_material;

  GLMesh arena_mesh;
  GLMaterial arena_material;

  GLMesh bullet_mesh;
  GLMaterial bullet_material;

  GLMesh enemy_mesh;
  GLMaterial enemy_material;

  u32 overlay_program;

  u32 uniforms[NUM_UNIFORMS];
};

inline void player_cancel_ability(Player *player) { player->state = PLAYER_STATE_NORMAL; }

inline void player_activate_ability(Player *player) {

  switch (player->state) {
    // If normal, no reason not to activate the current ability.
  case PLAYER_STATE_NORMAL:
    switch (player->ability) {
    case PLAYER_ABILITY_NONE:
      return;
    case PLAYER_ABILITY_SPEED_BOOST:
      player->state = PLAYER_STATE_SPEED_BOOST;
      return;
    case PLAYER_ABILITY_DASH:
      player->state = PLAYER_STATE_AIMING_DASH;
      return;
    case PLAYER_ABILITY_SLOW_MOTION:
      player->state = PLAYER_STATE_SLOW_MOTION;
      return;
    }
    break;

    // No action required.
  case PLAYER_STATE_AIMING_DASH:
  case PLAYER_STATE_DASHING:
  case PLAYER_STATE_SPEED_BOOST:
  case PLAYER_STATE_SLOW_MOTION:
    return;

  default:
    assert(false);
  }
}

inline void player_release_ability(Player *player) {
  switch (player->state) {

  case PLAYER_STATE_AIMING_DASH:
    // TODO real dashing logic
    player->state = PLAYER_STATE_DASHING;
    return;

    // No action required.
  case PLAYER_STATE_NORMAL:
  case PLAYER_STATE_DASHING:
  case PLAYER_STATE_SPEED_BOOST:
  case PLAYER_STATE_SLOW_MOTION:
    player->state = PLAYER_STATE_NORMAL;
    return;

  default:
    assert(false);
  }
}

inline void bullet_hell_move_player_normal(Player *player, glm::vec3 input_movement_vector, f32 dt) {

  const f32 X_BOUNDARY = 0.5 * (BULLET_HELL_ARENA_WIDTH - PLAYER_SIDE_LENGTH_METERS);
  const f32 Y_BOUNDARY = 0.5 * (BULLET_HELL_ARENA_HEIGHT - PLAYER_SIDE_LENGTH_METERS);

  const f32 speed = 5.0f;
  glm::vec3 movement_vector = speed * dt * input_movement_vector;

  f32 next_x = player->pos.x + movement_vector.x;
  f32 clamped_next_x = clamp_f32(next_x, -X_BOUNDARY, X_BOUNDARY);

  f32 next_y = player->pos.y + movement_vector.y;
  f32 clamped_next_y = clamp_f32(next_y, -Y_BOUNDARY, Y_BOUNDARY);

  player->pos.x = clamped_next_x;
  player->pos.y = clamped_next_y;
}

enum PlayerIntentFlags {
  PLAYER_INTENT_NONE = 0,
  PLAYER_INTENT_ACTIVATE_ABILITY = 1 << 0,
  PLAYER_INTENT_CANCEL_ABILITY = 1 << 1,
  PLAYER_INTENT_RELEASE_ABILITY = 1 << 1,
};

struct PlayerIntent {
  glm::vec3 movement_vector;
  u32 flags;
};

inline PlayerIntent handle_inputs_player(const Inputs *inputs) {
  // Inputs define intent
  // Shift held to activate ability
  // q to cancel ability
  glm::vec3 input_movement_vector = inputs_to_movement_vector(inputs);
  bool shift_held = key_held(inputs, INPUT_KEY_LEFT_SHIFT) || key_held(inputs, INPUT_KEY_RIGHT_SHIFT);
  bool shift_released = key_released(inputs, INPUT_KEY_LEFT_SHIFT) || key_released(inputs, INPUT_KEY_RIGHT_SHIFT);
  bool q_held = key_held(inputs, INPUT_KEY_Q);

  // Update state given the intent for this frame. Transition to normal state if q held,
  // or attempt to activate power if shift_held.
  u32 flags = 0;
  flags |= shift_held * PLAYER_INTENT_ACTIVATE_ABILITY;
  flags |= shift_released * PLAYER_INTENT_RELEASE_ABILITY;
  flags |= q_held * PLAYER_INTENT_CANCEL_ABILITY;

  PlayerIntent player_intents{
      .movement_vector = input_movement_vector,
      .flags = flags,
  };

  return player_intents;
}

inline void bullet_hell_update_player(Player *player, PlayerIntent player_intent, f32 dt) {

  // Intent for cancel assumed to be stronger than intent for activation
  u32 flags = player_intent.flags;
  if (flags & PLAYER_INTENT_CANCEL_ABILITY) {
    player_cancel_ability(player);
  } else if (flags & PLAYER_INTENT_RELEASE_ABILITY) {
    player_release_ability(player);
  } else if (flags & PLAYER_INTENT_ACTIVATE_ABILITY) {
    player_activate_ability(player);
  }

  // Move the player according to the current state
  switch (player->state) {
  case PLAYER_STATE_NORMAL:
  case PLAYER_STATE_SLOW_MOTION:
    bullet_hell_move_player_normal(player, player_intent.movement_vector, dt);
    return;
  case PLAYER_STATE_SPEED_BOOST: {
    glm::vec3 scaled_movement_vector = player->speed_boost.boost_factor * player_intent.movement_vector;
    bullet_hell_move_player_normal(player, scaled_movement_vector, dt);
    return;
  }

  case PLAYER_STATE_AIMING_DASH:
    return;
  case PLAYER_STATE_DASHING:
    return;
  default:
    assert(false);
  }
}

// Supposing a rectangle of half side lengths is positioned at the origin
inline f32 rectangle_sdf(f32 half_width, f32 half_height, glm::vec2 pos) {

  glm::vec2 abs_pos = glm::abs(pos);
  glm::vec2 rect_vec = glm::vec2(half_width, half_height);
  glm::vec2 diff = abs_pos - rect_vec;

  glm::vec2 clamped_diff = glm::max(glm::vec2(0.0f), diff);
  f32 dist_outside = glm::length(clamped_diff);
  f32 dist_inside = fmin(fmax(diff.x, diff.y), 0.0f);

  return dist_outside + dist_inside;
}

// This function will never spawn new bullets. It only moves and kills bullets that were alive
// at the beginning of this frame.
// Can consider double buffering the bullets.
inline void update_bullets(BulletManager *bullet_manager, f32 dt) {

  u32 live_bullet_index = 0;
  u32 end = bullet_manager->num_live_bullets;

  for (u32 i = 0; i < end;) {

    // Current bullet. Update, and check if it survives this frame.
    Bullet bullet = bullet_manager->bullets[i];
    bullet.position += dt * bullet.velocity;

    // Signed distance to the arena. If negative, the bullet is still alive
    // TODO this would break down if I decide to add bullets that go outside the arena and come back in
    f32 signed_distance = rectangle_sdf(BULLET_HELL_ARENA_HALF_WIDTH, BULLET_HELL_ARENA_HALF_HEIGHT, bullet.position);

    // Add to render data if in box, i.e. signed_distance < 0
    if (signed_distance < 0.0f) {
      bullet_manager->render_data[live_bullet_index].pos = bullet.position;
      bullet_manager->render_data[live_bullet_index].size = 0.3; // FIXME
      bullet_manager->bullets[i] = bullet;
      live_bullet_index++;
      i++;
    } else {
      // Kill. Don't copy back into bullet data. Move the bullet at the end of the live range into this spot.
      // Skip updating i so we update the bullet copied into i in the next iteration.
      bullet_manager->bullets[i] = bullet_manager->bullets[--end];
    }
  }

  bullet_manager->num_live_bullets = live_bullet_index;
}

// TODO do I want all OpenGL to be in the draw function, do I want explicit updates on the GPU right after CPU updates?
//  Is the OpenGL API the GPU API or the drawing API?
//  Leaning more toward all in the drawing.
inline void bullet_hell_update(void *scene_data, void *global_state, f32 dt) {
  GlobalState *gs = (GlobalState *)global_state;
  const Inputs *inputs = &gs->inputs;

  // FIXME need to make a proper state machine for the world
  if (key_pressed(inputs, INPUT_KEY_ESCAPE)) {
    gs->game_state = (gs->game_state == GAME_STATE_PAUSED) ? GAME_STATE_RUNNING : GAME_STATE_PAUSED;
  }

  if (gs->game_state == GAME_STATE_PAUSED) {
    return;
  }

  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;
  BulletManager *bullet_manager = data->bullet_manager;
  EnemyManager *enemy_manager = &data->enemy_manager;
  Player *player = &data->player;

  PlayerIntent player_intent = handle_inputs_player(&gs->inputs);

  // Update dt
  // Potential FIXME: this is not really scalable, maybe
  if (player->state == PLAYER_STATE_AIMING_DASH) {
    dt = dt * player->dash.time_dilation_factor;
  } else if (player->state == PLAYER_STATE_SLOW_MOTION) {
    dt = dt * player->slow_motion.time_dilation_factor;
  } else if (player->state == PLAYER_STATE_AIMING_DASH) {
    dt = dt * player->dash.time_dilation_factor;
  }

  bullet_hell_update_player(&data->player, player_intent, dt);

  // TODO: Need window resize callback. Want to only update and rebuffer when there's new data.
  CameraMatrices camera_matrices = create_camera_matrices(&data->camera, gs->window_width, gs->window_height);
  buffer_vp_matrix_to_gl_ubo(&camera_matrices, data->vp_ubo);

  // Update billboards with this frame's view matrix
  clear_billboard_manager(&data->billboard_manager);

  Billboard billboard{
      .center_pos = glm::vec3(sinf(gs->t), cosf(gs->t), EPSILON),
      .size = glm::vec2(PLAYER_SIDE_LENGTH_METERS),
      .rotation = 0.0f,
  };
  push_billboard(&data->billboard_manager, billboard);

  // Decrement and clamp invincibility_time
  player->invincibility_time -= dt;
  player->invincibility_time = (player->invincibility_time < 0.0) ? 0.0 : player->invincibility_time;
  BulletHellPlayerFrag player_frag_data{.invincibility_time = player->invincibility_time};
  glBindBuffer(GL_UNIFORM_BUFFER, data->uniforms[UNIFORM_PLAYER_FRAG]);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BulletHellPlayerFrag), &player_frag_data);

  // If I add slow motion or anything like that, will need to consider how to track time updates
  // Influences on the rate of time passing are player inputs - could add something like enemy effects
  gs->t += dt;

  glm::mat4 player_model = glm::translate(glm::mat4(1.0f), data->player.pos);
  player_model = glm::scale(player_model, data->player.size);
  glBindBuffer(GL_UNIFORM_BUFFER, data->uniforms[UNIFORM_PLAYER_MODEL]);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  // enemy update bringup
  f32 x_amplitude = BULLET_HELL_ARENA_HALF_WIDTH * 0.8f;
  f32 theta = 2.0 * gs->t;
  enemy_manager->enemies[0].position.x = x_amplitude * sinf(theta);
  enemy_manager->render_data[0].pos.x = x_amplitude * sinf(theta);
  glBindBuffer(GL_ARRAY_BUFFER, data->enemy_mesh.vbos[0]);
  glBufferSubData(
      GL_ARRAY_BUFFER, 0, sizeof(EnemyRenderData) * enemy_manager->num_live_enemies, &enemy_manager->render_data
  );

  // Spawn bullets from enemy[0] - need some general bullet spawning data wrapper
  f32 spawn_interval = 0.1;
  data->bullet_spawn_time += dt;
  if (data->bullet_spawn_time > spawn_interval) {
    Enemy *enemy0 = &enemy_manager->enemies[0];
    Bullet new_bullet{
        .position = enemy0->position,
        .velocity = glm::vec3(0.0f, -8.0f, 0.0f),
        .size = glm::vec3(0.3f),
        .t0 = gs->t,
        .pattern = BULLET_PATTERN_LINEAR,
    };
    spawn_bullet(bullet_manager, new_bullet);
    data->bullet_spawn_time -= spawn_interval;
  }

  // Move bullets
  update_bullets(bullet_manager, dt);
  glBindBuffer(GL_ARRAY_BUFFER, data->bullet_mesh.vbos[0]);
  glBufferSubData(
      GL_ARRAY_BUFFER, 0, sizeof(BulletRenderData) * bullet_manager->num_live_bullets, &bullet_manager->render_data
  );

  // Collision detection
  if (player->invincibility_time <= 0.0) {
    glm::vec2 player_xy(player->pos.x, player->pos.y);
    glm::vec2 player_size_xy(player->size.x, player->size.y);

    for (u32 i = 0; i < data->bullet_manager->num_live_bullets; i++) {
      Bullet *bullet = &bullet_manager->bullets[i];

      if (aabb_collision_vec2(player_xy, player_size_xy, bullet->position, bullet->size)) {
        player->current_health -= (player->current_health > 0);
        player->invincibility_time = 1.0;
      }
    }
  }
}

inline void bullet_hell_draw(const GLRenderer *renderer, const void *scene_data) {
  (void)renderer;
  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;

  // Draw directly to screen, no post processing yet
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // The arena, player, enemies, bullets
  draw_gl_mesh(&data->arena_mesh, data->arena_material);
  draw_gl_mesh_instanced(&data->bullet_mesh, data->bullet_material, data->bullet_manager->num_live_bullets);
  draw_gl_mesh_instanced(&data->enemy_mesh, data->enemy_material, data->enemy_manager.num_live_enemies);
  draw_gl_mesh(&data->player_mesh, data->player_material);

  // Overlay: health (Weapon wheel?)
  BulletHellData bullet_hell_render_data{
      .max_health = data->player.max_health,
      .health = data->player.current_health,
  };

  glUseProgram(data->overlay_program);
  glBindBuffer(GL_UNIFORM_BUFFER, data->uniforms[UNIFORM_OVERLAY]);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BulletHellData), &bullet_hell_render_data);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glm::mat4 view = camera_look_at(&data->camera);
  render_billboards_opengl(&data->billboard_manager, view);
}

////////////////////////////////// BIG INIT FUNCTION //////////////////////////////////
inline BulletHellSceneData create_bullet_hell_scene(u32 vp_ubo) {

  BulletManager *bullet_manager = (BulletManager *)malloc(sizeof(BulletManager));
  memset(bullet_manager, 0, sizeof(BulletManager));

  Camera bullet_hell_camera = create_camera(CAMERA_TYPE_2D);
  bullet_hell_camera.position.z = 15.0f;

  // Player
  u32 player_program = shader_handles_to_gl_program(
      SHADER_HANDLE_TOPDOWN_BULLET_HELL_PLAYER_VERT, SHADER_HANDLE_TOPDOWN_BULLET_HELL_PLAYER_FRAG
  );

  u32 player_model_ubo = create_gl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  u32 player_frag_ubo = create_gl_ubo(sizeof(BulletHellPlayerFrag), GL_DYNAMIC_DRAW);

  GLMaterial player_material = create_gl_material(player_program);
  gl_material_add_uniform(
      &player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_BULLET_HELL_PLAYER_MODEL, "PlayerModel"
  );
  gl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  GLMesh bullet_player_mesh = create_gl_mesh_with_vertex_layout(
      player_vertices, sizeof(player_vertices), 6, VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW
  );
  GLMaterial bullet_player_material = create_gl_material(player_program);
  gl_material_add_uniform(
      &player_material, player_frag_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_BULLET_HELL_PLAYER_FRAG, "BulletHellPlayerFrag"
  );
  gl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_PLAYER_MODEL, "PlayerModel");
  gl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Arena
  u32 arena_program = shader_handles_to_gl_program(SHADER_HANDLE_TOPDOWN_ARENA_VERT, SHADER_HANDLE_TOPDOWN_ARENA_FRAG);

  GLMesh arena_mesh = create_gl_mesh_with_vertex_layout(
      arena_vertices, sizeof(arena_vertices), 6, VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW
  );
  GLMaterial arena_material = create_gl_material(arena_program);
  gl_material_add_uniform(&arena_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Bullets
  u32 bullet_program =
      shader_handles_to_gl_program(SHADER_HANDLE_TOPDOWN_BULLET_VERT, SHADER_HANDLE_TOPDOWN_BULLET_FRAG);

  GLMesh bullet_mesh;
  bullet_mesh.vbos[0] = allocate_vbo(sizeof(bullet_manager->render_data), GL_DYNAMIC_DRAW);
  bullet_mesh.num_vbos = 1;
  bullet_mesh.num_vertices = 4;
  init_gl_mesh_vao(&bullet_mesh, SHADER_HANDLE_TOPDOWN_BULLET_VERT);

  GLMaterial bullet_material = create_gl_material(bullet_program);
  bullet_material.primitive = GL_TRIANGLE_STRIP;
  gl_material_add_uniform(&bullet_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Enemy
  u32 enemy_program = shader_handles_to_gl_program(SHADER_HANDLE_TOPDOWN_ENEMY_VERT, SHADER_HANDLE_TOPDOWN_ENEMY_FRAG);

  GLMesh enemy_mesh;
  enemy_mesh.vbos[0] = allocate_vbo(MEMBER_SIZE(EnemyManager, render_data), GL_DYNAMIC_DRAW);
  enemy_mesh.num_vbos = 1;
  enemy_mesh.num_vertices = 4;
  init_gl_mesh_vao(&enemy_mesh, SHADER_HANDLE_TOPDOWN_ENEMY_VERT);

  GLMaterial enemy_material = create_gl_material(enemy_program);
  enemy_material.primitive = GL_TRIANGLE_STRIP;
  gl_material_add_uniform(&enemy_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Screen Overlay
  u32 overlay_program = shader_handles_to_gl_program(
      SHADER_HANDLE_TOPDOWN_BULLET_HELL_OVERLAY_VERT, SHADER_HANDLE_TOPDOWN_BULLET_HELL_OVERLAY_FRAG
  );
  u32 overlay_ubo = create_gl_ubo(sizeof(BulletHellData), GL_DYNAMIC_DRAW);
  gl_bind_ubo_to_block(overlay_program, overlay_ubo, UNIFORM_BUFFER_LABEL_BULLET_HELL_DATA, "BulletHellData");

  // Make Player
  Player player{
      .pos = glm::vec3(0.0f, 0.0f, 0.0f),
      .size = glm::vec3(PLAYER_SIDE_LENGTH_METERS),
      .current_health = 100,
      .max_health = 100,
      .invincibility_time = 0.0f,
      .state = PLAYER_STATE_NORMAL,
      .ability = PLAYER_ABILITY_DASH,
      .unlocked_abilities_mask = 0,
      .dash =
          {
              .cooldown_left = 0.0f,
              .time_dilation_factor = 0.6f,
          },
      .speed_boost =
          {
              .boost_factor = 2.0f,
          },
      .slow_motion = {
          .time_dilation_factor = 0.5f,
      },
  };

  // Make Scene
  BulletHellSceneData bullet_hell{
      .player = player,
      .bullet_manager = bullet_manager,
      .bullet_spawn_time = 0.0,
      // FIXME need a real scale for number of billboards
      .billboard_manager = create_billboard_manager(5, vp_ubo),
      .camera = bullet_hell_camera,
      .vp_ubo = vp_ubo,
      .player_mesh = bullet_player_mesh,
      .player_material = bullet_player_material,
      .arena_mesh = arena_mesh,
      .arena_material = arena_material,
      .bullet_mesh = bullet_mesh,
      .bullet_material = bullet_material,
      .enemy_mesh = enemy_mesh,
      .enemy_material = enemy_material,
      .overlay_program = overlay_program,
  };

  bullet_hell.uniforms[UNIFORM_PLAYER_MODEL] = player_model_ubo;
  bullet_hell.uniforms[UNIFORM_PLAYER_FRAG] = player_frag_ubo;
  bullet_hell.uniforms[UNIFORM_OVERLAY] = overlay_ubo;

  // Put an enemy that moves sinusoidally in x 80% up the arena
  memset(&bullet_hell.enemy_manager, 0, sizeof(EnemyManager));
  bullet_hell.enemy_manager.num_live_enemies = 1;
  f32 enemy_height = BULLET_HELL_ARENA_HALF_HEIGHT * 0.8f;
  bullet_hell.enemy_manager.enemies[0].position.y = enemy_height;
  bullet_hell.enemy_manager.render_data[0].pos.y = enemy_height;

  return bullet_hell;
}
