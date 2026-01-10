#pragma once

#include "topdown.h"

#define MAX_NUM_BULLETS (512)

const f32 BULLET_HELL_ARENA_WIDTH = 10.0f;
const f32 BULLET_HELL_ARENA_HEIGHT = 8.0f;
const f32 BULLET_HELL_ARENA_HALF_WIDTH = BULLET_HELL_ARENA_WIDTH / 2.0;
const f32 BULLET_HELL_ARENA_HALF_HEIGHT = BULLET_HELL_ARENA_HEIGHT / 2.0;

// clang-format off
const f32 arena_vertices[] = {
  // x,y,z              u, v
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
struct Bullet {
  BulletPattern pattern;
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 size;
};

struct BulletManager {
  Bullet bullets[MAX_NUM_BULLETS];
  glm::vec2 render_positions[MAX_NUM_BULLETS];

  u32 num_live_bullets;
};

struct BulletHellSceneData {
  // player_pos is the position within the bullet hell arena
  // The arena's center is (0.0, 0.0)
  glm::vec3 player_pos;
  BulletManager *bullet_manager;

  Camera camera;
  u32 vp_ubo;

  u32 player_model_ubo;
  OpenGLMesh player_mesh;
  OpenGLMaterial player_material;

  OpenGLMesh arena_mesh;
  OpenGLMaterial arena_material;
};

inline void bullet_hell_move_player(BulletHellSceneData *scene_data, GlobalState *global_state, f32 dt) {

  const f32 X_BOUNDARY = 0.5 * (BULLET_HELL_ARENA_WIDTH - PLAYER_SIDE_LENGTH_METERS);
  const f32 Y_BOUNDARY = 0.5 * (BULLET_HELL_ARENA_HEIGHT - PLAYER_SIDE_LENGTH_METERS);

  const f32 speed = 5.0f;
  glm::vec3 input_movement_vector = speed * dt * inputs_to_movement_vector(&global_state->inputs);

  glm::vec3 *player_pos = &scene_data->player_pos;

  f32 next_x = player_pos->x + input_movement_vector.x;
  f32 clamped_next_x = clamp_f32(next_x, -X_BOUNDARY, X_BOUNDARY);

  f32 next_y = player_pos->y + input_movement_vector.y;
  f32 clamped_next_y = clamp_f32(next_y, -Y_BOUNDARY, Y_BOUNDARY);

  player_pos->x = clamped_next_x;
  player_pos->y = clamped_next_y;
}

inline void bullet_hell_update(void *scene_data, void *global_state, f32 dt) {
  GlobalState *gs = (GlobalState *)global_state;
  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;

  bullet_hell_move_player(data, gs, dt);

  glm::mat4 player_model = glm::translate(glm::mat4(1.0f), data->player_pos);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
  glBindBuffer(GL_UNIFORM_BUFFER, data->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  // TODO: Need window resize callback. Want to only update and rebuffer when there's new data.
  buffer_vp_matrix_to_gl_ubo(&data->camera, data->vp_ubo, gs->window_width, gs->window_height);
}

inline void bullet_hell_draw(const void *scene_data) {
  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;
  glClear(GL_COLOR_BUFFER_BIT);
  draw_opengl_mesh(&data->arena_mesh, data->arena_material);
  draw_opengl_mesh_instanced(&data->arena_mesh, data->arena_material, data->bullet_manager->num_live_bullets);
  draw_opengl_mesh(&data->player_mesh, data->player_material);
}

// inline void spawn_bullets(BulletHellSceneData *scene, u32 num_bullets, BulletPattern pattern) { }

inline BulletHellSceneData new_bullet_hell_scene(u32 vp_ubo) {
  Camera bullet_hell_camera = new_camera(CAMERA_TYPE_2D);
  bullet_hell_camera.position.z = 15.0f;

  u32 player_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  u32 player_model_ubo = create_opengl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  OpenGLMaterial player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_BULLET_HELL_PLAYER_MODEL,
                              "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  u32 arena_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_TOPDOWN_ARENA_VERT, SHADER_HANDLE_TOPDOWN_ARENA_FRAG);

  OpenGLMesh bullet_player_mesh = create_opengl_mesh_with_vertex_layout(
      player_vertices, sizeof(player_vertices), 6, VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial bullet_player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_PLAYER_MODEL, "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  OpenGLMesh arena_mesh = create_opengl_mesh_with_vertex_layout(arena_vertices, sizeof(arena_vertices), 6,
                                                                VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial arena_material = create_opengl_material(arena_program);
  opengl_material_add_uniform(&arena_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  BulletHellSceneData bullet_hell{
      .camera = bullet_hell_camera,
      .player_pos = glm::vec3(0.0f, 0.0f, 0.0f),
      .vp_ubo = vp_ubo,
      .player_model_ubo = player_model_ubo,
      .player_mesh = bullet_player_mesh,
      .player_material = bullet_player_material,
      .arena_mesh = arena_mesh,
      .arena_material = arena_material,
  };

  bullet_hell.bullet_manager = (BulletManager *)malloc(sizeof(BulletManager));
  memset(bullet_hell.bullet_manager, 0, sizeof(BulletManager));

  // RENDERER BRINGUP
  // render a line of bullets in the middle of the arena
  u32 NUM_BULLETS = 10;
  f32 dx = BULLET_HELL_ARENA_HALF_WIDTH / (f32)NUM_BULLETS;
  for (u32 i = 0; i < NUM_BULLETS; i++) {
    bullet_hell.bullet_manager->bullets->pattern = BULLET_PATTERN_LINEAR;
    bullet_hell.bullet_manager->bullets->position.x = -BULLET_HELL_ARENA_HALF_WIDTH + i * dx;
    bullet_hell.bullet_manager->bullets->position.y = 0.0f;
    bullet_hell.bullet_manager->bullets->size = glm::vec3(0.3f);
    bullet_hell.bullet_manager->bullets->velocity = glm::vec3(0.0f);
  }

  bullet_hell.bullet_manager->num_live_bullets = NUM_BULLETS;

  return bullet_hell;
}
