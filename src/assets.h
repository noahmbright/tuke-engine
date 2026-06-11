#pragma once

#include "stb_image.h"
#include "tuke_engine.h"
#include "utils.h"
#include "vulkan_base.h"

#define MAX_NUM_TEXTURES (64)

static inline void
load_vulkan_textures(VulkanContext *ctx, const char **paths, u32 num_textures, VulkanTexture *textures) {
  assert(num_textures < MAX_NUM_TEXTURES);
  STBHandle stbs[MAX_NUM_TEXTURES];

  // Get max size
  u32 max_size = 0;
  for (u32 i = 0; i < num_textures; i++) {
    stbs[i] = load_texture(paths[i]);
    u32 size = stbs[i].width * stbs[i].height * stbs[i].n_channels;
    max_size = (size > max_size) ? size : max_size;
  }

  // Allocate staging buffer
  // Slightly unhappy about having this raw Vulkan stuff outside of my wrapper, but okay.
  VulkanBuffer buffer = create_buffer(ctx, BUFFER_TYPE_STAGING, max_size);
  void *texture_data;
  VkResult result = vkMapMemory(ctx->device, buffer.memory, 0, buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  // Create textures and free STB data
  for (u32 i = 0; i < num_textures; i++) {
    textures[i] = create_vulkan_texture(
        ctx, stbs[i].width, stbs[i].height, stbs[i].n_channels, stbs[i].data, buffer, texture_data
    );
    free_stb_handle(&stbs[i]);
  }

  vkUnmapMemory(ctx->device, buffer.memory);
  destroy_vulkan_buffer(ctx, buffer);
}
