#pragma once

#include "window.h"

#include "camera.h"
#include "opengl_base.h"
#include "scene_manager.h"
#include "tilemap.h"
#include "tuke_engine.h"
#include "utils.h"

// TODO
// Transition in and out of scene changes
// Get GLFW and OpenGL explicit API calls out of here and into a platform/renderer layer
// NPCs

#define PLAYER_SIDE_LENGTH_METERS (0.6f)
#define PLAYER_INTERACTION_DISTANCE (1.0f)
#define PLAYER_INTERACTION_FOV (1.04f) // Radians, around 60 degrees
#define NUM_ENTITIES (64)

const f32 OVERWORLD_CAMERA_Z0 = 15.0f;
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

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(vp), &vp);
}

inline glm::vec3 inputs_to_movement_vector(const Inputs *inputs) {
  glm::vec2 movement_vector = inputs_to_direction(inputs);
  f32 zoom_speed = 2.0f;
  return glm::vec3(movement_vector.x, movement_vector.y, zoom_speed * inputs->scroll_dy);
}

enum ShaderID {
  SHADER_ID_NIL = 0,
  SHADER_ID_TILEMAP,
  SHADER_ID_PLAYER,
  SHADER_ID_FULLSCREEN_QUAD,
  SHADER_ID_OVERWORLD_OVERLAY,
  SHADER_ID_GLYPHS,
  SHADER_ID_VISION_CONE,

  NUM_SHADER_IDS
};

enum MeshID {
  MESH_ID_NIL = 0,
  MESH_TILEMAP,
  MESH_TILEMAP1,
  MESH_PLAYER,
  MESH_FULLSCREEN_QUAD,
};

enum MaterialID {
  MATERIAL_ID_NIL = 0,
  MATERIAL_TILEMAP,
  MATERIAL_PLAYER,
  MATERIAL_FULLSCREEN_QUAD,
};

enum EntityType {
  ENTITY_NIL = 0,
  ENTITY_PLAYER,
  ENTITY_NPC,
};

// Trying out a technique where 0 is a sentinel failure value. Index 0 into these arrays is always invalid.
// ZII.
struct EntityIndex {
  u32 idx;
};

struct Entities {
  EntityType types[NUM_ENTITIES];
  bool active[NUM_ENTITIES];
  glm::vec3 positions[NUM_ENTITIES];
  f32 rotations[NUM_ENTITIES];

  // ShaderID shader_id[NUM_ENTITIES];
  // MaterialID material_id[NUM_ENTITIES];
  // MeshID mesh_id[NUM_ENTITIES];

  EntityIndex next_inactive;
};

inline Entities create_entities() {
  Entities entities;
  memset(&entities, 0, sizeof(entities));
  entities.next_inactive.idx = 1;
  return entities;
}

// Return the index of the added entity.
inline EntityIndex entities_add(Entities *entities) {

  u32 next_inactive = entities->next_inactive.idx;
  if (next_inactive < NUM_ENTITIES && !entities->active[next_inactive]) {
    entities->next_inactive.idx++;
    return {
        .idx = next_inactive,
    };
  }

  for (u32 i = 1; i < NUM_ENTITIES; i++) {
    if (entities->active[i] == false) {
      entities->next_inactive.idx = i + 1;
      return {
          .idx = i,
      };
    }
  }

  assert(false && "Out of space for non nil entities");
}

enum SceneID { SCENE0, SCENE1, SCENE_BULLET_HELL, NUM_SCENES, SCENE_NONE };

enum CameraMode {
  CAMERA_MODE_OVERWORLD,
  CAMERA_MODE_DEBUG,
};

struct OverworldSceneData {
  // Is this impure? This is half debug?
  CameraMode camera_mode;
  Entities entities;

  EntityIndex player_index;

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

  GLMesh tilemap_mesh;
  GLMaterial tilemap_material;

  GLRenderTarget render_target;
  GLMesh fullscreen_quad_mesh;
  GLMaterial fullscreen_quad_material;

  u32 vision_cone_ubo;
};

enum GameState {
  GAME_STATE_PAUSED,
  GAME_STATE_RUNNING,
};

struct GlobalState {
  GLFWwindow *window;
  GLRenderer renderer;
  SceneManager scene_manager;
  Inputs inputs;
  f64 t;
  int window_width;
  int window_height;
  GameState game_state;
};

GlobalState create_global_state(u32 window_width, u32 window_height) {
  GlobalState global_state;

  global_state.window = create_window(false /* is vulkan */, "topdown", window_width, window_height);
  glfwGetFramebufferSize(global_state.window, &global_state.window_width, &global_state.window_height);

  global_state.renderer = create_gl_renderer();

  init_inputs(&global_state.inputs);
  memset(&global_state.scene_manager, 0, sizeof(SceneManager));
  global_state.t = 0.0;
  global_state.game_state = GAME_STATE_RUNNING;

  return global_state;
}

void destroy_global_state(GlobalState *global_state) { glfwDestroyWindow(global_state->window); }

// Processing inputs

enum OverworldPlayerIntentFlags {
  TRANSITION_REQUESTED = 1 << 0,
  DEBUG_MODE_CHANGE_REQUESTED = 1 << 1,
};

// Can I C-style namespace this? This should be an internal implementation detail here.
struct OverworldPlayerIntent {
  glm::vec2 key_movement_vector;
  f32 scroll;
  u32 flags;
};

inline OverworldPlayerIntent overworld_process_inputs(const Inputs *inputs) {
  OverworldPlayerIntent player_intent;
  player_intent.key_movement_vector = inputs_to_direction(inputs);
  player_intent.scroll = inputs->scroll_dy;

  player_intent.flags = 0;
  player_intent.flags |= key_pressed(inputs, INPUT_KEY_T) * TRANSITION_REQUESTED;
  player_intent.flags |= key_pressed(inputs, INPUT_KEY_F8) * DEBUG_MODE_CHANGE_REQUESTED;

  return player_intent;
}

static inline void move_player_in_tilemap(
    glm::vec2 movement_vector, const Tilemap *tilemap, glm::vec3 *player_pos, u8 *x_tile, u8 *y_tile
) {

  const glm::vec3 tile_size(PLAYER_SIDE_LENGTH_METERS);
  glm::vec3 final_movement_vector(0.0f);

  // Right now, tile == 0 means move. Otherwise, reset motion
  glm::vec3 x_moved_position = *player_pos + glm::vec3(movement_vector.x, 0.0f, 0.0f);
  *x_tile = tilemap_check_collision(tilemap, x_moved_position, tile_size);
  final_movement_vector.x = (*x_tile == 0) * movement_vector.x;

  glm::vec3 y_moved_position = *player_pos + glm::vec3(0.0f, movement_vector.y, 0.0f);
  *y_tile = tilemap_check_collision(tilemap, y_moved_position, tile_size);
  final_movement_vector.y = (*y_tile == 0) * movement_vector.y;

  *player_pos += final_movement_vector;
}

inline void overworld_update(void *scene_data_void_ptr, void *global_state_void_ptr, f32 dt) {

  GlobalState *global_state = (GlobalState *)global_state_void_ptr;
  OverworldSceneData *scene_data = (OverworldSceneData *)scene_data_void_ptr;

  // Move player.
  // TODO camera_mode is more like control_mode
  OverworldPlayerIntent player_intent = overworld_process_inputs(&global_state->inputs);
  const f32 ROTATION_SPEED = 0.05f;
  const f32 ZOOM_SPEED = 2.0f;

  if (player_intent.flags & DEBUG_MODE_CHANGE_REQUESTED) {
    switch (scene_data->camera_mode) {
    case CAMERA_MODE_OVERWORLD: {
      // Switch into free 3D camera mode
      // TODO add or remove a debug overlay
      scene_data->camera_mode = CAMERA_MODE_DEBUG;
      glfwSetInputMode(global_state->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      break;
    }
    case CAMERA_MODE_DEBUG: {
      // Switch back to overworld mode.
      glfwSetInputMode(global_state->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      scene_data->camera_mode = CAMERA_MODE_OVERWORLD;
      scene_data->camera.position.z = OVERWORLD_CAMERA_Z0;
      scene_data->camera.direction = CAMERA_DIRECTION0;
      scene_data->camera.up = CAMERA_UP0;
      scene_data->camera.right = CAMERA_RIGHT0;
      scene_data->camera.has_moused_yet = false;
      break;
    }
    default:
      assert(false);
    }
  }

  Camera *camera = &scene_data->camera;
  switch (scene_data->camera_mode) {
    // In overworld mode, the camera follows the player. On a reset back to overworld mode from
    // debug mode, the camera should jump back to the player's position.
  case CAMERA_MODE_OVERWORLD: {
    // Move player.
    const f32 SPEED = 5.0f;
    glm::vec3 *player_pos = &scene_data->entities.positions[scene_data->player_index.idx];
    glm::vec2 movement_vector = SPEED * dt * player_intent.key_movement_vector;
    u8 x_tile, y_tile;
    move_player_in_tilemap(movement_vector, scene_data->tilemap, player_pos, &x_tile, &y_tile);

    // Move camera.
    camera->position.x = player_pos->x;
    camera->position.y = player_pos->y;

    f32 next_camera_z = camera->position.z + ZOOM_SPEED * player_intent.scroll;
    camera->position.z = clamp_f32(next_camera_z, 1.0f, CAMERA_FAR_Z - EPSILON);

    // TODO feels a little impure to have a key driven transition request taking place here.
    // TODO Define magic numbers in the tilemap
    // 2 is transition scene block
    bool should_transition = (x_tile == 2 || y_tile == 2 || player_intent.flags & TRANSITION_REQUESTED);
    if (!scene_data->just_transitioned && should_transition) {
      scene_data->just_transitioned = true;
      global_state->scene_manager.pending_scene = scene_data->other_scene;
      global_state->scene_manager.pending_scene_action = SCENE_ACTION_SET;
    }
    scene_data->just_transitioned = should_transition;

    glm::vec3 player_xy(player_pos->x, player_pos->y, 0.0f);

    f32 input_x = player_intent.key_movement_vector.x;
    f32 input_y = player_intent.key_movement_vector.y;
    f32 input_angle_radians = atan2(input_y, input_x); // arctan(y/x) in [-pi, +pi]

    f32 input_xy_norm2 = input_x * input_x + input_y * input_y;
    if (input_xy_norm2 > EPSILON) {
      scene_data->player_rotation_simulation = input_angle_radians;
    }

    // Set view cone for interacting with world.
    // The view cone is a single triangle. Find the bounding box for the triangle, and then
    // intersect that with the tilemap. If an interactable object is in the BB, then check its
    // barycentric coords to confirm collision with the triangle.
    // TODO not doing any triangle stuff yet
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
    break;
  }
    // End monolithic swtich case
  case CAMERA_MODE_DEBUG: {
    camera_move_3d(camera, player_intent.key_movement_vector);

    f64 xpos, ypos;
    glfwGetCursorPos(global_state->window, &xpos, &ypos);
    process_mouse_input3d(camera, xpos, ypos);
    break;
  }
  default:
    assert(false);
  }

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

inline void overworld_draw(const GLRenderer *renderer, const void *scene_data_void_ptr) {
  OverworldSceneData *scene_data = (OverworldSceneData *)scene_data_void_ptr;

  glBindFramebuffer(GL_FRAMEBUFFER, scene_data->render_target.fbo);
  glClear(GL_COLOR_BUFFER_BIT);

  glm::mat4 player_model = glm::mat4(1.0f);
  player_model = glm::translate(player_model, scene_data->entities.positions[scene_data->player_index.idx]);
  player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
  player_model = glm::rotate(player_model, scene_data->player_rotation_render, glm::vec3(0.0f, 0.0f, 1.0f));

  glBindBuffer(GL_UNIFORM_BUFFER, scene_data->player_model_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

  // Draw world and player into overworld data's FBO
  draw_gl_mesh(&scene_data->tilemap_mesh, scene_data->tilemap_material);
  draw_gl_mesh(&renderer->meshes[MESH_PLAYER], renderer->materials[MATERIAL_PLAYER]);

  // Draw player vision cone
  // Is this more for debug?
  // NB: blendFunc is required for blending. Just Enabling doesn't get transparency.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(renderer->programs[SHADER_ID_VISION_CONE]);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Draw overlay into overworld framebuffer
  glDisable(GL_BLEND);
  glUseProgram(renderer->programs[SHADER_ID_OVERWORLD_OVERLAY]);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Present world to screen
  // Does this rely on hidden assumptions that the screen and these FBOs have the same dimensions?
  // Where do I enforce this?
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, scene_data->render_target.texture.texture);
  draw_gl_mesh(&scene_data->fullscreen_quad_mesh, scene_data->fullscreen_quad_material);
}
