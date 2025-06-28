#pragma once

#include "renderer.h"
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
#define MAX_SHADER_STAGE_COUNT (5)
#define MAX_PHYSICAL_DEVICES (16)
#define MAX_COPY_REGIONS (32)
#define MAX_VERTEX_BINDINGS (4)
#define MAX_VERTEX_ATTRIBUTES (4)

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
  // needed most often
  GLFWwindow *window;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;
  uint32_t image_index;

  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
  SwapchainStorage swapchain_storage;
  VkRenderPass render_pass;
  VkFramebuffer framebuffers[NUM_SWAPCHAIN_IMAGES];

  FrameSyncObjects frame_sync_objects[MAX_FRAMES_IN_FLIGHT];
  uint32_t current_frame;
  uint8_t current_frame_index;

  VkCommandBuffer graphics_command_buffers[NUM_SWAPCHAIN_IMAGES];
  VkCommandBuffer compute_command_buffers[NUM_SWAPCHAIN_IMAGES];

  VkPipelineCache pipeline_cache;
  VkPipelineVertexInputStateCreateInfo vertex_layouts[VERTEX_LAYOUT_COUNT];

  // init
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  QueueFamilyIndices queue_family_indices;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceProperties physical_device_properties;

  VkCommandPool graphics_command_pool;
  VkCommandPool compute_command_pool;
  VkCommandPool present_command_pool;
  VkCommandPool transient_command_pool;
  bool compute_queue_index_is_different_than_graphics;
  bool present_queue_index_is_different_than_graphics;
};

struct VulkanBuffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkMemoryRequirements memory_requirements;
  VkMemoryPropertyFlags memory_property_flags;
};

struct UniformBuffer {
  VulkanBuffer vulkan_buffer;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;
  void *mapped;
  uint32_t size;
};

struct ViewportState {
  VkViewport viewport;
  VkRect2D scissor;
};

enum BlendMode { BLEND_MODE_OPAQUE, BLEND_MODE_ALPHA };

struct ShaderStage {
  VkShaderModule module;
  VkShaderStageFlagBits stage;
  const char *entry_point;
};

struct GraphicsPipelineStages {
  ShaderStage vertex_shader;
  ShaderStage fragment_shader;
};

struct PipelineConfig {
  // pipeline create info
  ShaderStage stages[MAX_SHADER_STAGE_COUNT];
  size_t stage_count;
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state_create_info;
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;

  // input assembly
  VkPrimitiveTopology topology;
  VkBool32 primitive_restart_enabled;

  // rasterization_state
  VkPolygonMode polygon_mode;
  VkCullModeFlags cull_mode;
  VkFrontFace front_face;

  // multisample state
  VkSampleCountFlagBits sample_count_flag;

  // color blend state
  BlendMode blend_mode;

  VkExtent2D swapchain_extent;
};

enum BufferType {
  BUFFER_TYPE_STAGING,
  BUFFER_TYPE_VERTEX,
  BUFFER_TYPE_INDEX,
  BUFFER_TYPE_UNIFORM,
};

struct StagingArena {
  VulkanBuffer buffer;
  uint32_t total_size;
  VkBuffer destination_buffers[MAX_COPY_REGIONS];
  VkBufferCopy copy_regions[MAX_COPY_REGIONS];
  uint32_t num_copy_regions;
  uint32_t offset;
};

struct VulkanVertexLayout {
  size_t binding_description_count;
  VkVertexInputBindingDescription binding_descriptions[MAX_VERTEX_BINDINGS];

  size_t attribute_description_count;
  VkVertexInputAttributeDescription
      attribute_descriptions[MAX_VERTEX_ATTRIBUTES];
};

extern const VulkanVertexLayout vulkan_vertex_layouts[VERTEX_LAYOUT_COUNT];

VulkanContext create_vulkan_context(const char *title);
void destroy_vulkan_context(VulkanContext *);
VulkanBuffer create_buffer_explicit(const VulkanContext *context,
                                    VkBufferUsageFlags usage, VkDeviceSize size,
                                    VkMemoryPropertyFlags properties);
VulkanBuffer create_buffer(const VulkanContext *context, BufferType buffer_type,
                           VkDeviceSize size);
void destroy_vulkan_buffer(const VulkanContext *context, VulkanBuffer buffer);
void write_to_vulkan_buffer(VulkanContext *context, void *src_data,
                            VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer);
VkCommandBuffer begin_single_use_command_buffer(VulkanContext *context);
void end_single_use_command_buffer(VulkanContext *context,
                                   VkCommandBuffer command_buffer);
VkShaderModule create_shader_module(VkDevice device, const uint32_t *code,
                                    size_t code_size);

VkPipeline create_graphics_pipeline(VkDevice device, PipelineConfig *config,
                                    VkPipelineCache pipeline_cache);

bool begin_frame(VulkanContext *context);
VkCommandBuffer begin_command_buffer(const VulkanContext *context);
ViewportState create_viewport_state(VkExtent2D swapchain_extent,
                                    VkOffset2D offset);
void begin_render_pass(const VulkanContext *context,
                       VkCommandBuffer command_buffer, VkClearValue clear_value,
                       VkOffset2D offset);
void submit_and_present(const VulkanContext *context,
                        VkCommandBuffer command_buffer);

VkPipelineVertexInputStateCreateInfo create_vertex_input_state(
    uint32_t binding_description_count,
    const VkVertexInputBindingDescription *binding_descriptions,
    uint32_t attribute_description_count,
    const VkVertexInputAttributeDescription *attribute_descriptions);

VkPipelineLayout
create_pipeline_layout(VkDevice device,
                       const VkDescriptorSetLayout *descriptor_set_layout,
                       uint32_t set_layout_count);

VkDescriptorPool create_descriptor_pool(VkDevice device,
                                        const VkDescriptorPoolSize *pool_sizes,
                                        uint32_t pool_size_count,
                                        uint32_t max_sets);

VkDescriptorSet
create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
                      VkDescriptorSetLayout descriptor_set_layout);

VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device,
                             const VkDescriptorSetLayoutBinding *bindings,
                             uint32_t binding_count);

void update_uniform_descriptor_sets(VkDevice device, VkBuffer buffer,
                                    VkDeviceSize offset, VkDeviceSize range,
                                    VkDescriptorSet descriptor_set,
                                    uint32_t binding);

StagingArena create_staging_arena(const VulkanContext *context,
                                  uint32_t total_size);
uint32_t stage_data_explicit(const VulkanContext *context, StagingArena *arena,
                             void *data, uint32_t size, VkBuffer destination,
                             uint32_t dst_offset);
uint32_t stage_data_auto(const VulkanContext *context, StagingArena *arena,
                         void *data, uint32_t size, VkBuffer destination);

#define STAGE_ARRAY(context, arena, array, destination)                        \
  (stage_data_auto(context, arena, array, sizeof(array), destination))

void flush_staging_arena(const VulkanContext *context, StagingArena *arena);
ShaderStage create_shader_stage(VkShaderModule module,
                                VkShaderStageFlagBits stage,
                                const char *entry_point);

UniformBuffer create_uniform_buffer(const VulkanContext *context,
                                    uint32_t buffer_size,
                                    VkDescriptorPool descriptor_pool);

UniformBuffer create_dynamic_uniform_buffer(const VulkanContext *context,
                                            uint32_t buffer_size,
                                            VkDescriptorPool descriptor_pool);

void destroy_uniform_buffer(const VulkanContext *context,
                            UniformBuffer *uniform_buffer);

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data,
                             uint32_t size);

PipelineConfig create_default_graphics_pipeline_config(
    const VulkanContext *context, const GraphicsPipelineStages shader_stages,
    VertexLayoutID layout_id, VkPipelineLayout pipeline_layout);
