#pragma once

#include "c_reflector_bringup.h"
#include "generated_shader_utils.h"
#include "glm/common.hpp"
#include "opengl_base.h"
#include "topdown.h"
#include "tuke_engine.h"
#include <OpenGL/OpenGL.h>

#define MAX_NUM_BULLETS (512)
#define MAX_NUM_ENEMIES (8)

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
  f32 t0; // Spawn time
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

struct BulletHellSceneData {
  // player_pos is the position within the bullet hell arena
  // The arena's center is (0.0, 0.0)
  glm::vec3 player_pos;
  BulletManager *bullet_manager;
  EnemyManager enemy_manager;

  Camera camera;
  u32 vp_ubo;

  u32 player_model_ubo;
  OpenGLMesh player_mesh;
  OpenGLMaterial player_material;

  OpenGLMesh arena_mesh;
  OpenGLMaterial arena_material;

  OpenGLMesh bullet_mesh;
  OpenGLMaterial bullet_material;

  OpenGLMesh enemy_mesh;
  OpenGLMaterial enemy_material;
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

// Supposing a rectangle of half side lengths width and height is positioned at the origin
inline f32 rectangle_sdf(f32 half_width, f32 half_height, glm::vec2 pos) {

  glm::vec2 abs_pos = glm::abs(pos);
  glm::vec2 rect_vec = glm::vec2(half_width, half_height);
  glm::vec2 diff = abs_pos - rect_vec;

  glm::vec2 clamped_diff = glm::max(glm::vec2(0.0f), diff);
  f32 dist_outside = glm::length(clamped_diff);
  f32 dist_inside = fmin(fmax(diff.x, diff.y), 0.0f);

  return dist_outside + dist_inside;
}

inline void update_bullets(BulletManager *bullet_manager) {

  u32 live_bullet_index = 0;

  for (u32 i = 0; i < MAX_NUM_BULLETS; i++) {

    Bullet *bullet = &bullet_manager->bullets[i];
    bullet->position += bullet->velocity;

    f32 box_bullet_signed_distance =
        rectangle_sdf(BULLET_HELL_ARENA_HALF_WIDTH, BULLET_HELL_ARENA_HALF_HEIGHT, bullet->position);

    if (box_bullet_signed_distance < 0.0f) {
      bullet_manager->render_data[live_bullet_index++].pos = bullet->position;
    }
  }

  bullet_manager->num_live_bullets = live_bullet_index;
}

inline void bullet_hell_update(void *scene_data, void *global_state, f32 dt) {
  GlobalState *gs = (GlobalState *)global_state;
  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;
  BulletManager *bullet_manager = data->bullet_manager;
  EnemyManager *enemy_manager = &data->enemy_manager;

  bullet_hell_move_player(data, gs, dt);

  // If I add slow motion or anything like that, will need to consider how to track time updates
  gs->t += dt;

  glm::mat4 player_model = glm::translate(glm::mat4(1.0f), data->player_pos);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
  glBindBuffer(GL_UNIFORM_BUFFER, data->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  update_bullets(bullet_manager);
  glBindBuffer(GL_ARRAY_BUFFER, data->bullet_mesh.vbos[VBO_INSTANCE]);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(BulletRenderData) * bullet_manager->num_live_bullets,
                  &bullet_manager->render_data);

  // enemy update bringup
  f32 x_amp = BULLET_HELL_ARENA_HALF_WIDTH * 0.8f;
  data->enemy_manager.enemies[0].position.x = x_amp * sinf(gs->t);
  data->enemy_manager.render_data[0].pos.x = x_amp * sinf(gs->t);
  glBindBuffer(GL_ARRAY_BUFFER, data->enemy_mesh.vbos[VBO_INSTANCE]);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(EnemyRenderData) * enemy_manager->num_live_enemies,
                  &enemy_manager->render_data);

  // TODO: Need window resize callback. Want to only update and rebuffer when there's new data.
  buffer_vp_matrix_to_gl_ubo(&data->camera, data->vp_ubo, gs->window_width, gs->window_height);
}

inline void bullet_hell_draw(const void *scene_data) {
  BulletHellSceneData *data = (BulletHellSceneData *)scene_data;
  glClear(GL_COLOR_BUFFER_BIT);
  draw_opengl_mesh(&data->arena_mesh, data->arena_material);
  draw_opengl_mesh_instanced(&data->bullet_mesh, data->bullet_material, data->bullet_manager->num_live_bullets);
  draw_opengl_mesh_instanced(&data->enemy_mesh, data->enemy_material, data->enemy_manager.num_live_enemies);
  draw_opengl_mesh(&data->player_mesh, data->player_material);
}

// inline void spawn_bullets(BulletHellSceneData *scene, u32 num_bullets, BulletPattern pattern) { }

// --------------------- BIG INIT FUNCTION ---------------------
inline BulletHellSceneData new_bullet_hell_scene(u32 vp_ubo) {

  BulletManager *bullet_manager = (BulletManager *)malloc(sizeof(BulletManager));
  memset(bullet_manager, 0, sizeof(BulletManager));

  Camera bullet_hell_camera = new_camera(CAMERA_TYPE_2D);
  bullet_hell_camera.position.z = 15.0f;

  // Player
  u32 player_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  u32 player_model_ubo = create_opengl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  OpenGLMaterial player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_BULLET_HELL_PLAYER_MODEL,
                              "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  OpenGLMesh bullet_player_mesh = create_opengl_mesh_with_vertex_layout(
      player_vertices, sizeof(player_vertices), 6, VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial bullet_player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_PLAYER_MODEL, "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Arena
  u32 arena_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_TOPDOWN_ARENA_VERT, SHADER_HANDLE_TOPDOWN_ARENA_FRAG);

  OpenGLMesh arena_mesh = create_opengl_mesh_with_vertex_layout(arena_vertices, sizeof(arena_vertices), 6,
                                                                VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial arena_material = create_opengl_material(arena_program);
  opengl_material_add_uniform(&arena_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Bullets
  u32 bullet_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_TOPDOWN_BULLET_VERT, SHADER_HANDLE_TOPDOWN_BULLET_FRAG);

  OpenGLMesh bullet_mesh;
  bullet_mesh.vbos[VBO_INSTANCE] = allocate_vbo(sizeof(bullet_manager->render_data), GL_DYNAMIC_DRAW);
  bullet_mesh.num_vbos = 1;
  bullet_mesh.num_vertices = 4;
  init_opengl_mesh_vao(&bullet_mesh, SHADER_HANDLE_TOPDOWN_BULLET_VERT);

  OpenGLMaterial bullet_material = create_opengl_material(bullet_program);
  bullet_material.primitive = GL_TRIANGLE_STRIP;
  opengl_material_add_uniform(&bullet_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Enemy
  u32 enemy_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_TOPDOWN_ENEMY_VERT, SHADER_HANDLE_TOPDOWN_ENEMY_FRAG);

  OpenGLMesh enemy_mesh;
  enemy_mesh.vbos[VBO_INSTANCE] = allocate_vbo(member_size(EnemyManager, render_data), GL_DYNAMIC_DRAW);
  enemy_mesh.num_vbos = 1;
  enemy_mesh.num_vertices = 4;
  init_opengl_mesh_vao(&enemy_mesh, SHADER_HANDLE_TOPDOWN_ENEMY_VERT);

  OpenGLMaterial enemy_material = create_opengl_material(enemy_program);
  enemy_material.primitive = GL_TRIANGLE_STRIP;
  opengl_material_add_uniform(&enemy_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Make Scene
  BulletHellSceneData bullet_hell{
      .camera = bullet_hell_camera,
      .player_pos = glm::vec3(0.0f, 0.0f, 0.0f),
      .vp_ubo = vp_ubo,
      .player_model_ubo = player_model_ubo,
      .player_mesh = bullet_player_mesh,
      .player_material = bullet_player_material,
      .arena_mesh = arena_mesh,
      .arena_material = arena_material,
      .bullet_manager = bullet_manager,
      .bullet_mesh = bullet_mesh,
      .bullet_material = bullet_material,
      .enemy_mesh = enemy_mesh,
      .enemy_material = enemy_material,
  };

  memset(&bullet_hell.enemy_manager, 0, sizeof(EnemyManager));
  bullet_hell.enemy_manager.num_live_enemies = 1;
  f32 enemy_height = BULLET_HELL_ARENA_HALF_HEIGHT * 0.8f;
  bullet_hell.enemy_manager.enemies[0].position.y = enemy_height;
  bullet_hell.enemy_manager.render_data[0].pos.y = enemy_height;

  // RENDERER BRINGUP
  // render a line of bullets in the middle of the arena
  const u32 NUM_BULLETS = 20;
  f32 dx = BULLET_HELL_ARENA_WIDTH / (f32)NUM_BULLETS;
  for (u32 i = 0; i < NUM_BULLETS; i++) {

    f32 size = 0.3f;
    f32 x = -BULLET_HELL_ARENA_HALF_WIDTH + (i + 0.5f) * dx;

    bullet_hell.bullet_manager->bullets[i].pattern = BULLET_PATTERN_LINEAR;
    bullet_hell.bullet_manager->bullets[i].position.x = x;
    bullet_hell.bullet_manager->bullets[i].position.y = BULLET_HELL_ARENA_HALF_HEIGHT;

    bullet_hell.bullet_manager->bullets[i].size = glm::vec3(size);
    bullet_hell.bullet_manager->bullets[i].velocity = glm::vec3(0.0f, -0.1f, 0.0f);
    bullet_hell.bullet_manager->bullets[i].t0 = x; // lol

    bullet_hell.bullet_manager->render_data[i].pos.x = x;
    bullet_hell.bullet_manager->render_data[i].size = size;
  }

  bullet_hell.bullet_manager->num_live_bullets = NUM_BULLETS;

  return bullet_hell;
}
