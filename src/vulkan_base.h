#pragma once

#include "hashmap.h"
#include "renderer.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <stdlib.h>
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

#define MAX_LAYOUT_BINDINGS (4)
#define MAX_DESCRIPTOR_WRITES (4)
#define MAX_DESCRIPTOR_COPIES (4)
#define MAX_DESCRIPTOR_BUFFER_INFOS (4)
#define MAX_DESCRIPTOR_IMAGE_INFOS (4)

inline const char *vk_result_string(VkResult result) {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_UNKNOWN:
    return "VK_ERROR_UNKNOWN";
  default:
    return "UNKNOWN_VK_RESULT";
  }
}

// TODO adapt to print validation layer errors on failure
#define VK_CHECK(result, fmt)                                                  \
  do {                                                                         \
    if ((result) != VK_SUCCESS) {                                              \
      const char *type = vk_result_string(result);                             \
      fprintf(stderr, "Vulkan error of type %s at %s:%d\n" fmt "\n", type,     \
              __FILE__, __LINE__);                                             \
      fflush(stderr);                                                          \
      fflush(stdout);                                                          \
      assert(0);                                                               \
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

  u32 image_count;
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

struct ShaderStage {
  VkShaderModule module;
  VkShaderStageFlagBits stage;
  const char *entry_point;
};

using ShaderCacheMap = HashMap<const char *, ShaderStage, CStringHashFunctor,
                               CStringEqualityFunctor>;
struct VulkanShaderCache {
  VkDevice device;
  ShaderCacheMap hash_map;
};

struct VulkanContext {
  // needed most often
  GLFWwindow *window;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;
  u32 image_index;

  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
  SwapchainStorage swapchain_storage;
  VkRenderPass render_pass;
  VkFramebuffer framebuffers[NUM_SWAPCHAIN_IMAGES];

  FrameSyncObjects frame_sync_objects[MAX_FRAMES_IN_FLIGHT];
  u32 current_frame;
  uint8_t current_frame_index;

  VkCommandBuffer graphics_command_buffers[NUM_SWAPCHAIN_IMAGES];
  VkCommandBuffer compute_command_buffers[NUM_SWAPCHAIN_IMAGES];

  VkPipelineCache pipeline_cache;
  VkPipelineVertexInputStateCreateInfo vertex_layouts[VERTEX_LAYOUT_COUNT];

  // this cache is a templated type. the compiler freaks out if I put the
  // data directly here. I need to use a pointer instead. I don't understand
  // the fundamental issue. Something to do with constructors perhaps. Could
  // be good to try and lump allocations for these into one.
  VulkanShaderCache *shader_cache;

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
  u8 *mapped;
  u32 size;
};

struct ViewportState {
  VkViewport viewport;
  VkRect2D scissor;
};

enum BlendMode { BLEND_MODE_OPAQUE, BLEND_MODE_ALPHA };

struct ShaderSpec {
  const u32 *spv;
  u32 size;
  const char *name;
  VkShaderStageFlagBits stage_flags;
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
  u32 total_size;
  VkBuffer destination_buffers[MAX_COPY_REGIONS];
  VkBufferCopy copy_regions[MAX_COPY_REGIONS];
  u32 num_copy_regions;
  u32 offset;
};

struct VulkanVertexLayout {
  size_t binding_description_count;
  VkVertexInputBindingDescription binding_descriptions[MAX_VERTEX_BINDINGS];

  size_t attribute_description_count;
  VkVertexInputAttributeDescription
      attribute_descriptions[MAX_VERTEX_ATTRIBUTES];
};

// TODO consider rewriting into a VertexLayout
// right now this is conceptualized as a builder, but all the state
// needs to persist at least until pipeline creation time
// I could conceptualize this as just the layout, and finalize it in
// the end
struct VertexLayoutBuilder {
  size_t binding_description_count;
  VkVertexInputBindingDescription binding_descriptions[MAX_VERTEX_BINDINGS];

  size_t attribute_description_count;
  VkVertexInputAttributeDescription
      attribute_descriptions[MAX_VERTEX_ATTRIBUTES];

  VkPipelineVertexInputStateCreateInfo vertex_input_state;
};

using VertexLayout = VertexLayoutBuilder;

struct DescriptorSetBuilder {
  VkDevice device;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSetLayoutBinding layout_bindings[MAX_LAYOUT_BINDINGS];
  u32 binding_count;

  u32 write_descriptor_count;
  VkWriteDescriptorSet descriptor_writes[MAX_DESCRIPTOR_WRITES];
  u32 copy_descriptor_count;
  VkCopyDescriptorSet descriptor_copies[MAX_DESCRIPTOR_COPIES];

  u32 buffer_info_count;
  VkDescriptorBufferInfo descriptor_buffer_infos[MAX_DESCRIPTOR_BUFFER_INFOS];
  u32 image_info_count;
  VkDescriptorImageInfo descriptor_image_infos[MAX_DESCRIPTOR_IMAGE_INFOS];
};

struct VulkanTexture {
  VkImage image;
  u32 height, width;
  VkDeviceMemory device_memory;
  VkImageView image_view;
};

struct RenderCall {
  u32 num_vertices;
  u32 instance_count;
  VkPipeline graphics_pipeline;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSet descriptor_set;

  u32 num_vertex_buffers;
  VkDeviceSize vertex_buffer_offsets[MAX_VERTEX_BINDINGS];
  VkBuffer vertex_buffers[MAX_VERTEX_BINDINGS];

  VkDeviceSize index_buffer_offset;
  u32 num_indices;
  VkBuffer index_buffer;
  bool is_indexed;
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
void write_to_vulkan_buffer(VulkanContext *context, const void *src_data,
                            VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer);
VkCommandBuffer begin_single_use_command_buffer(const VulkanContext *context);
void end_single_use_command_buffer(const VulkanContext *context,
                                   VkCommandBuffer command_buffer);
VkShaderModule create_shader_module(VkDevice device, const u32 *code,
                                    size_t code_size);

VkPipeline create_graphics_pipeline(VkDevice device, PipelineConfig *config,
                                    VkPipelineCache pipeline_cache);

VkPipeline create_default_graphics_pipeline(
    const VulkanContext *context, const char *vertex_shader_name,
    const char *fragment_shader_name,
    const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
    VkPipelineLayout pipeline_layout);

bool begin_frame(VulkanContext *context);
VkCommandBuffer begin_command_buffer(const VulkanContext *context);

ViewportState create_viewport_state_offset(VkExtent2D swapchain_extent,
                                           VkOffset2D offset);

ViewportState create_viewport_state_xy(VkExtent2D swapchain_extent, u32 x,
                                       u32 y);

void begin_render_pass(const VulkanContext *context,
                       VkCommandBuffer command_buffer, VkClearValue clear_value,
                       VkOffset2D offset);
void submit_and_present(const VulkanContext *context,
                        VkCommandBuffer command_buffer);

VkPipelineVertexInputStateCreateInfo create_vertex_input_state(
    u32 binding_description_count,
    const VkVertexInputBindingDescription *binding_descriptions,
    u32 attribute_description_count,
    const VkVertexInputAttributeDescription *attribute_descriptions);

VkPipelineLayout
create_pipeline_layout(VkDevice device,
                       const VkDescriptorSetLayout *descriptor_set_layout,
                       u32 set_layout_count);

VkDescriptorPool create_descriptor_pool(VkDevice device,
                                        const VkDescriptorPoolSize *pool_sizes,
                                        u32 pool_size_count, u32 max_sets);

VkDescriptorSet
create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
                      VkDescriptorSetLayout *descriptor_set_layout);

VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device,
                             const VkDescriptorSetLayoutBinding *bindings,
                             u32 binding_count);

StagingArena create_staging_arena(const VulkanContext *context, u32 total_size);
u32 stage_data_explicit(const VulkanContext *context, StagingArena *arena,
                        const void *data, u32 size, VkBuffer destination,
                        u32 dst_offset);
u32 stage_data_auto(const VulkanContext *context, StagingArena *arena,
                    const void *data, u32 size, VkBuffer destination);

#define STAGE_ARRAY(context, arena, array, destination)                        \
  (stage_data_auto(context, arena, array, sizeof(array), destination))

void flush_staging_arena(const VulkanContext *context, StagingArena *arena);

ShaderStage create_shader_stage(VkShaderModule module,
                                VkShaderStageFlagBits stage,
                                const char *entry_point = "main");

// TODO abstract over dynamic uniform buffers
UniformBuffer create_uniform_buffer(const VulkanContext *context,
                                    u32 buffer_size);

void destroy_uniform_buffer(const VulkanContext *context,
                            UniformBuffer *uniform_buffer);

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data,
                             u32 offset, u32 size);

PipelineConfig create_default_graphics_pipeline_config(
    const VulkanContext *context, const char *vertex_shader_name,
    const char *fragment_shader_name,
    const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
    VkPipelineLayout pipeline_layout);

VkPipelineVertexInputStateCreateInfo
get_common_vertex_input_state(VulkanContext *context, VertexLayoutID layout_id);

VkVertexInputBindingDescription
create_instanced_vertex_binding_description(u32 binding, u32 stride);
VkVertexInputAttributeDescription
create_vertex_attribute_description(u32 location, u32 binding, VkFormat format,
                                    u32 offset);
VkVertexInputBindingDescription create_vertex_binding_description(u32 binding,
                                                                  u32 stride);

VertexLayoutBuilder create_vertex_layout_builder();
void push_vertex_binding(VertexLayoutBuilder *builder, u32 binding, u32 stride,
                         VkVertexInputRate input_rate);
void push_vertex_attribute(VertexLayoutBuilder *builder, u32 location,
                           u32 binding, VkFormat format, u32 offset);
VkPipelineVertexInputStateCreateInfo
build_vertex_input_state(VertexLayoutBuilder *builder);
void finalize_vertex_input_state(VertexLayoutBuilder *builder);

u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter,
                     VkMemoryPropertyFlags properties);

VulkanTexture create_vulkan_texture_from_file(VulkanContext *context,
                                              const char *path);

void load_vulkan_textures(VulkanContext *context, const char **paths,
                          u32 num_paths, VulkanTexture *out_textures);

void destroy_vulkan_texture(VkDevice device, VulkanTexture *vulkan_texture);

VkSampler create_sampler(VkDevice device);

VkDescriptorSetLayoutBinding create_descriptor_set_layout_binding(
    u32 binding, VkShaderStageFlags stage_flags,
    VkDescriptorType descriptor_type, u32 descriptor_count);

DescriptorSetBuilder create_descriptor_set_builder(VulkanContext *context);

VkDescriptorSet build_descriptor_set(DescriptorSetBuilder *builder,
                                     VkDescriptorPool descriptor_pool);

void add_uniform_buffer_descriptor_set(DescriptorSetBuilder *builder,
                                       UniformBuffer *uniform_buffer,
                                       u32 offset, u32 range, u32 binding,
                                       u32 descriptor_count,
                                       VkShaderStageFlags stage_flags,
                                       bool dynamic);

void add_texture_descriptor_set(DescriptorSetBuilder *builder,
                                VulkanTexture *texture, VkSampler sampler,
                                u32 binding, u32 descriptor_count,
                                VkShaderStageFlags stage_flags);

void destroy_descriptor_set_builder(DescriptorSetBuilder *builder);

VulkanShaderCache *create_shader_cache(VkDevice device);
bool cache_shader_module(VulkanShaderCache *cache, ShaderSpec spec);
void destroy_shader_cache(VulkanShaderCache *cache);

void render_mesh(VkCommandBuffer command_buffer, RenderCall *render_call);
