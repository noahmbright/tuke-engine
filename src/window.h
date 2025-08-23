#pragma once

#include "GLFW/glfw3.h"
#include "glm/vec4.hpp"
#include "tuke_engine.h"

// TODO does input belong in the windowing file?
enum Input {
  INPUT_KEY_UNKNOWN = -1,

  INPUT_KEY_SPACEBAR,

  INPUT_KEY_W,
  INPUT_KEY_A,
  INPUT_KEY_S,
  INPUT_KEY_D,

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
  bool key_inputs_array[2][NUM_INPUTS];
  bool *key_inputs;
  bool *prev_key_inputs;

  bool left_mouse_clicked;
  bool right_mouse_clicked;
};

GLFWwindow *new_window(bool is_vulkan = false, const char *title = "Tuke",
                       const int width = 800, const int height = 600);
void update_key_inputs_glfw(Inputs *inputs, GLFWwindow *window);
void init_inputs(Inputs *inputs);
bool key_pressed(const Inputs *inputs, Input key);
bool key_released(const Inputs *inputs, Input key);
bool key_held(const Inputs *inputs, Input key);
