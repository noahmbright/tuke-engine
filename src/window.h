#pragma once

#include "GLFW/glfw3.h"
#include "glm/vec4.hpp"

GLFWwindow *new_window(bool is_vulkan = false, const char *title = "Tuke",
                       const int width = 800, const int height = 600);
glm::vec4 get_window_movement_vector(GLFWwindow *window);
