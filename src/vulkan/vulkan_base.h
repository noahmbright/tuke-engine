#pragma once

#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef float f32;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;

#define VK_CHECK(result, fmt)                                                                                          \
  do {                                                                                                                 \
    if ((result) != VK_SUCCESS) {                                                                                      \
      fprintf(stderr, "[%s] %s:%d — %s: " fmt "\n", __func__, __FILE__, __LINE__, vk_result_string(result));           \
      fflush(stderr);                                                                                                  \
      fflush(stdout);                                                                                                  \
      assert(0);                                                                                                       \
    }                                                                                                                  \
  } while (0)

#define VK_CHECK_VARIADIC(result, fmt, ...)                                                                            \
  do {                                                                                                                 \
    if ((result) != VK_SUCCESS) {                                                                                      \
      fprintf(stderr, "[%s] %s:%d — %s: " fmt "\n", __func__, __FILE__, __LINE__, vk_result_string(result),            \
              __VA_ARGS__);                                                                                            \
      fflush(stderr);                                                                                                  \
      fflush(stdout);                                                                                                  \
      assert(0);                                                                                                       \
    }                                                                                                                  \
  } while (0)

// Frames in flight is usually not increased very high because it increases
// latency between user input and rendering for a given frame.
// Frames in flight only applies to resources shared by the CPU and GPU.
// Resources used only by the GPU do not need multi-buffered.
#define MAX_FRAMES_IN_FLIGHT (2)

#define NUM_SWAPCHAIN_IMAGES (4)
#define NUM_QUEUE_FAMILY_INDICES (3) // Graphics, Compute, Present
#define NUM_ATTACHMENTS (2)          // 0 color, 1 depth
#define MAX_SHADER_STAGE_COUNT (5)
#define MAX_PHYSICAL_DEVICES (16)
#define MAX_QUEUE_FAMILIES (16)
#define MAX_COPY_REGIONS (32)
#define MAX_VERTEX_BINDINGS (4)
#define MAX_VERTEX_ATTRIBUTES (4)
#define MAX_DESCRIPTOR_SETS (4)

#define MAX_LAYOUT_BINDINGS (4)
#define MAX_DESCRIPTOR_WRITES (4)
#define MAX_DESCRIPTOR_COPIES (4)
#define MAX_DESCRIPTOR_BUFFER_INFOS (4)
#define MAX_DESCRIPTOR_IMAGE_INFOS (4)

#define MAX_BUFFER_UPLOADS (32)

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

typedef struct {
  VkSemaphore image_available_semaphore;
  VkSemaphore render_finished_semaphore;
  VkFence in_flight_fence;
} FrameSyncObjects;

struct QueueFamilyIndices {
  int graphics_family;
  int present_family;
  int compute_family;
};

// History: here is the first C typedef struct of this project's life cycle.
typedef struct {
  u32 extension_count;
  const char **extensions;
  VkSurfaceKHR (*create_surface)(VkInstance, void *window);
  void *window;
  int width;
  int height;
} VulkanWindowInfo;

struct DepthBuffer {
  VkImage image;
  VkImageView image_view;
  VkDeviceMemory device_memory;
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

  DepthBuffer depth_buffers[NUM_SWAPCHAIN_IMAGES];
};

struct ShaderModule {
  VkShaderModule module;
  VkShaderStageFlagBits stage;
  const char *entry_point;
};

struct VulkanContext {
  // needed most often
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;
  u32 image_index;

  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
  SwapchainStorage swapchain_storage;
  i32 window_framebuffer_width;
  i32 window_framebuffer_height;

  // TODO get this out of the context, add a render pass manager
  VkRenderPass render_pass;
  VkFramebuffer framebuffers[NUM_SWAPCHAIN_IMAGES];

  FrameSyncObjects frame_sync_objects[MAX_FRAMES_IN_FLIGHT];
  u64 current_frame;
  u8 current_frame_index;

  VkCommandBuffer graphics_command_buffers[NUM_SWAPCHAIN_IMAGES];
  VkCommandBuffer compute_command_buffers[NUM_SWAPCHAIN_IMAGES];

  VkPipelineCache pipeline_cache;

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

// Uniform buffer layout is currently managed manually via push_uniform, which
// advances a linear offset and returns an (offset, size) handle. Callers are
// responsible for:
//   - passing data whose size matches the reserved slot size
//   - ensuring structs whose sizes are not multiples of minUniformBufferOffsetAlignment
//     don't leave the next slot unaligned (validation error on vkUpdateDescriptorSets)
//
// Potential improvements:
//   - Store alignment in UniformBufferManager and round up current_offset after
//     each push, so misalignment is structurally impossible.
//   - Replace push_uniform entirely with a single mapped struct whose fields are
//     the actual uniform types. offsetof() gives correct aligned offsets for free,
//     and writes become direct field assignments through the mapped pointer.
//   - Add explicit data_size to write_to_uniform_buffer so the copy size is always
//     the caller's actual type size, not the reserved slot size.
struct UniformBufferManager {
  u32 current_offset;
};

struct UniformWrite {
  u32 offset;
  u32 size;
};

struct UniformBuffer {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
};

struct ReadOnlyStorageBuffer {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
};

struct ReadWriteStorageBuffer {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
};

struct ViewportState {
  VkViewport viewport;
  VkRect2D scissor;
};

enum BlendMode { BLEND_MODE_OPAQUE, BLEND_MODE_ALPHA };

struct PipelineConfig {
  // pipeline create info
  VkPipelineShaderStageCreateInfo stages[MAX_SHADER_STAGE_COUNT];
  u32 stage_count;
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
  BUFFER_TYPE_COHERENT_STREAMING,
  BUFFER_TYPE_READONLY_STORAGE,
  BUFFER_TYPE_READ_WRITE_STORAGE,
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
  u32 binding_count;
  VkVertexInputBindingDescription bindings[MAX_VERTEX_BINDINGS];

  u32 attribute_count;
  VkVertexInputAttributeDescription attributes[MAX_VERTEX_ATTRIBUTES];

  // vertex_input_state is the finalized state, used in pipeline creation
  VkPipelineVertexInputStateCreateInfo vertex_input_state;
};

struct DescriptorSetBuilder {
  VkDevice device;
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

struct DescriptorSetHandle {
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_set_layout;
};

// TODO don't like how this is redefined for my personal engine, STB, and Vulkan
struct VulkanImageData {
  u32 width;
  u32 height;
  u32 n_channels;
  u8 *data;
};

struct VulkanTexture {
  VkImage image;
  u32 height;
  u32 width;
  VkDeviceMemory device_memory;
  VkImageView image_view;
};

struct RenderCall {
  u32 num_vertices;
  u32 instance_count;
  VkPipeline graphics_pipeline;
  VkPipelineLayout pipeline_layout;

  u32 num_descriptor_sets;
  VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SETS];

  u32 num_vertex_buffers;
  VkDeviceSize vertex_buffer_offsets[MAX_VERTEX_BINDINGS];
  VkBuffer vertex_buffers[MAX_VERTEX_BINDINGS];

  VkDeviceSize index_buffer_offset;
  u32 num_indices;
  VkBuffer index_buffer;
  bool is_indexed;
};

struct BufferHandle {
  u64 offset;
  u64 size;
  BufferType buffer_type;
  void *data;
};

struct BufferUploadQueue {
  u64 vertex_buffer_offset;
  u64 index_buffer_offset;
  BufferHandle slices[MAX_BUFFER_UPLOADS];
  u32 num_slices;
};

struct BufferManager {
  VulkanContext *context;
  VulkanBuffer vertex_buffer;
  VulkanBuffer index_buffer;
  StagingArena staging_arena;
};

struct CoherentStreamingBuffer {
  VulkanBuffer vulkan_buffer;
  u8 *data;
  u32 size;
  u32 head;
};

struct ColorDepthFramebuffer {
  VkRenderPass render_pass;
  VkFramebuffer framebuffer;

  VkImage color_image;
  VkDeviceMemory color_image_device_memory;
  VkImageView color_image_view;

  VkImage depth_image;
  VkDeviceMemory depth_image_device_memory;
  VkImageView depth_image_view;
};

VulkanContext create_vulkan_context(const char *title, VulkanWindowInfo window_info);
void destroy_vulkan_context(VulkanContext *);
VulkanBuffer create_buffer_explicit(const VulkanContext *context, VkBufferUsageFlags usage, VkDeviceSize size,
                                    VkMemoryPropertyFlags properties);
VulkanBuffer create_buffer(const VulkanContext *context, BufferType buffer_type, VkDeviceSize size);
void destroy_vulkan_buffer(const VulkanContext *context, VulkanBuffer buffer);
void write_to_vulkan_buffer(VulkanContext *context, const void *src_data, VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer);
VkCommandBuffer begin_single_use_command_buffer(const VulkanContext *context);
void end_single_use_command_buffer(const VulkanContext *context, VkCommandBuffer command_buffer);
VkShaderModule create_shader_module(VkDevice device, const u32 *code, u32 code_size);

VkPipeline create_graphics_pipeline(VkDevice device, const PipelineConfig *config, VkPipelineCache pipeline_cache);

VkPipeline create_default_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                            VkShaderModule vertex_shader, VkShaderModule fragment_shader,
                                            const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
                                            VkPipelineLayout pipeline_layout);

void begin_frame(VulkanContext *context);
VkCommandBuffer begin_command_buffer(const VulkanContext *context);

ViewportState create_viewport_state_offset(VkExtent2D swapchain_extent, VkOffset2D offset);

ViewportState create_viewport_state_xy(VkExtent2D swapchain_extent, u32 x, u32 y);

void begin_render_pass(const VulkanContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, const VkClearValue *clear_value, u32 clear_value_count,
                       VkOffset2D offset);
void submit_and_present(const VulkanContext *context, VkCommandBuffer command_buffer);

VkPipelineVertexInputStateCreateInfo
create_vertex_input_state(u32 binding_description_count, const VkVertexInputBindingDescription *binding_descriptions,
                          u32 attribute_description_count,
                          const VkVertexInputAttributeDescription *attribute_descriptions);

VkPipelineLayout create_pipeline_layout(VkDevice device, const VkDescriptorSetLayout *descriptor_set_layouts,
                                        u32 set_layout_count);

VkDescriptorPool create_descriptor_pool(VkDevice device, const VkDescriptorPoolSize *pool_sizes, u32 pool_size_count,
                                        u32 max_sets);

VkDescriptorSet create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
                                      VkDescriptorSetLayout *descriptor_set_layout);

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutBinding *bindings,
                                                   u32 binding_count);

StagingArena create_staging_arena(const VulkanContext *context, u32 total_size);
u32 stage_data_explicit(const VulkanContext *context, StagingArena *arena, const void *data, u32 size,
                        VkBuffer destination, u32 dst_offset);
u32 stage_data_auto(const VulkanContext *context, StagingArena *arena, const void *data, u32 size,
                    VkBuffer destination);

#define STAGE_ARRAY(context, arena, array, destination)                                                                \
  (stage_data_auto(context, arena, array, sizeof(array), destination))

void flush_staging_arena(const VulkanContext *context, StagingArena *arena);

ShaderModule create_shader_stage(VkShaderModule module, VkShaderStageFlagBits stage, const char *entry_point = "main");

// TODO abstract over dynamic uniform buffers
UniformBuffer create_uniform_buffer(const VulkanContext *context, u32 buffer_size);

void destroy_uniform_buffer(const VulkanContext *context, UniformBuffer *uniform_buffer);

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data, UniformWrite uniform_write);

UniformBufferManager create_uniform_buffer_manager();

UniformWrite push_uniform(UniformBufferManager *uniform_buffer_manager, u32 size);

PipelineConfig create_default_graphics_pipeline_config(VkRenderPass render_pass, VkShaderModule vertex_shader,
                                                       VkShaderModule fragment_shader,
                                                       const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
                                                       VkPipelineLayout pipeline_layout);

VkVertexInputBindingDescription create_instanced_vertex_binding_description(u32 binding, u32 stride);
VkVertexInputAttributeDescription create_vertex_attribute_description(u32 location, u32 binding, VkFormat format,
                                                                      u32 offset);
VkVertexInputBindingDescription create_vertex_binding_description(u32 binding, u32 stride);

u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties);
VulkanTexture create_vulkan_texture(VulkanContext *context, VulkanImageData image_data, VulkanBuffer staging_buffer,
                                    void *ptr_to_mapped_memory);
void load_vulkan_textures(VulkanContext *context, const VulkanImageData *image_datas, u32 num_images,
                          VulkanTexture *out_textures);
void destroy_vulkan_texture(VkDevice device, VulkanTexture *vulkan_texture);
VkSampler create_sampler(VkDevice device);

VkDescriptorSetLayoutBinding create_descriptor_set_layout_binding(u32 binding, VkShaderStageFlags stage_flags,
                                                                  VkDescriptorType descriptor_type,
                                                                  u32 descriptor_count);
DescriptorSetBuilder create_descriptor_set_builder(VulkanContext *context);

// TODO this function/builder structure itself needs updated to decouple layouts
// from descriptor sets - a layout should be a list of descriptors
DescriptorSetHandle build_descriptor_set(DescriptorSetBuilder *builder, VkDescriptorPool descriptor_pool);

void add_uniform_buffer_descriptor_set(DescriptorSetBuilder *builder, const UniformBuffer *uniform_buffer, u32 offset,
                                       u32 range, u32 binding, u32 descriptor_count, VkShaderStageFlags stage_flags,
                                       bool dynamic);

void add_image_descriptor_set(DescriptorSetBuilder *builder, VkImageView image_view, VkSampler sampler, u32 binding,
                              u32 descriptor_count, VkShaderStageFlags stage_flags);

void destroy_descriptor_set_handle(VkDevice device, DescriptorSetHandle *handle);

void render_mesh(VkCommandBuffer command_buffer, RenderCall *render_call);

BufferUploadQueue create_buffer_upload_queue();
const BufferHandle *upload_data(BufferUploadQueue *queue, BufferType buffer_type, void *data, u64 size);
BufferManager flush_buffers(VulkanContext *context, BufferUploadQueue *queue);
void destroy_buffer_manager(BufferManager *buffer_manager);

// these macros are specifically for arrays, e.g., f32 array[], with the [];
// will fail on pointers to arrays of data
#define UPLOAD_VERTEX_ARRAY(queue, array) (upload_data(&queue, BUFFER_TYPE_VERTEX, (void *)array, sizeof(array)))

#define UPLOAD_INDEX_ARRAY(queue, array) (upload_data(&queue, BUFFER_TYPE_INDEX, (void *)array, sizeof(array)))

CoherentStreamingBuffer create_coherent_streaming_buffer(const VulkanContext *ctx, u32 size);
void write_to_streaming_buffer(CoherentStreamingBuffer *coherent_streaming_buffer, void *data, u32 size);

VkRenderPass create_render_pass(VkDevice device, u32 num_attachment_descriptions,
                                const VkAttachmentDescription *attachment_descriptions, u32 num_subpass_descriptions,
                                const VkSubpassDescription *subpass_descriptions, u32 num_dependencies,
                                const VkSubpassDependency *dependencies);

VkFormat find_depth_format(VkPhysicalDevice physical_device);
VkRenderPass create_color_depth_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format);
VkRenderPass create_color_render_pass(VkDevice device, VkFormat format);

VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass render_pass, u32 num_attachments,
                                 VkImageView *image_view_attachments, VkExtent2D extent);

VkImage create_default_image(const VulkanContext *context, u32 width, u32 height, VkImageUsageFlags usage,
                             VkFormat format);

VkDeviceMemory allocate_and_bind_image_memory(const VulkanContext *context, VkImage image);

VkImageView create_default_image_view(const VulkanContext *context, VkImage image, VkFormat format,
                                      VkImageAspectFlags aspect_flags);

ColorDepthFramebuffer create_color_depth_framebuffer(const VulkanContext *context, VkExtent2D extent,
                                                     VkFormat color_format, VkFormat depth_format);

void destroy_color_depth_framebuffer(const VulkanContext *context, ColorDepthFramebuffer *color_depth_framebuffer);

void transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
                             VkImageLayout new_layout);
