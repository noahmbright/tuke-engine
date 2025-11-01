#pragma once

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "tuke_engine.h"

// TODO does input belong in the windowing file?
enum Input {
  INPUT_KEY_UNKNOWN = -1,

  INPUT_KEY_SPACEBAR,

  INPUT_KEY_W,
  INPUT_KEY_A,
  INPUT_KEY_S,
  INPUT_KEY_D,
  INPUT_KEY_H,

  INPUT_KEY_LEFT_ARROW,
  INPUT_KEY_RIGHT_ARROW,
  INPUT_KEY_UP_ARROW,
  INPUT_KEY_DOWN_ARROW,

  INPUT_KEY_Q,
  INPUT_KEY_ESCAPE,

  INPUT_LEFT_SHIFT,
  INPUT_RIGHT_SHIFT,

  NUM_INPUTS
};

struct Inputs {
  f64 scroll_dx;
  f64 scroll_dy;
  bool key_inputs_array[2][NUM_INPUTS];
  bool *key_inputs;
  bool *prev_key_inputs;

  bool left_mouse_clicked;
  bool right_mouse_clicked;
};

// scroll callback for glfw
struct ScrollDeltas {
  f64 dx, dy;
};

GLFWwindow *new_window(bool is_vulkan, const char *title = "Tuke", const int width = 800, const int height = 600);
void update_key_inputs_glfw(Inputs *inputs, GLFWwindow *window);
void init_inputs(Inputs *inputs);

inline bool key_pressed(const Inputs *inputs, Input key) {
  return inputs->key_inputs[key] && !inputs->prev_key_inputs[key];
}

inline bool key_released(const Inputs *inputs, Input key) {
  return !inputs->key_inputs[key] && inputs->prev_key_inputs[key];
}

inline bool key_held(const Inputs *inputs, Input key) { return inputs->key_inputs[key]; }

inline glm::vec3 inputs_to_direction(const Inputs *inputs) {
  glm::vec3 direction(0.0f);

  if (key_held(inputs, INPUT_KEY_W) || key_held(inputs, INPUT_KEY_UP_ARROW)) {
    direction.y += 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_A) || key_held(inputs, INPUT_KEY_LEFT_ARROW)) {
    direction.x -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_S) || key_held(inputs, INPUT_KEY_DOWN_ARROW)) {
    direction.y -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_D) || key_held(inputs, INPUT_KEY_RIGHT_ARROW)) {
    direction.x += 1.0f;
  }

  return direction;
}

inline glm::vec3 inputs_to_direction_wasd(const Inputs *inputs) {
  glm::vec3 direction(0.0f);

  if (key_held(inputs, INPUT_KEY_W)) {
    direction.y += 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_A)) {
    direction.x -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_S)) {
    direction.y -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_D)) {
    direction.x += 1.0f;
  }

  return direction;
}

inline glm::vec3 inputs_to_direction_arrow_keys(const Inputs *inputs) {
  glm::vec3 direction(0.0f);

  if (key_held(inputs, INPUT_KEY_UP_ARROW)) {
    direction.y += 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_LEFT_ARROW)) {
    direction.x -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_DOWN_ARROW)) {
    direction.y -= 1.0f;
  }
  if (key_held(inputs, INPUT_KEY_RIGHT_ARROW)) {
    direction.x += 1.0f;
  }

  return direction;
}
