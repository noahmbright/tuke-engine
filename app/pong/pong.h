#pragma once

#include "camera.h"
#include "physics.h"
#include "shaders.h"
#include "statistics.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_base.h"
#include "window.h"

#define MAX_POWERUPS (8)
#define POWERUP_MAX (MAX_POWERUPS - 1)

// clang-format off
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

const f32 INTRO_DURATION = 2.0f;

// Using dimensions in meters
// 30m is about 96ft, a basketball court
const f32 aspect_ratio = 16.0f / 9.0f;
const f32 arena_dimensions_x0 = 30.0f;
const f32 arena_dimensions_y0 = arena_dimensions_x0 / aspect_ratio;
const f32 x_inset_from_wall0 = 0.05f * arena_dimensions_x0;
const f32 x_offset0 = arena_dimensions_x0 / 2.0f - x_inset_from_wall0;
const Vec3 arena_dimensions0{arena_dimensions_x0, arena_dimensions_y0, 1.0f};

typedef enum {
  ENTITY_LEFT_PADDLE = 0,
  ENTITY_RIGHT_PADDLE,
  ENTITY_BALL,
  NUM_ENTITIES,
} EntityIndex;

typedef enum {
  PADDLE_NEITHER,
  PADDLE_LEFT,
  PADDLE_RIGHT,
  NUM_PADDLES,
} PaddleID;

const f64 powerup_draw_interval_in_sec = 1.0f;
const f32 prob_powerup_spawns = 0.2f;
const f32 powerup_time0 = 5.0f;

const f32 speed0 = 12.5f;
const f32 cpu_speed0 = .8 * speed0;

const Vec3 paddle_scale0 = vec3(1.0f, 4.0f, 1.0f);
const Vec3 ball_scale0 = vec3(0.5f, 0.5f, 0.5f);
const Vec3 powerup_size = vec3(1.0f, 1.0f, 1.0f);
const f32 z0 = 0.25f;

const Vec3 left_paddle_pos0 = vec3(-x_offset0, 0.0f, z0);
const Vec3 right_paddle_pos0 = vec3(x_offset0, 0.0f, z0);
const Vec3 ball_pos0 = vec3(0.0f, 0.0f, z0);

const u32 PLAYER_SELECT_ROWS = 2;
const f32 PLAYER_PORTRAIT_WIDTH = 0.4f;
const f32 PORTRAIT_Y_OFFSET = 0.2f;

typedef enum {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_FIELD_BACKGROUND,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  TEXTURE_MENU_UI,
  TEXTURE_CHARACTERS,

  NUM_TEXTURES
} TextureID;

typedef enum {
  GAMEMODE_PAUSED,
  GAMEMODE_PLAYING,
  GAMEMODE_MAIN_MENU,
} GameMode;

typedef enum {
  MOVEMENT_MODE_VERTICAL_ONLY,
} MovementMode;

typedef enum {
  PONG_MODE_BETWEEN_POINTS,
  PONG_MODE_LIVE_BALL,
} PongMode;

typedef struct {
  Vec3 position;
  Vec3 velocity;
  Vec3 size;
} Paddle;

typedef struct {
  RNG ball_direction;
  RNG powerup_spawn;
} RNGs;

typedef enum {
  POWERUP_SIGNATURE_MOVE,
  POWERUP_BIG_PADDLE,
  POWERUP_KILL_OPPONENT,

  NUM_POWERUP_TYPES
} PowerUpType;

const f32 powerup_likelihoods[NUM_POWERUP_TYPES] = {
    [POWERUP_BIG_PADDLE] = 1.0f,
    [POWERUP_KILL_OPPONENT] = 0.5f,
};

typedef struct {
  PowerUpType type;
  f32 time_remaining;
  Vec3 pos;
} PowerUp;

//////////////////////////////////////////////////
/// Main menu
//////////////////////////////////////////////////
typedef enum {
  MAIN_MENU_UI_LOGO,
  MAIN_MENU_UI_STORY,
  MAIN_MENU_UI_1V1,
  MAIN_MENU_UI_OPTIONS,

  NUM_MENU_UIS,
} MainMenuUI;

typedef enum {
  CHARACTER_PADDLE,
  CHARACTER_BABOON,
  CHARACTER_GORILLA,
  CHARACTER_NOBU,
  CHARACTER_EL_GAUCHO, // https://en.wikipedia.org/wiki/Gaucho

  NUM_CHARACTERS,
} CharacterID;

typedef enum {
  MENU_TYPE_MAIN,
  MENU_TYPE_1V1,
  MENU_TYPE_OPTIONS,

  NUM_MENU_TYPE,
} MenuType;

typedef struct {
  Vec2 center;
  Vec2 size;
  f32 rotation;
  u32 tex_id;
  u32 flags;
  // Effects?
} UiElement;

// Fat struct for all possible menus before the game begins
typedef struct {
  MenuType menu_type; // Tag
  f32 anim_t;         // Generalized until proven impossible

  // Main Menu
  MainMenuUI selected_option;
  Vec2 selector_pos;
  bool intro_active;

  // 1V1 menu
  CharacterID selected_character;
} MenuState;

typedef struct {
  Vec3 arena_dimensions;
  u32 right_score, left_score;

  Vec3 lpaddle_pos;
  Vec3 lpaddle_vel;
  Vec3 lpaddle_size;
  CharacterID lcharacter;

  Vec3 rpaddle_pos;
  Vec3 rpaddle_vel;
  Vec3 rpaddle_size;
  CharacterID rcharacter;

  Vec3 ball_pos;
  Vec3 ball_vel;
  Vec3 ball_size;

  f32 left_paddle_cooldown;
  f32 right_paddle_cooldown;

  MovementMode movement_mode;
  PongMode pong_mode;

  f32 lpaddle_speed;
  f32 rpaddle_speed;
  f32 ball_speed;

  f64 time_since_last_powerup_draw;
  PaddleID last_paddle;
  struct {
    f32 time_left;
  } paddle_powerups[NUM_PADDLES][NUM_POWERUP_TYPES];
  u32 left_paddle_powerup_flags;
  u32 right_paddle_powerup_flags;

  Camera camera;
  ScreenShake screen_shake;

  PowerUp powerups[MAX_POWERUPS];
  u32 current_powerup_index;
  AliasTable powerup_alias_table;
  RNGs rngs;
} PlayingState;

typedef struct {
  VulkanContext ctx;
  VulkanTexture textures[NUM_TEXTURES];
  VkDescriptorSetLayout descriptor_set_layouts[NUM_DESCRIPTOR_SET_LAYOUTS]; // Storage
  VkClearValue clear_values[NUM_ATTACHMENTS];
  UniformBuffer uniform_buffer;
  BufferManager buffer_manager;
  StreamingBuffer ui_buffer;

  VulkanMesh mesh;
  VulkanMaterial background_mat;
  VulkanMaterial paddle_mat;
  VulkanMaterial main_menu_mat;
  VulkanMaterial ui_mat;
  VulkanMaterial characters_mat;
} Renderer;

typedef struct {
  GLFWwindow *window;
  int window_width;
  int window_height;

  UiElement ui_elements[32];

  GameMode game_mode;
  MenuState menu_state;
  PlayingState playing_state;

  u32 current_frame;
  f64 time;

  Renderer renderer;
  Inputs inputs;
} State;

State setup_state(const char *title);
void destroy_state(State *state);

void initialize_textures(u32 num_textures, VulkanTexture *out_textures);
void render(State *state);
void process_inputs(State *state, const f32 dt);
void update_game_state(State *state, const f32 dt);
