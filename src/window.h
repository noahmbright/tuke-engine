#pragma once

#include "GLFW/glfw3.h"
#include "linalg.h"
#include "tuke_engine.h"

enum Input {
  INPUT_KEY_UNKNOWN = -1,

  INPUT_KEY_SPACEBAR,

  INPUT_KEY_W,
  INPUT_KEY_A,
  INPUT_KEY_S,
  INPUT_KEY_D,
  INPUT_KEY_H,
  INPUT_KEY_T,

  INPUT_KEY_LEFT_ARROW,
  INPUT_KEY_RIGHT_ARROW,
  INPUT_KEY_UP_ARROW,
  INPUT_KEY_DOWN_ARROW,

  INPUT_KEY_Q,
  INPUT_KEY_ESCAPE,

  INPUT_KEY_LEFT_SHIFT,
  INPUT_KEY_RIGHT_SHIFT,
  INPUT_KEY_ENTER,

  INPUT_KEY_F1,
  INPUT_KEY_F2,
  INPUT_KEY_F3,
  INPUT_KEY_F4,
  INPUT_KEY_F5,
  INPUT_KEY_F6,
  INPUT_KEY_F7,
  INPUT_KEY_F8,
  INPUT_KEY_F9,

  NUM_INPUTS
};

struct Inputs {
  f64 scroll_dx;
  f64 scroll_dy;

  f64 cursor_x;
  f64 cursor_y;

  bool key_inputs_array[2][NUM_INPUTS];
  bool *curr;
  bool *prev;

  bool curr_lclick;
  bool prev_lclick;

  bool curr_rclick;
  bool prev_rclick;
};

GLFWwindow *create_window(bool is_vulkan, const char *title = "Tuke", const int width = 800, const int height = 600);
void update_inputs_glfw(Inputs *inputs, GLFWwindow *window);
void init_inputs(Inputs *inputs);

Vec2 inputs_to_direction_arrow_keys(const Inputs *inputs);
Vec2 inputs_to_direction_wasd(const Inputs *inputs);
Vec2 inputs_to_direction(const Inputs *inputs);

inline bool key_pressed(const Inputs *inputs, Input key) { return inputs->curr[key] && !inputs->prev[key]; }

inline bool lclick_pressed(const Inputs *inputs) { return inputs->curr_lclick && !inputs->prev_lclick; }

inline bool key_released(const Inputs *inputs, Input key) { return !inputs->curr[key] && inputs->prev[key]; }

inline bool key_held(const Inputs *inputs, Input key) { return inputs->curr[key]; }
