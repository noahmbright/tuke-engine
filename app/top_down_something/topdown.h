#pragma once

#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "scene_manager.h"
#include "tilemap.h"
#include "tuke_engine.h"
#include "utils.h"

#define PLAYER_SIDE_LENGTH_METERS (0.6f)

const u32 width_in_tiles = 9;
const u32 height_in_tiles = 20;

// clang-format off
inline u8 tilemap_data[width_in_tiles * height_in_tiles] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 2, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
};

inline Tilemap tilemap = new_tilemap(width_in_tiles, height_in_tiles, &tilemap_data[0]);


// map 1
const u32 width_in_tiles1 = 16;
const u32 height_in_tiles1 = 8;

inline u8 tilemap1_data[width_in_tiles1 * height_in_tiles1] = {
    1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 2, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 1,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 1,   1, 1, 1, 1,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,
    1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,
};

inline Tilemap tilemap1 = new_tilemap(width_in_tiles1, height_in_tiles1, &tilemap1_data[0]);

const f32 player_vertices[] = {
  // x,y,z              u, v
  -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // BL
  -0.5f,  0.5f,  0.0f,  0.0f, 1.0f, // TL
   0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // TR

   0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // TR
   0.5f, -0.5f,  0.0f,  1.0f, 0.0f, // BR
  -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // BL
};
// clang-format on

inline void buffer_vp_matrix_to_gl_ubo(const Camera *camera, u32 ubo, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices = new_camera_matrices(camera, window_width, window_height);
  glm::mat4 vp = camera_matrices.projection * camera_matrices.view;

#ifndef NDEBUG
  if (matrix_has_nan(camera_matrices.view)) {
    exit(1);
  }
#endif

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(vp), &vp);
}

inline glm::vec3 inputs_to_movement_vector(const Inputs *inputs) {
  glm::vec3 movement_vector = inputs_to_direction(inputs);

  f32 xy_plane_speed = 1.0f;
  if (key_held(inputs, INPUT_LEFT_SHIFT)) {
    xy_plane_speed += xy_plane_speed; // 2x speed
  }
  movement_vector *= xy_plane_speed;

  f32 zoom_speed = 10.0f;
  movement_vector.z = zoom_speed * inputs->scroll_dy;

  return movement_vector;
}

enum SceneID { SCENE0, SCENE1, SCENE_BULLET_HELL, NUM_SCENES, SCENE_NONE };

struct OverworldSceneData {
  glm::vec3 player_pos;
  Camera camera;
  Tilemap *tilemap;

  u32 vp_ubo;

  u32 player_model_ubo;
  OpenGLMesh player_mesh;
  OpenGLMaterial player_material;

  OpenGLMesh tilemap_mesh;
  OpenGLMaterial tilemap_material;

  SceneID other_scene;
  bool just_transitioned;
};

struct GlobalState {
  int window_width, window_height;
  SceneManager scene_manager;
  Inputs inputs;
  f64 t;
};

inline void move_camera_in_tilemap(OverworldSceneData *scene_data, GlobalState *global_state, f32 dt) {

  const f32 speed = 5.0f;
  const glm::vec3 tile_size(PLAYER_SIDE_LENGTH_METERS);

  Camera *camera = &scene_data->camera;
  Tilemap *tilemap = scene_data->tilemap;

  glm::vec3 last_camera_pos = camera->position;
  glm::vec3 input_movement_vector = speed * dt * inputs_to_movement_vector(&global_state->inputs);
  glm::vec3 final_movement_vector(0.0f);

  // Right now, tile == 0 means move. Otherwise, reset motion
  glm::vec3 x_moved_position = last_camera_pos + glm::vec3(input_movement_vector.x, 0.0f, 0.0f);
  u8 x_tile = tilemap_check_collision(tilemap, x_moved_position, tile_size);
  final_movement_vector.x = (x_tile == 0) * input_movement_vector.x;

  glm::vec3 y_moved_position = last_camera_pos + glm::vec3(0.0f, input_movement_vector.y, 0.0f);
  u8 y_tile = tilemap_check_collision(tilemap, y_moved_position, tile_size);
  final_movement_vector.y = (y_tile == 0) * input_movement_vector.y;

  f32 z_moved_position = camera->position.z + input_movement_vector.z;
  camera->position.z = clamp_f32(z_moved_position, 1.0f, CAMERA_PERSPECTIVE_PROJECTION_FAR_Z - EPSILON);

  move_camera(camera, final_movement_vector);

  // TODO Define magic numbers in the tilemap
  bool should_transition = (x_tile == 2 || y_tile == 2 || key_pressed(&global_state->inputs, INPUT_KEY_T));
  if (!scene_data->just_transitioned && should_transition) {
    scene_data->just_transitioned = true;
    global_state->scene_manager.pending_scene = scene_data->other_scene;
    global_state->scene_manager.pending_scene_action = SCENE_ACTION_SET;
  }
  scene_data->just_transitioned = should_transition;
}

// TODO separate player position and camera position
// TODO will i consider the players position within a tilemap? within the whole world?
inline void scene0_update(void *scene_data, void *global_state, f32 dt) {

  GlobalState *gs = (GlobalState *)global_state;
  OverworldSceneData *overworld_sd = (OverworldSceneData *)scene_data;

  move_camera_in_tilemap(overworld_sd, gs, dt);

  glm::vec3 camera_xy(overworld_sd->camera.position.x, overworld_sd->camera.position.y, 0.0f);
  glm::mat4 player_model = glm::translate(glm::mat4(1.0f), camera_xy);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));

  glBindBuffer(GL_UNIFORM_BUFFER, overworld_sd->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  buffer_vp_matrix_to_gl_ubo(&overworld_sd->camera, overworld_sd->vp_ubo, gs->window_width, gs->window_height);
}

inline void scene0_draw(const void *scene_data) {
  OverworldSceneData *overworld_data = (OverworldSceneData *)scene_data;
  glClear(GL_COLOR_BUFFER_BIT);
  draw_opengl_mesh(&overworld_data->tilemap_mesh, overworld_data->tilemap_material);
  draw_opengl_mesh(&overworld_data->player_mesh, overworld_data->player_material);
}
