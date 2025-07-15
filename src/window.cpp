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
  for (u32 i = 0; i < NUM_KEYS; i++) {
    inputs->key_inputs_array[0][i] = false;
    inputs->key_inputs_array[1][i] = false;
  }

  inputs->key_inputs = inputs->key_inputs_array[0];
  inputs->prev_key_inputs = inputs->key_inputs_array[1];
}

bool key_pressed(Inputs *inputs, Key key) {
  return inputs->key_inputs[key] && !inputs->prev_key_inputs[key];
}

// update the inputs AND swap the pointers to the current frames inputs
void update_key_inputs_glfw(Inputs *inputs, GLFWwindow *window) {

  const i32 key_to_glfw_key[] = {

      [KEY_SPACEBAR] = GLFW_KEY_SPACE,
      [KEY_W] = GLFW_KEY_W,
      [KEY_A] = GLFW_KEY_A,
      [KEY_S] = GLFW_KEY_S,
      [KEY_D] = GLFW_KEY_D,

      [KEY_LEFT_ARROW] = GLFW_KEY_LEFT,
      [KEY_RIGHT_ARROW] = GLFW_KEY_RIGHT,
      [KEY_UP_ARROW] = GLFW_KEY_UP,
      [KEY_DOWN_ARROW] = GLFW_KEY_DOWN,

      [KEY_Q] = GLFW_KEY_Q,
      [KEY_ESCAPE] = GLFW_KEY_ESCAPE,
  };

  bool *temp = inputs->key_inputs;
  inputs->key_inputs = inputs->prev_key_inputs;
  inputs->prev_key_inputs = temp;

  for (u32 i = 0; i < NUM_KEYS; i++) {
    inputs->key_inputs[i] =
        (glfwGetKey(window, key_to_glfw_key[i]) == GLFW_PRESS);
  }
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

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
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

glm::vec4 get_window_movement_vector(GLFWwindow *window) {

  glm::vec4 res{};

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    res.y += 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    res.x -= 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    res.y -= 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    res.x += 1.0f;
  }

  if (glm::length(res))
    res = glm::normalize(res);

  float sprint_boost =
      1.0f + float(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

  res.w = sprint_boost;
  return res;
}

glm::vec2 get_cursor_position(GLFWwindow *window) {
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  return {xpos, ypos};
}

// TODO array of pressed keys? press and release?
bool is_left_clicking(GLFWwindow *window) {
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  return (state == GLFW_PRESS);
}

bool is_right_clicking(GLFWwindow *window) {
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  return (state == GLFW_PRESS);
}
