#pragma once

#include "camera.h"
#include "generated_shader_utils.h"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/geometric.hpp"
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
    1, 0, 0, 0, 0, 0, 3, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
};

inline Tilemap tilemap = create_tilemap(width_in_tiles, height_in_tiles, &tilemap_data[0]);


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

inline Tilemap tilemap1 = create_tilemap(width_in_tiles1, height_in_tiles1, &tilemap1_data[0]);

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
  CameraMatrices camera_matrices = create_camera_matrices(camera, window_width, window_height);
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

  f32 zoom_speed = 10.0f;
  movement_vector.z = zoom_speed * inputs->scroll_dy;

  return movement_vector;
}

enum SceneID { SCENE0, SCENE1, SCENE_BULLET_HELL, NUM_SCENES, SCENE_NONE };

struct OverworldSceneData {
  // Actual state specific to the overworld
  glm::vec3 player_pos;

  // CCW angle from right, in radians
  f32 player_rotation_simulation;
  f32 player_rotation_render;

  Camera camera;
  Tilemap *tilemap;

  SceneID other_scene;
  bool just_transitioned;

  // OpenGL Stuff
  u32 vp_ubo;

  u32 player_model_ubo;
  OpenGLMesh player_mesh;
  OpenGLMaterial player_material;

  OpenGLMesh tilemap_mesh;
  OpenGLMaterial tilemap_material;

  OpenGLRenderTarget render_target;
  OpenGLMesh fullscreen_quad_mesh;
  OpenGLMaterial fullscreen_quad_material;

  u32 overworld_overlay_program;
};

enum GameState {
  GAME_STATE_PAUSED,
  GAME_STATE_RUNNING,
};

struct GlobalState {
  int window_width;
  int window_height;
  SceneManager scene_manager;
  Inputs inputs;
  f64 t;
  GameState game_state;
};

inline void move_camera_in_tilemap(glm::vec3 movement_vector, OverworldSceneData *scene_data,
                                   GlobalState *global_state) {

  const glm::vec3 tile_size(PLAYER_SIDE_LENGTH_METERS);

  Camera *camera = &scene_data->camera;
  Tilemap *tilemap = scene_data->tilemap;

  glm::vec3 last_camera_pos = camera->position;
  glm::vec3 final_movement_vector(0.0f);

  // Right now, tile == 0 means move. Otherwise, reset motion
  glm::vec3 x_moved_position = last_camera_pos + glm::vec3(movement_vector.x, 0.0f, 0.0f);
  u8 x_tile = tilemap_check_collision(tilemap, x_moved_position, tile_size);
  final_movement_vector.x = (x_tile == 0) * movement_vector.x;

  glm::vec3 y_moved_position = last_camera_pos + glm::vec3(0.0f, movement_vector.y, 0.0f);
  u8 y_tile = tilemap_check_collision(tilemap, y_moved_position, tile_size);
  final_movement_vector.y = (y_tile == 0) * movement_vector.y;

  f32 z_moved_position = camera->position.z + movement_vector.z;
  camera->position.z = clamp_f32(z_moved_position, 1.0f, CAMERA_PERSPECTIVE_PROJECTION_FAR_Z - EPSILON);

  move_camera(camera, final_movement_vector);

  // TODO Define magic numbers in the tilemap
  // 2 is transition scene block
  bool should_transition = (x_tile == 2 || y_tile == 2 || key_pressed(&global_state->inputs, INPUT_KEY_T));
  if (!scene_data->just_transitioned && should_transition) {
    scene_data->just_transitioned = true;
    global_state->scene_manager.pending_scene = scene_data->other_scene;
    global_state->scene_manager.pending_scene_action = SCENE_ACTION_SET;
  }
  scene_data->just_transitioned = should_transition;
}

// TODO separate player position and camera position
// TODO will I consider the players position within a tilemap? within the whole world?
inline void scene0_update(void *scene_data, void *global_state, f32 dt) {

  GlobalState *gs = (GlobalState *)global_state;
  OverworldSceneData *overworld_sd = (OverworldSceneData *)scene_data;

  const f32 SPEED = 5.0f;
  const f32 ROTATION_SPEED = 0.05f;

  glm::vec3 input_movement_vector = inputs_to_movement_vector(&gs->inputs);
  glm::vec3 movement_vector = SPEED * dt * input_movement_vector;
  move_camera_in_tilemap(movement_vector, overworld_sd, gs);

  glm::vec3 camera_xy(overworld_sd->camera.position.x, overworld_sd->camera.position.y, 0.0f);

  glm::vec3 input_movement_vector_xy = glm::vec3(input_movement_vector.x, input_movement_vector.y, 0.0f);
  // atan2 returns arctan(y/x) in [-pi, +pi]
  f32 input_angle_radians = atan2(input_movement_vector_xy.y, input_movement_vector_xy.x);

  if (glm::length(input_movement_vector_xy) > EPSILON) {
    overworld_sd->player_rotation_simulation = input_angle_radians;
  }

  // FIXME - rotations towards the left seem to be faster. Need to rederive some math for this.
  // Pissed off rn, taking a break and moving onto more meaningful things.
  // Can't cross the +/- PI barrier correctly. Can't go from pointing top left to bottom left,
  // or bottom left to top left. Can only go around the full 360 degrees the other way.
  f32 angle_diff = overworld_sd->player_rotation_simulation - overworld_sd->player_rotation_render;
  f32 angle_update_sign = fabs(angle_diff) > PI ? -1.0f : 1.0f;
  f32 next_angle_raw = overworld_sd->player_rotation_render + ROTATION_SPEED * angle_update_sign * angle_diff;
  f32 next_angle = clamp_f32(next_angle_raw, -PI, PI);
  overworld_sd->player_rotation_render = next_angle;

  glm::mat4 player_model = glm::mat4(1.0f);
  player_model = glm::translate(player_model, camera_xy);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
  player_model = glm::rotate(player_model, overworld_sd->player_rotation_render, glm::vec3(0.0f, 0.0f, 1.0f));

  glBindBuffer(GL_UNIFORM_BUFFER, overworld_sd->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  buffer_vp_matrix_to_gl_ubo(&overworld_sd->camera, overworld_sd->vp_ubo, gs->window_width, gs->window_height);
}

inline void scene0_draw(const void *scene_data) {
  OverworldSceneData *overworld_data = (OverworldSceneData *)scene_data;

  // Draw world into overworld data's FBO
  glBindFramebuffer(GL_FRAMEBUFFER, overworld_data->render_target.fbo);
  glClear(GL_COLOR_BUFFER_BIT);
  draw_opengl_mesh(&overworld_data->tilemap_mesh, overworld_data->tilemap_material);
  draw_opengl_mesh(&overworld_data->player_mesh, overworld_data->player_material);

  // Draw overlay into overworld framebuffer
  glUseProgram(overworld_data->overworld_overlay_program);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Present world to screen
  // Does this rely on hidden assumptions that the screen and these FBOs have the same dimensions?
  // Where do I enforce this?
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, overworld_data->render_target.texture.texture);
  draw_opengl_mesh(&overworld_data->fullscreen_quad_mesh, overworld_data->fullscreen_quad_material);
}
