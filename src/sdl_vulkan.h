#pragma once

#include "vulkan/vulkan_base.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

static inline VkSurfaceKHR create_sdl_vulkan_surface(VkInstance instance, void *window) {
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface((SDL_Window *)window, instance, NULL, &surface)) {
    fprintf(stderr, "[%s] %s:%d: Failed to create SDL Vulkan surface: %s\n", __func__, __FILE__, __LINE__,
            SDL_GetError());
    fflush(stderr);
    assert(0);
  }
  return surface;
}

static inline VulkanWindowInfo create_sdl_vulkan_window_info(SDL_Window *window) {
  uint32_t extension_count;
  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (extensions == NULL) {
    fprintf(stderr, "%s(): SDL_Vulkan_GetInstanceExtensions returned NULL\n", __func__);
    exit(1);
  }

  int width, height;
  SDL_GetWindowSizeInPixels(window, &width, &height);

  VulkanWindowInfo ret = {
      .extension_count = extension_count,
      .extensions = (const char **)extensions,
      .create_surface = create_sdl_vulkan_surface,
      .window = window,
      .width = (u32)width,
      .height = (u32)height,
  };

  return ret;
}
