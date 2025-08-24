#include "glm/glm.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "window.h"
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

// need to init in place, because inputs contains arrays that would be
// stack allocated. the problem is when we go to point with the prev/cur
// pointers to those stack allocated arrays
void init_inputs(Inputs *inputs) {
  for (u32 i = 0; i < NUM_INPUTS; i++) {
    inputs->key_inputs_array[0][i] = false;
    inputs->key_inputs_array[1][i] = false;
  }

  inputs->key_inputs = inputs->key_inputs_array[0];
  inputs->prev_key_inputs = inputs->key_inputs_array[1];

  inputs->left_mouse_clicked = false;
  inputs->right_mouse_clicked = false;
}

bool key_pressed(const Inputs *inputs, Input key) {
  return inputs->key_inputs[key] && !inputs->prev_key_inputs[key];
}

bool key_released(const Inputs *inputs, Input key) {
  return !inputs->key_inputs[key] && inputs->prev_key_inputs[key];
}

bool key_held(const Inputs *inputs, Input key) {
  return inputs->key_inputs[key];
}

void update_mouse_input_glfw(Inputs *inputs, GLFWwindow *window) {

  inputs->left_mouse_clicked =
      (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

  inputs->right_mouse_clicked =
      (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
}

// update the inputs AND swap the pointers to the current frames inputs
void update_key_inputs_glfw(Inputs *inputs, GLFWwindow *window) {

  const i32 key_to_glfw_key[] = {

      [INPUT_KEY_SPACEBAR] = GLFW_KEY_SPACE,
      [INPUT_KEY_W] = GLFW_KEY_W,
      [INPUT_KEY_A] = GLFW_KEY_A,
      [INPUT_KEY_S] = GLFW_KEY_S,
      [INPUT_KEY_D] = GLFW_KEY_D,
      [INPUT_KEY_H] = GLFW_KEY_H,

      [INPUT_KEY_LEFT_ARROW] = GLFW_KEY_LEFT,
      [INPUT_KEY_RIGHT_ARROW] = GLFW_KEY_RIGHT,
      [INPUT_KEY_UP_ARROW] = GLFW_KEY_UP,
      [INPUT_KEY_DOWN_ARROW] = GLFW_KEY_DOWN,

      [INPUT_LEFT_SHIFT] = GLFW_KEY_LEFT_SHIFT,
      [INPUT_RIGHT_SHIFT] = GLFW_KEY_RIGHT_SHIFT,

      [INPUT_KEY_Q] = GLFW_KEY_Q,
      [INPUT_KEY_ESCAPE] = GLFW_KEY_ESCAPE,
  };

  bool *temp = inputs->key_inputs;
  inputs->key_inputs = inputs->prev_key_inputs;
  inputs->prev_key_inputs = temp;

  for (u32 i = 0; i < NUM_INPUTS; i++) {
    inputs->key_inputs[i] =
        (glfwGetKey(window, key_to_glfw_key[i]) == GLFW_PRESS);
  }
}

glm::vec2 get_cursor_position(GLFWwindow *window) {
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  return {xpos, ypos};
}

void error_callback(int error, const char *description) {
  (void)error;
  fprintf(stderr, "GLFW error callback:\n%s\n", description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  (void)action;
  (void)mods;
  (void)scancode;

  if (key == GLFW_KEY_O && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

GLFWwindow *new_window(bool is_vulkan, const char *title, const int width,
                       const int height) {

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize glfw\n");
    exit(1);
  }

  if (is_vulkan) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  } else { // is opengl
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  }

  GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to initialize window\n");
    glfwTerminate();
    exit(1);
  }

  glfwSetKeyCallback(window, key_callback);

  if (!is_vulkan) {
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
  }

  return window;
}
