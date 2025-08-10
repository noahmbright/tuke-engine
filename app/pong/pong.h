#pragma once

#include "glm/glm.hpp"
#include "tuke_engine.h"
#include "vulkan_base.h"

#define MAT4_SIZE (sizeof(glm::mat4))

// clang-format off
// start with rectangles, TODO cube
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
const f32 x_offset0 = 0.05f * arena_dimensions_x0;
const glm::vec3 arena_dimensions0{arena_dimensions_x0, arena_dimensions_y0,
                                  1.0f};

enum EntityIndex {
  ENTITY_LEFT_PADDLE = 0,
  ENTITY_RIGHT_PADDLE,
  ENTITY_BALL,
  ENTITY_BACKGROUND,
  NUM_ENTITIES
};

// clang-format off
const f32 instance_data0[] = {
    -arena_dimensions_x0 + x_offset0, 0.0f, 0.0f, // left paddle
     arena_dimensions_x0 - x_offset0, 0.0f, 0.0f, // right paddle
     0.0f, 0.0f, 0.0f, // ball
};
// clang-format on

const u32 paddle_vertices_size = sizeof(paddle_vertices);
const u32 indices_size = sizeof(unit_square_indices);
const u32 instance_data_size = sizeof(instance_data0);
const u32 total_size = paddle_vertices_size + indices_size + instance_data_size;

enum TextureID {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_FIELD_BACKGROUND,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  NUM_TEXTURES
};

enum DescriptorHandleID {
  DESCRIPTOR_HANDLE_BACKGROUND,
  DESCRIPTOR_HANDLE_PADDLES_AND_BALL,
  NUM_DESCRIPTOR_HANDLES
};

enum GameMode {
  GAMEMODE_PAUSED,
  GAMEMODE_PLAYING,
  GAMEMODE_MAIN_MENU,
};

struct UniformWrites {
  UniformWrite camera_vp;
  UniformWrite arena_model;
  UniformWrite player_paddle_model;
};

struct State {
  u32 right_score, left_score;

  glm::vec2 arena_dimensions;

  VulkanContext context;
  VulkanTexture textures[NUM_TEXTURES];
  VulkanBuffer vertex_buffer;
  VulkanBuffer index_buffer;
  VkSampler sampler;
  DescriptorSetHandle descriptor_set_handles[NUM_DESCRIPTOR_HANDLES];
  VkDescriptorPool descriptor_pool;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkClearValue clear_value;
  ViewportState viewport_state;
  RenderCall render_call;

  UniformBuffer uniform_buffer;
  UniformWrites uniform_writes;

  Inputs inputs;
  GameMode game_mode;
};

struct Paddle {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::vec3 size;
};

State setup_state(const char *title);
void destroy_state(State *state);

void initialize_textures(u32 num_textures, VulkanTexture *out_textures);
void render(State *state);
void process_inputs(State *state);
void write_uniforms(State *state);
