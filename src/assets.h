#pragma once

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "tuke_engine.h"
#include "utils.h"
#include "vulkan_base.h"

#define MAX_NUM_TEXTURES (64)
#define MAX_LAYERS (8)

typedef struct {
  const char *paths[MAX_LAYERS];
  u32 num_paths;
} StringArray;

#define TEX(path) {.paths = {path}, .num_paths = 1}

static inline void
load_vulkan_textures(VulkanContext *ctx, const StringArray *string_arrs, u32 num_textures, VulkanTexture *textures) {
  if (textures == NULL) {
    return;
  }
  assert(num_textures < MAX_NUM_TEXTURES);
  STBImage stbs[MAX_NUM_TEXTURES][MAX_LAYERS];

  // Get max size
  u32 max_dim = 0;
  for (u32 i = 0; i < num_textures; i++) {
    for (u32 j = 0; j < string_arrs[i].num_paths; j++) {
      assert(string_arrs[i].num_paths < MAX_LAYERS);
      const char *path = string_arrs[i].paths[j];
      stbs[i][j] = load_texture(path, false /*flip_vertically*/);
      u32 dim = (stbs[i][j].width > stbs[i][j].height) ? stbs[i][j].width : stbs[i][j].height;
      u32 aligned_dim = next_pow2(dim);
      max_dim = (aligned_dim > max_dim) ? aligned_dim : max_dim;
      printf("Loaded %u x %u image %s\n", stbs[i][j].width, stbs[i][j].height, path);
    }
  }

  // Allocate staging buffer
  // Slightly unhappy about having this raw Vulkan stuff outside of my wrapper, but okay.
  u32 size = max_dim * max_dim * 4;
  printf("Allocating %u x %u staging buffer\n", max_dim, max_dim);
  VulkanBuffer buffer = create_buffer(ctx, BUFFER_TYPE_STAGING, size);
  void *texture_data;
  VkResult result = vkMapMemory(ctx->device, buffer.memory, 0, buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  unsigned char *resized = (unsigned char *)malloc(size);
  // Create textures and free STB data
  for (u32 i = 0; i < num_textures; i++) {
    textures[i] = create_vulkan_texture(ctx, max_dim, max_dim, string_arrs[i].num_paths);

    // First transition
    VkCommandBuffer cmd = begin_single_use_command_buffer(ctx);
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transition_image_layout(cmd, textures[i].image, old_layout, new_layout);
    end_single_use_command_buffer(ctx, cmd);

    for (u32 j = 0; j < string_arrs[i].num_paths; j++) {
      u32 w = stbs[i][j].width;
      u32 h = stbs[i][j].height;
      unsigned char *data = stbs[i][j].data;
      bool needs_resized = (w != max_dim) || (h != max_dim);
      if (needs_resized) {
        stbir_resize_uint8_srgb(data, w, h, 0, resized, max_dim, max_dim, 0, STBIR_RGBA);
        data = resized;
      }
      copy_data_to_texture(ctx, &buffer, j, data, texture_data, &textures[i]);
      free_stb_image(&stbs[i][j]);
    }

    // Second transition
    cmd = begin_single_use_command_buffer(ctx);
    old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transition_image_layout(cmd, textures[i].image, old_layout, new_layout);
    end_single_use_command_buffer(ctx, cmd);
  }

  free(resized);
  vkUnmapMemory(ctx->device, buffer.memory);
  destroy_vulkan_buffer(ctx, buffer);
}
