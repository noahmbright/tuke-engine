#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "vulkan_base.h"

// https://www.glfw.org/docs/latest/group__vulkan.html#ga9308f2acf6b5f6ff49cf0d4aa9ba1fab
// https://www.glfw.org/docs/latest/vulkan_guide.html#vulkan_surface
// https://www.glfw.org/docs/latest/vulkan_guide.html#vulkan_ext

static inline VkSurfaceKHR create_glfw_vulkan_surface(VkInstance instance, void *window) {
  VkSurfaceKHR surface;
  VK_CHECK(glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, &surface),
           "create_surface: Failed to createWindowSurface\n");
  return surface;
}

static inline VulkanWindowInfo create_glfw_vulkan_window_info(GLFWwindow *window) {
  uint32_t extension_count;
  const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  if (extensions == NULL) {
    fprintf(stderr, "create_instance: glfw_extensions is NULL\n");
    exit(1);
  }

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  return (VulkanWindowInfo){
      .extension_count = extension_count,
      .extensions = extensions,
      .create_surface = create_glfw_vulkan_surface,
      .window = window,
      .width = width,
      .height = height,
  };
}
