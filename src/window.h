#pragma once

#include "GLFW/glfw3.h"
#include "glm/vec4.hpp"
#include "tuke_engine.h"

#define NUM_KEYS (GLFW_KEY_LAST + 1)

// TODO per frame pointer swapping current/prev frame handling
// how many key inputs do I need? GLFW_KEY_LAST is 348. what about other apis?
struct Inputs {
  bool key_inputs[NUM_KEYS][2];
};

GLFWwindow *new_window(bool is_vulkan = false, const char *title = "Tuke",
                       const int width = 800, const int height = 600);
glm::vec4 get_window_movement_vector(GLFWwindow *window);
