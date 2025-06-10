#pragma once

#include "vulkan/vulkan_core.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "window.h"

#define NUM_SWAPCHAIN_IMAGES (4)
#define MAX_FRAMES_IN_FLIGHT (2)
#define NUM_QUEUE_FAMILY_INDICES (3)
#define NUM_ATTACHMENTS (1) // 0 color, TODO 1 depth

#define VK_CHECK(result, fmt)                                                  \
  do {                                                                         \
    if ((result) != VK_SUCCESS) {                                              \
      fprintf(stderr, "Vulkan error at %s:%d\n" fmt "\n", __FILE__, __LINE__); \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define VK_CHECK_VARIADIC(result, fmt, ...)                                    \
  do {                                                                         \
    if ((result) != VK_SUCCESS) {                                              \
      fprintf(stderr, "Vulkan error at %s:%d\n" fmt "\n", __FILE__, __LINE__,  \
              __VA_ARGS__);                                                    \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct FrameSyncObjects {
  VkSemaphore image_available_semaphore;
  VkSemaphore render_finished_semaphore;
  VkFence in_flight_fence;
} FrameSyncObjects;

struct QueueFamilyIndices {
  int graphics_family;
  int present_family;
  int compute_family;
};

struct SwapchainStorage {
  bool use_static;

  uint32_t image_count;
  union {
    struct {
      VkImage images[NUM_SWAPCHAIN_IMAGES];
      VkImageView image_views[NUM_SWAPCHAIN_IMAGES];
    } static_storage;

    struct {
      VkImage *images;
      VkImageView *image_views;
    } dynamic_storage;
  } as;
};

struct VulkanContext {
  GLFWwindow *window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  QueueFamilyIndices queue_family_indices;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceProperties physical_device_properties;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;

  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
  SwapchainStorage swapchain_storage;
  VkRenderPass render_pass;
  VkFramebuffer framebuffers[NUM_SWAPCHAIN_IMAGES];

  FrameSyncObjects frame_sync_objects[MAX_FRAMES_IN_FLIGHT];
  uint32_t current_frame;

  VkCommandPool graphics_command_pool;
  bool compute_queue_index_is_different_than_graphics;
  VkCommandPool compute_command_pool;
  bool present_queue_index_is_different_than_graphics;
  VkCommandPool present_command_pool;
  VkCommandPool transient_command_pool;

  VkCommandBuffer graphics_command_buffers[NUM_SWAPCHAIN_IMAGES];
  VkCommandBuffer compute_command_buffers[NUM_SWAPCHAIN_IMAGES];

  VkPipelineCache pipeline_cache;
};

struct VulkanBuffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkMemoryRequirements memory_requirements;
  VkMemoryPropertyFlags memory_property_flags;
};

VulkanContext create_vulkan_context();
void destroy_vulkan_context(VulkanContext *);
VulkanBuffer create_buffer(VulkanContext *context, VkBufferUsageFlags usage,
                           uint32_t size, VkMemoryPropertyFlags properties);
void destroy_vulkan_buffer(VulkanContext *context, VulkanBuffer buffer);
void write_to_vulkan_buffer(VulkanContext *context, void *src_data,
                            VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer);
VkCommandBuffer begin_single_use_command_buffer(VulkanContext *context);
void end_single_use_command_buffer(VulkanContext *context,
                                   VkCommandBuffer command_buffer);
VkShaderModule create_shader_module(VkDevice device, const uint32_t *code,
                                    size_t code_size);

VkPipeline create_graphics_pipeline(
    VkDevice device, VkPipelineShaderStageCreateInfo *stages,
    size_t stage_count,
    VkPipelineVertexInputStateCreateInfo *vertex_input_state_create_info,
    VkPrimitiveTopology topology, VkExtent2D swapchain_extent,
    VkPolygonMode polygon_mode, VkSampleCountFlagBits sample_count_flag,
    VkBool32 blending_enabled, VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout, VkPipelineCache pipeline_cache,
    VkCullModeFlags cull_mode);
