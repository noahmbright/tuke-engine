#pragma once

#include "window.h"
#define GLM_ENABLE_EXPERIMENTAL

#include "camera.h"
#include "generated_shader_utils.h"
#include "glm/ext.hpp"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/geometric.hpp"
#include "opengl_base.h"
#include "scene_manager.h"
#include "tilemap.h"
#include "tuke_engine.h"
#include "utils.h"

// TODO
// Transition in and out of scene changes

#define PLAYER_SIDE_LENGTH_METERS (0.6f)
#define PLAYER_INTERACTION_DISTANCE (1.0f)
#define PLAYER_INTERACTION_FOV (1.04f) // Radians, around 60 degrees

const f32 PLAYER_INTERACTION_HALF_FOV = PLAYER_INTERACTION_FOV / 2.0f;

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

inline Tilemap tilemap0 = create_tilemap(width_in_tiles, height_in_tiles, &tilemap_data[0]);


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

inline void buffer_vp_matrix_to_gl_ubo(const CameraMatrices *camera_matrices, u32 ubo) {
  glm::mat4 vp = camera_matrices->projection * camera_matrices->view;

#ifndef NDEBUG
  if (matrix_has_nan(camera_matrices->view)) {
    exit(1);
  }
#endif

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(vp), &vp);
}

inline glm::vec3 inputs_to_movement_vector(const Inputs *inputs) {
  glm::vec3 movement_vector = inputs_to_direction(inputs);

  f32 zoom_speed = 2.0f;
  movement_vector.z = zoom_speed * inputs->scroll_dy;

  return movement_vector;
}

struct OverworldPlayer {
  glm::vec3 position;
};

enum SceneID { SCENE0, SCENE1, SCENE_BULLET_HELL, NUM_SCENES, SCENE_NONE };

struct OverworldSceneData {
  // Actual state specific to the overworld
  OverworldPlayer player;

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

  u32 vision_cone_program;
  u32 vision_cone_ubo;
};

enum GameState {
  GAME_STATE_PAUSED,
  GAME_STATE_RUNNING,
};

struct GlobalState {
  SceneManager scene_manager;
  Inputs inputs;
  f64 t;
  int window_width;
  int window_height;
  GameState game_state;
};

// TODO process input in a separate function and process player intent here.
// TODO will I consider the players position within a tilemap? within the whole world?
inline void scene0_update(void *scene_data_void_ptr, void *global_state_void_ptr, f32 dt) {

  GlobalState *global_state = (GlobalState *)global_state_void_ptr;
  OverworldSceneData *scene_data = (OverworldSceneData *)scene_data_void_ptr;

  const f32 SPEED = 5.0f;
  const f32 ROTATION_SPEED = 0.05f;

  // Move player.
  // This is a lot of code, could be nice to extract into functions, but hard to separate out
  // collision detection, player movement, and input cleanly - will continue to think.
  glm::vec3 input_movement_vector = inputs_to_movement_vector(&global_state->inputs);
  glm::vec3 movement_vector = SPEED * dt * input_movement_vector;

  const glm::vec3 tile_size(PLAYER_SIDE_LENGTH_METERS);
  OverworldPlayer *player = &scene_data->player;
  glm::vec3 last_player_pos = player->position;
  glm::vec3 final_movement_vector(0.0f);

  // Right now, tile == 0 means move. Otherwise, reset motion
  glm::vec3 x_moved_position = last_player_pos + glm::vec3(movement_vector.x, 0.0f, 0.0f);
  u8 x_tile = tilemap_check_collision(scene_data->tilemap, x_moved_position, tile_size);
  final_movement_vector.x = (x_tile == 0) * movement_vector.x;

  glm::vec3 y_moved_position = last_player_pos + glm::vec3(0.0f, movement_vector.y, 0.0f);
  u8 y_tile = tilemap_check_collision(scene_data->tilemap, y_moved_position, tile_size);
  final_movement_vector.y = (y_tile == 0) * movement_vector.y;

  player->position += final_movement_vector;

  // Move camera.
  // TODO NEXT camera and player movement are decoupled. Add debug camera moving in 3D next.
  Camera *camera = &scene_data->camera;
  camera->position.x = player->position.x;
  camera->position.y = player->position.y;
  f32 next_camera_z = camera->position.z + input_movement_vector.z;
  camera->position.z = clamp_f32(next_camera_z, 1.0f, CAMERA_PERSPECTIVE_PROJECTION_FAR_Z - EPSILON);

  // TODO Define magic numbers in the tilemap
  // 2 is transition scene block
  // FIXME the camera movement should not worry about scene transitions
  bool should_transition = (x_tile == 2 || y_tile == 2 || key_pressed(&global_state->inputs, INPUT_KEY_T));
  if (!scene_data->just_transitioned && should_transition) {
    scene_data->just_transitioned = true;
    global_state->scene_manager.pending_scene = scene_data->other_scene;
    global_state->scene_manager.pending_scene_action = SCENE_ACTION_SET;
  }
  scene_data->just_transitioned = should_transition;

  glm::vec3 player_xy(player->position.x, player->position.y, 0.0f);

  f32 input_x = input_movement_vector.x;
  f32 input_y = input_movement_vector.y;
  f32 input_angle_radians = atan2(input_y, input_x); // arctan(y/x) in [-pi, +pi]

  f32 input_xy_norm2 = input_x * input_x + input_y * input_y;
  if (input_xy_norm2 > EPSILON) {
    scene_data->player_rotation_simulation = input_angle_radians;
  }

  // Set view cone for interacting with world.
  // The view cone is a single triangle. Find the bounding box for the triangle, and then
  // intersect that with the tilemap. If an interactable object is in the BB, then check its
  // barycentric coords to confirm collision with the triangle.
  f32 theta_b = scene_data->player_rotation_simulation + PLAYER_INTERACTION_HALF_FOV;
  f32 theta_c = scene_data->player_rotation_simulation - PLAYER_INTERACTION_HALF_FOV;
  glm::vec3 cone_b = PLAYER_INTERACTION_DISTANCE * glm::vec3(cosf(theta_b), sinf(theta_b), 0.0f);
  glm::vec3 cone_c = PLAYER_INTERACTION_DISTANCE * glm::vec3(cosf(theta_c), sinf(theta_c), 0.0f);
  VisionCone vision_cone{
      .a = player_xy,
      .b = player_xy + cone_b,
      .c = player_xy + cone_c,
  };

  f32 cone_x_min = fmin(vision_cone.a.x, fmin(vision_cone.b.x, vision_cone.c.x));
  f32 cone_x_max = fmax(vision_cone.a.x, fmax(vision_cone.b.x, vision_cone.c.x));
  f32 cone_y_min = fmin(vision_cone.a.y, fmin(vision_cone.b.y, vision_cone.c.y));
  f32 cone_y_max = fmax(vision_cone.a.y, fmax(vision_cone.b.y, vision_cone.c.y));
  f32 cone_bb_center_x = 0.5f * (cone_x_max + cone_x_min);
  f32 cone_bb_center_y = 0.5f * (cone_y_max + cone_y_min);
  glm::vec3 cone_bb_center = glm::vec3(cone_bb_center_x, cone_bb_center_y, 0.0f);
  glm::vec3 cone_bb_size = glm::vec3(cone_x_max - cone_x_min, cone_y_max - cone_y_min, 0.0f);

  int cone_collision = tilemap_check_collision(scene_data->tilemap, cone_bb_center, cone_bb_size);
  bool is_interacting = (cone_collision == 3);

  // FIXME shitty state machine
  if (is_interacting) {
    if (key_pressed(&global_state->inputs, INPUT_KEY_ENTER)) {
      printf("Interacted\n");
    }
  }

  glBindBuffer(GL_UNIFORM_BUFFER, scene_data->vision_cone_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VisionCone), &vision_cone);

  // FIXME - rotations towards the left seem to be faster. Need to rederive some math for this.
  // Pissed off rn, taking a break and moving onto more meaningful things.
  // Can't cross the +/- PI barrier correctly. Can't go from pointing top left to bottom left,
  // or bottom left to top left. Can only go around the full 360 degrees the other way.
  f32 angle_diff = scene_data->player_rotation_simulation - scene_data->player_rotation_render;
  f32 angle_update_sign = fabs(angle_diff) > PI ? -1.0f : 1.0f;
  f32 next_angle_raw = scene_data->player_rotation_render + ROTATION_SPEED * angle_update_sign * angle_diff;
  f32 next_angle = clamp_f32(next_angle_raw, -PI, PI);
  scene_data->player_rotation_render = next_angle;

  CameraMatrices camera_matrices =
      create_camera_matrices(&scene_data->camera, global_state->window_width, global_state->window_height);
  buffer_vp_matrix_to_gl_ubo(&camera_matrices, scene_data->vp_ubo);
}

inline void scene0_draw(const void *scene_data_void_ptr) {
  OverworldSceneData *scene_data = (OverworldSceneData *)scene_data_void_ptr;

  glm::mat4 player_model = glm::mat4(1.0f);
  player_model = glm::translate(player_model, scene_data->player.position);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
  player_model = glm::rotate(player_model, scene_data->player_rotation_render, glm::vec3(0.0f, 0.0f, 1.0f));

  glBindBuffer(GL_UNIFORM_BUFFER, scene_data->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  // Draw world and player into overworld data's FBO
  glBindFramebuffer(GL_FRAMEBUFFER, scene_data->render_target.fbo);
  glClear(GL_COLOR_BUFFER_BIT);
  draw_opengl_mesh(&scene_data->tilemap_mesh, scene_data->tilemap_material);
  draw_opengl_mesh(&scene_data->player_mesh, scene_data->player_material);

  // Draw player vision cone
  // Is this more for debug?
  // NB: blendFunc is required for blending. Just Enabling doesn't get transparency.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(scene_data->vision_cone_program);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Draw overlay into overworld framebuffer
  glDisable(GL_BLEND);
  glUseProgram(scene_data->overworld_overlay_program);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Present world to screen
  // Does this rely on hidden assumptions that the screen and these FBOs have the same dimensions?
  // Where do I enforce this?
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, scene_data->render_target.texture.texture);
  draw_opengl_mesh(&scene_data->fullscreen_quad_mesh, scene_data->fullscreen_quad_material);
}
