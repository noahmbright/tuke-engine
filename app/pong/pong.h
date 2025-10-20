#pragma once

#include "c_reflector_bringup.h"
#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "physics.h"
#include "statistics.h"
#include "transform.h"
#include "tuke_engine.h"
#include "vulkan_base.h"

#define MAX_POWERUPS (8)
#define POWERUP_MAX (MAX_POWERUPS - 1)

// clang-format off
// start with rectangles, TODO Pong: cube
// TL, BL, BR, TR
const f32 paddle_vertices[] = {
   // x, y, z         u, v
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};
// clang-format on

// using dimensions in meters
// 30m is about 96ft, a basketball court
const f32 aspect_ratio = 16.0f / 9.0f;
const f32 arena_dimensions_x0 = 30.0f;
const f32 arena_dimensions_y0 = arena_dimensions_x0 / aspect_ratio;
const f32 x_inset_from_wall0 = 0.05f * arena_dimensions_x0;
const f32 x_offset0 = arena_dimensions_x0 / 2.0f - x_inset_from_wall0;
const glm::vec3 arena_dimensions0{arena_dimensions_x0, arena_dimensions_y0, 1.0f};

enum EntityIndex { ENTITY_LEFT_PADDLE = 0, ENTITY_RIGHT_PADDLE, ENTITY_BALL, NUM_ENTITIES };

enum LastPaddle {
  LAST_PADDLE_NEITHER,
  LAST_PADDLE_LEFT,
  LAST_PADDLE_RIGHT,
};

const f64 powerup_draw_interval_in_sec = 1.0f;
const f32 prob_powerup_spawns = 0.2f;

const f32 speed0 = 12.5f;
const f32 cpu_speed0 = .8 * speed0;

const glm::vec3 paddle_scale0{1.0f, 4.0f, 1.0f};
const glm::vec3 ball_scale0{0.5f, 0.5f, 0.5f};
const f32 z0 = 0.25f;

const glm::vec3 left_paddle_pos0{-x_offset0, 0.0f, z0};
const glm::vec3 right_paddle_pos0{x_offset0, 0.0f, z0};
const glm::vec3 ball_pos0{0.0f, 0.0f, z0};

const glm::mat4 left_paddle_translated0 = glm::translate(glm::mat4(1.0f), left_paddle_pos0);
const glm::mat4 left_paddle_model0 = glm::scale(left_paddle_translated0, paddle_scale0);

const glm::mat4 right_paddle_translated0 = glm::translate(glm::mat4(1.0f), right_paddle_pos0);
const glm::mat4 right_paddle_model0 = glm::scale(right_paddle_translated0, paddle_scale0);

const glm::mat4 ball_translated0 = glm::translate(glm::mat4(1.0f), ball_pos0);
const glm::mat4 ball_model0 = glm::scale(ball_translated0, ball_scale0);

const u32 paddle_vertices_size = sizeof(paddle_vertices);
const u32 indices_size = sizeof(unit_square_indices);

enum TextureID {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_FIELD_BACKGROUND,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  NUM_TEXTURES
};

enum DescriptorHandleID {
  DESCRIPTOR_HANDLE_BACKGROUND,
  DESCRIPTOR_HANDLE_GLOBAL_VP,
  DESCRIPTOR_HANDLE_PADDLES_AND_BALL,
  NUM_DESCRIPTOR_HANDLES
};

enum GameMode {
  GAMEMODE_PAUSED,
  GAMEMODE_PLAYING,
  GAMEMODE_MAIN_MENU,
};

enum MovementMode {
  MOVEMENT_MODE_VERTICAL_ONLY,
  MOVEMENT_MODE_HORIZONTAL_ENABLED,
};

enum PongMode {
  PONG_MODE_BETWEEN_POINTS,
  PONG_MODE_LIVE_BALL,
};

struct Material {
  DescriptorSetHandle descriptor_set_handle;
  VkPipelineLayout pipeline_layout;
  RenderCall render_call;
  VkPipeline pipeline;
};

inline void destroy_material(VulkanContext *ctx, Material *material) {

  vkDestroyPipelineLayout(ctx->device, material->pipeline_layout, NULL);
  vkDestroyPipeline(ctx->device, material->pipeline, NULL);
}

struct UniformWrites {
  UniformWrite camera_vp;
  UniformWrite arena_model;
  UniformWrite instance_data;
};

struct Paddle {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::vec3 size;
};

struct RNGs {
  RNG ball_direction;
  RNG powerup_spawn;
};

enum PowerUpType {
  POWERUP_BIG_PADDLE,
  POWERUP_KILL_OPPONENT,

  NUM_POWERUPS
};

const f32 powerup_likelihoods[NUM_POWERUPS] = {
    [POWERUP_BIG_PADDLE] = 1.0f,
    [POWERUP_KILL_OPPONENT] = 0.5f,
};

const glm::vec3 powerup_scale{1.0f, 1.f, 1.0f};

struct PowerUp {
  PowerUpType type;
  f32 time_remaining;
  glm::vec4 position;
};

struct State {
  u32 right_score, left_score;

  glm::vec2 arena_dimensions;

  VulkanContext context;
  VulkanTexture textures[NUM_TEXTURES];
  VkSampler sampler;
  DescriptorSetHandle descriptor_set_handles[NUM_DESCRIPTOR_HANDLES];
  VkDescriptorPool descriptor_pool;
  VkClearValue clear_values[NUM_ATTACHMENTS];

  Material background_material;
  Material paddle_material;

  ViewportState viewport_state;

  UniformBuffer uniform_buffer;
  UniformWrites uniform_writes;
  BufferManager buffer_manager;
  InstanceDataUBO instance_data;

  Inputs inputs;
  RNGs rngs;

  // TODO engine: figure out a system for batching rewrites that occur often
  // vs ones that occur infrequently
  // allow for a system that avoids recomputing matrix multiplications for
  // things that don't change
  //
  // this stuff all should be updated every frame, only the paddles and ball
  Transform transforms[NUM_ENTITIES];
  glm::vec3 positions[NUM_ENTITIES];
  glm::vec3 velocities[NUM_ENTITIES];
  glm::vec3 scales[NUM_ENTITIES];

  f32 left_paddle_cooldown;
  f32 right_paddle_cooldown;

  GameMode game_mode;
  MovementMode movement_mode;
  PongMode pong_mode;

  f32 left_paddle_speed;
  f32 right_paddle_speed;
  f32 ball_speed;

  f64 time_since_last_powerup_draw;
  LastPaddle last_paddle_to_hit;
  u32 left_paddle_powerup_flags;
  u32 right_paddle_powerup_flags;

  Camera camera;
  ScreenShake screen_shake;

  PowerUp powerups[MAX_POWERUPS];
  u32 current_powerup_index;
  AliasMethod powerup_alias_table;
};

State setup_state(const char *title);
void destroy_state(State *state);

void initialize_textures(u32 num_textures, VulkanTexture *out_textures);
void render(State *state);
void process_inputs(State *state, const f32 dt);
void update_game_state(State *state, const f32 dt);
