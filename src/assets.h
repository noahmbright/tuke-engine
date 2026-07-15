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

typedef struct {
  u32 base;
  u32 count;
} TextureRange;

#define _NARGS(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define NARGS(...) _NARGS(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
#define TEX(...) {.paths = {__VA_ARGS__}, .num_paths = NARGS(__VA_ARGS__)}

static inline void load_vulkan_texture_array(
    VulkanContext *ctx, VkDescriptorSet set, u32 binding, const char **paths, u32 num_paths, VulkanTexture *textures
) {
  if (paths == NULL || ctx == NULL) {
    return;
  }
  STBImage stbs[MAX_NUM_TEXTURES];

  u32 max_size = 0;
  for (u32 i = 0; i < num_paths; i++) {
    stbs[i] = load_texture(paths[i], false /*flip_vertically*/);
    u32 size = stbs[i].width * stbs[i].height * stbs[i].n_channels;
    max_size = size > max_size ? size : max_size;
  }

  VulkanBuffer buffer = create_buffer(ctx, BUFFER_TYPE_STAGING, max_size);
  void *texture_data;
  VkResult result = vkMapMemory(ctx->device, buffer.memory, 0, buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  for (u32 i = 0; i < num_paths; i++) {
    textures[i] = create_vulkan_texture(ctx, stbs[i].width, stbs[i].height, /*layer_count=*/1);

    // First transition
    VkCommandBuffer cmd = begin_single_use_command_buffer(ctx);
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transition_image_layout(cmd, textures[i].image, old_layout, new_layout);
    end_single_use_command_buffer(ctx, cmd);

    copy_data_to_texture(ctx, &buffer, /*layer_idx=*/0, stbs[i].data, texture_data, &textures[i]);
    free_stb_image(&stbs[i]);

    // Second transition
    cmd = begin_single_use_command_buffer(ctx);
    old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transition_image_layout(cmd, textures[i].image, old_layout, new_layout);
    end_single_use_command_buffer(ctx, cmd);
  }

  vkUnmapMemory(ctx->device, buffer.memory);
  destroy_vulkan_buffer(ctx, buffer);

  VkWriteDescriptorSet writes[MAX_NUM_TEXTURES] = {};
  VkDescriptorImageInfo image_infos[MAX_NUM_TEXTURES] = {};

  for (u32 i = 0; i < num_paths; i++) {
    image_infos[i] = {
        .sampler = ctx->samplers[SAMPLER_LINEAR_CLAMP], // TODO not modular
        .imageView = textures[i].image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstArrayElement = i;
    writes[i].pImageInfo = &image_infos[i];
    writes[i].dstBinding = binding;
    writes[i].dstSet = set;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  }

  vkUpdateDescriptorSets(ctx->device, num_paths, writes, 0, NULL);
}

static inline void
load_vulkan_textures(VulkanContext *ctx, const StringArray *string_arrs, u32 num_textures, VulkanTexture *textures) {
  if (textures == NULL) {
    return;
  }
  assert(num_textures < MAX_NUM_TEXTURES);
  STBImage stbs[MAX_NUM_TEXTURES][MAX_LAYERS];
  u32 max_sizes[MAX_NUM_TEXTURES] = {};

  // Get max sizes per texture array
  u32 max_dim = 0;
  for (u32 i = 0; i < num_textures; i++) {
    for (u32 j = 0; j < string_arrs[i].num_paths; j++) {
      assert(string_arrs[i].num_paths < MAX_LAYERS);
      const char *path = string_arrs[i].paths[j];
      stbs[i][j] = load_texture(path, false /*flip_vertically*/);

      u32 dim = (stbs[i][j].width > stbs[i][j].height) ? stbs[i][j].width : stbs[i][j].height;
      u32 aligned_dim = next_pow2(dim);
      max_sizes[i] = (aligned_dim > max_sizes[i]) ? aligned_dim : max_sizes[i];
      printf("Loaded %u x %u image %s\n", stbs[i][j].width, stbs[i][j].height, path);
    }
    max_dim = max_dim > max_sizes[i] ? max_dim : max_sizes[i];
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
    textures[i] = create_vulkan_texture(ctx, max_sizes[i], max_sizes[i], string_arrs[i].num_paths);

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
      bool needs_resized = (w != max_sizes[i]) || (h != max_sizes[i]);
      if (needs_resized) {
        stbir_resize_uint8_srgb(data, w, h, 0, resized, max_sizes[i], max_sizes[i], 0, STBIR_RGBA);
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
