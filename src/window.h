#pragma once

#include "GLFW/glfw3.h"
GLFWwindow *new_window(bool is_vulkan = false, const char *title = "Tuke",
                       const int width = 800, const int height = 600);
