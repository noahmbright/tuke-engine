#pragma once

#include "opengl_base.h"
#include "tuke_engine.h"

#define MAX_SCENES 32
#define SCENE_STACK_SIZE 4

enum SceneAction { SCENE_ACTION_NONE, SCENE_ACTION_SET, SCENE_ACTION_PUSH, SCENE_ACTION_POP };

// UpdateFunction's take a pointer to scene specific data and global state per application.
typedef void (*UpdateFunction)(void *, void *, f32);

// RenderFunction's take a pointer to a renderer and scene specific data.
// FIXME will eventually want to make the GLRenderer here something more generic.
typedef void (*RenderFunction)(const GLRenderer *, const void *);

struct Scene {
  UpdateFunction update;
  RenderFunction render;
  void *data;
};

inline Scene create_scene(UpdateFunction update, RenderFunction render, void *data) {
  return Scene{.update = update, .render = render, .data = data};
}

struct SceneManager {
  Scene *scene_registry[MAX_SCENES];
  Scene *stack[SCENE_STACK_SIZE];

  // top points to the top valid scene, i.e., the current scene. Not one past the end.
  u32 top;

  // Use a u32 here to index into the scene registry.
  // Pending scene should be an enum handle into a scene in the registry.
  u32 pending_scene;
  SceneAction pending_scene_action;
};

inline Scene *get_current_scene(const SceneManager *scene_manager) { return scene_manager->stack[scene_manager->top]; }

inline void push_scene(SceneManager *scene_manager, Scene *scene_data) {
  assert(scene_manager->top + 1 < MAX_SCENES);
  scene_manager->stack[++scene_manager->top] = scene_data;
}

inline void pop_scene(SceneManager *scene_manager) {
  assert(scene_manager->top > 0);
  scene_manager->top--;
}

inline void handle_scene_action(SceneManager *scene_manager) {
  switch (scene_manager->pending_scene_action) {

  case SCENE_ACTION_NONE:
    return;

  case SCENE_ACTION_SET: {
    Scene *next_scene = scene_manager->scene_registry[scene_manager->pending_scene];
    scene_manager->stack[scene_manager->top] = next_scene;
    // TODO exit current scene, enter next scene
    break;
  }

  case SCENE_ACTION_PUSH: {
    Scene *next_scene = scene_manager->scene_registry[scene_manager->pending_scene];
    push_scene(scene_manager, next_scene);
    break;
  }

  case SCENE_ACTION_POP:
    pop_scene(scene_manager);
    break;
  }

  scene_manager->pending_scene_action = SCENE_ACTION_NONE;
}

inline void set_base_scene(SceneManager *scene_manager, Scene *scene) {
  scene_manager->top = 0;
  scene_manager->stack[0] = scene;
}

inline void request_set_scene(SceneManager *scene_manager, u32 scene_enum) {
  scene_manager->pending_scene = scene_enum;
  scene_manager->pending_scene_action = SCENE_ACTION_SET;
}
