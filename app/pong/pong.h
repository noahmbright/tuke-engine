#pragma once

#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "physics.h"
#include "shaders.h"
#include "statistics.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_base.h"
#include "window.h"

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
const Vec3 arena_dimensions0{arena_dimensions_x0, arena_dimensions_y0, 1.0f};

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

const Vec3 paddle_scale0{1.0f, 4.0f, 1.0f};
const Vec3 ball_scale0{0.5f, 0.5f, 0.5f};
const f32 z0 = 0.25f;

const Vec3 left_paddle_pos0{-x_offset0, 0.0f, z0};
const Vec3 right_paddle_pos0{x_offset0, 0.0f, z0};
const Vec3 ball_pos0{0.0f, 0.0f, z0};

const u32 paddle_vertices_size = sizeof(paddle_vertices);
const u32 indices_size = sizeof(unit_square_indices);

enum TextureID {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_FIELD_BACKGROUND,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  NUM_TEXTURES
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

struct UniformWrites {
  VkDescriptorBufferInfo camera_vp;
  VkDescriptorBufferInfo arena_model;
  VkDescriptorBufferInfo instance_data;
};

struct Paddle {
  Vec3 position;
  Vec3 velocity;
  Vec3 size;
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

const Vec3 powerup_scale{1.0f, 1.f, 1.0f};

struct PowerUp {
  PowerUpType type;
  f32 time_remaining;
  glm::vec4 position;
};

struct State {
  GLFWwindow *window;
  int window_width;
  int window_height;

  u32 right_score, left_score;
  u32 current_frame;
  f64 time;

  Vec3 arena_dimensions;

  // Renderer
  VulkanContext ctx;
  VulkanTexture textures[NUM_TEXTURES];
  VkSampler sampler;
  VkDescriptorSetLayout descriptor_set_layouts[NUM_DESCRIPTOR_SET_LAYOUTS];
  VkClearValue clear_values[NUM_ATTACHMENTS];
  ViewportState viewport_state;
  UniformBuffer uniform_buffer;
  UniformWrites uniform_writes;
  BufferManager buffer_manager;
  InstanceDataUBO instance_data;

  VulkanMesh mesh;
  VulkanMaterial background_material;
  VulkanMaterial paddle_material;
  VulkanMaterial main_menu_material;

  Inputs inputs;
  RNGs rngs;

  Vec3 positions[NUM_ENTITIES];
  Vec3 velocities[NUM_ENTITIES];
  Vec3 scales[NUM_ENTITIES];

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
