#pragma once

#include "GLFW/glfw3.h"
#include "glm/vec4.hpp"
#include "tuke_engine.h"

// TODO does input belong in the windowing file?
enum Key {
  KEY_UNKNOWN = -1,

  KEY_SPACEBAR,

  KEY_W,
  KEY_A,
  KEY_S,
  KEY_D,

  KEY_LEFT_ARROW,
  KEY_RIGHT_ARROW,
  KEY_UP_ARROW,
  KEY_DOWN_ARROW,

  KEY_Q,
  KEY_ESCAPE,

  NUM_KEYS
};

struct Inputs {
  bool key_inputs_array[2][NUM_KEYS];
  bool *key_inputs;
  bool *prev_key_inputs;
};

GLFWwindow *new_window(bool is_vulkan = false, const char *title = "Tuke",
                       const int width = 800, const int height = 600);
glm::vec4 get_window_movement_vector(GLFWwindow *window);
void update_key_inputs_glfw(Inputs *inputs, GLFWwindow *window);
void init_inputs(Inputs *inputs);
bool key_pressed(Inputs *inputs, Key key);
