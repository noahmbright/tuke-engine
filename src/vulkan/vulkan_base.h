#pragma once

#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <stdbool.h>
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
      fprintf(                                                                                                         \
          stderr, "[%s] %s:%d — %s: " fmt "\n", __func__, __FILE__, __LINE__, vk_result_string(result), __VA_ARGS__    \
      );                                                                                                               \
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
#define POOL_SIZE_DESCRIPTOR_COUNT (1024)

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

typedef struct {
  int graphics_family;
  int present_family;
  int compute_family;
} QueueFamilyIndices;

// History: here is the first C typedef struct of this project's life cycle.
//          At least, the first one I noticed (may have copied another one before, whoops)
typedef struct {
  u32 extension_count;
  const char **extensions;
  VkSurfaceKHR (*create_surface)(VkInstance, void *window);
  void *window;
  int width;
  int height;
} VulkanWindowInfo;

// TODO is this necessary as a swapchain member?
typedef struct {
  VkImage image;
  VkImageView image_view;
  VkDeviceMemory device_memory;
} DepthBuffer;

typedef struct {
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
} SwapchainStorage;

typedef struct {
  VkShaderModule module;
  VkShaderStageFlagBits stage;
  const char *entry_point;
} ShaderModule;

typedef struct {
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;

  // Populated by vkAcquireNextImageKHR so we can get the right framebuffer images.
  // TODO might want to get this out and pass around instead of caching.
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
  u8 current_frame_index;

  // TODO should these be per swapchain image or per frame in flight?
  VkCommandBuffer graphics_cmd_buffers[NUM_SWAPCHAIN_IMAGES];
  VkCommandBuffer compute_cmd_buffers[NUM_SWAPCHAIN_IMAGES];

  VkPipelineCache pipeline_cache;

  // Init
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  QueueFamilyIndices queue_family_indices;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceProperties physical_device_properties;
  VkDescriptorPool descriptor_pool;

  VkCommandPool graphics_cmd_pool;
  VkCommandPool compute_cmd_pool;
  VkCommandPool present_cmd_pool;
  VkCommandPool transient_cmd_pool;

  // These bools stored so we know to destroy the queue differently on teardown.
  bool compute_queue_index_is_different_than_graphics;
  bool present_queue_index_is_different_than_graphics;

  // Descriptor Set Layouts must outlive pipeline layouts, which need to outlive pipelines.
  // Layouts are only touched at creation and destruction.
  VkDescriptorSetLayout *descriptor_set_layouts;
  u32 num_descriptor_set_layouts;
} VulkanContext;

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkMemoryRequirements memory_requirements;
  VkMemoryPropertyFlags memory_property_flags;
} VulkanBuffer;

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
typedef struct {
  u32 current_offset;
} UniformBufferManager;

typedef struct {
  u32 offset;
  u32 size;
} UniformWrite;

typedef struct {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
} UniformBuffer;

typedef struct {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
} ReadOnlyStorageBuffer;

typedef struct {
  VulkanBuffer vulkan_buffer;
  u8 *mapped;
  u32 size;
} ReadWriteStorageBuffer;

// TODO this guy's on the chopping block.
typedef struct {
  VkViewport viewport;
  VkRect2D scissor;
} ViewportState;

typedef enum { BLEND_MODE_OPAQUE, BLEND_MODE_ALPHA } BlendMode;

typedef struct {
  VkShaderModule vertex_shader;
  VkShaderModule fragment_shader;
  u32 stage_count; // Reserving in case I add tesselation shaders in 20 years.
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state_create_info;
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPrimitiveTopology topology;
  VkBool32 primitive_restart_enabled; // TODO not sure what this is, maybe will rip out
  VkPolygonMode polygon_mode;
  VkCullModeFlags cull_mode;
  VkFrontFace front_face;
  VkSampleCountFlagBits sample_count_flag;
  BlendMode blend_mode;
  VkExtent2D swapchain_extent;
} PipelineConfig;

typedef enum {
  BUFFER_TYPE_STAGING,
  BUFFER_TYPE_VERTEX,
  BUFFER_TYPE_INDEX,
  BUFFER_TYPE_UNIFORM,
  BUFFER_TYPE_COHERENT_STREAMING,
  BUFFER_TYPE_READONLY_STORAGE,
  BUFFER_TYPE_READ_WRITE_STORAGE,
} BufferType;

typedef struct {
  VulkanBuffer buffer;
  u64 total_size;
  VkBuffer dst_buffers[MAX_COPY_REGIONS];
  VkBufferCopy copy_regions[MAX_COPY_REGIONS];
  u32 num_copy_regions;
  u32 offset;
} StagingArena;

typedef struct {
  VkImage image;
  u32 height;
  u32 width;
  VkDeviceMemory device_memory;
  VkImageView image_view;
} VulkanTexture;

typedef struct {
  // Data stuff
  u32 num_vertices;
  u32 instance_count;
  u32 num_indices;
  bool is_indexed;

  // Vulkan Stuff
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;

  u32 num_descriptor_sets;
  VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SETS];

  u32 num_vertex_buffers;
  VkDeviceSize vertex_buffer_offsets[MAX_VERTEX_BINDINGS];
  VkBuffer vertex_buffers[MAX_VERTEX_BINDINGS];

  VkDeviceSize index_buffer_offset;
  VkBuffer index_buffer;
} RenderCall;

typedef struct {
  u64 offset;
  u64 size;
  BufferType buffer_type;
  void *data;
} BufferHandle;

typedef struct {
  u64 vertex_buffer_offset;
  u64 index_buffer_offset;
  BufferHandle slices[MAX_BUFFER_UPLOADS];
  u32 num_slices;
} BufferUploadQueue;

typedef struct {
  VulkanContext *context;
  VulkanBuffer vertex_buffer;
  VulkanBuffer index_buffer;
  StagingArena staging_arena;
} BufferManager;

typedef struct {
  VkRenderPass render_pass;
  VkFramebuffer framebuffer;

  VkImage color_image;
  VkDeviceMemory color_image_device_memory;
  VkImageView color_image_view;

  VkImage depth_image;
  VkDeviceMemory depth_image_device_memory;
  VkImageView depth_image_view;
} ColorDepthFramebuffer;

typedef struct {
  u32 num_vertices;
  u32 instance_count;
  u32 num_indices;

  u32 num_vertex_buffers;
  VkBuffer vertex_buffers[MAX_VERTEX_BINDINGS];
  VkDeviceSize vertex_buffer_offsets[MAX_VERTEX_BINDINGS];

  VkDeviceSize index_buffer_offset;
  VkBuffer index_buffer;
} VulkanMesh;

typedef struct {
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SETS];
  u32 num_descriptor_sets;
  const VkWriteDescriptorSet *descriptor_set_writes[MAX_DESCRIPTOR_SETS];
  u32 descriptor_set_write_lens[MAX_DESCRIPTOR_SETS];
} VulkanMaterial;

////////////////////////////////////////////////////////////////
//////////////////////// PUBLIC API ////////////////////////////
////////////////////////////////////////////////////////////////

// Init/destroy
VulkanContext create_vulkan_context(const char *title, VulkanWindowInfo window_info);
void destroy_vulkan_context(VulkanContext *);

// Buffers
VulkanBuffer create_buffer_explicit(
    const VulkanContext *ctx, VkBufferUsageFlags usage, VkDeviceSize size, VkMemoryPropertyFlags properties
);
VulkanBuffer create_buffer(const VulkanContext *ctx, BufferType buffer_type, VkDeviceSize size);
void destroy_vulkan_buffer(const VulkanContext *ctx, VulkanBuffer buffer);
void write_to_vulkan_buffer(
    VulkanContext *ctx, const void *src_data, VkDeviceSize size, VkDeviceSize offset, VulkanBuffer vulkan_buffer
);

BufferUploadQueue create_buffer_upload_queue();
const BufferHandle *upload_data(BufferUploadQueue *queue, BufferType buffer_type, void *data, u64 size);
BufferManager flush_buffers(VulkanContext *ctx, BufferUploadQueue *queue);
void destroy_buffer_manager(BufferManager *buffer_manager);

// These macros are specifically for arrays, e.g., f32 array[], with the [];
// They will fail on pointers
#define UPLOAD_VERTEX_ARRAY(queue, array) (upload_data(&queue, BUFFER_TYPE_VERTEX, (void *)array, sizeof(array)))
#define UPLOAD_INDEX_ARRAY(queue, array) (upload_data(&queue, BUFFER_TYPE_INDEX, (void *)array, sizeof(array)))

// Materials
void destroy_vulkan_material(VkDevice device, VulkanMaterial *mat);

// Rendering APIs
void render_mesh(VkCommandBuffer command_buffer, RenderCall *render_call);
void render_mesh_material(VkCommandBuffer cmd, const VulkanMesh *mesh, const VulkanMaterial *mat);

// Command buffers
VkCommandBuffer begin_single_use_command_buffer(const VulkanContext *ctx);
void end_single_use_command_buffer(const VulkanContext *ctx, VkCommandBuffer command_buffer);

// Shaders
VkShaderModule create_shader_module(VkDevice device, const u32 *code, u32 code_size);

// Pipelines
VkPipeline create_graphics_pipeline(VkDevice device, const PipelineConfig *config, VkPipelineCache pipeline_cache);

VkPipeline create_default_graphics_pipeline(
    const VulkanContext *ctx,
    VkRenderPass render_pass,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
    VkPipelineLayout pipeline_layout
);

VkPipelineLayout
create_pipeline_layout(VkDevice device, const VkDescriptorSetLayout *descriptor_set_layouts, u32 set_layout_count);

// Per Frame Commands
void begin_frame(VulkanContext *ctx);
VkCommandBuffer begin_command_buffer(const VulkanContext *ctx);
void update_frame_index(VulkanContext *ctx);
void end_frame(VulkanContext *ctx, VkCommandBuffer cmd);
void submit_and_present(const VulkanContext *ctx, VkCommandBuffer command_buffer);

ViewportState create_viewport_state_offset(VkExtent2D swapchain_extent, VkOffset2D offset);
ViewportState create_viewport_state_xy(VkExtent2D swapchain_extent, u32 x, u32 y);

// Render passes
void begin_render_pass(
    const VulkanContext *ctx,
    VkCommandBuffer command_buffer,
    VkRenderPass render_pass,
    VkFramebuffer framebuffer,
    const VkClearValue *clear_value,
    u32 clear_value_count,
    ViewportState viewport_state
);

VkRenderPass create_render_pass(
    VkDevice device,
    u32 num_attachment_descriptions,
    const VkAttachmentDescription *attachment_descriptions,
    u32 num_subpass_descriptions,
    const VkSubpassDescription *subpass_descriptions,
    u32 num_dependencies,
    const VkSubpassDependency *dependencies
);

VkRenderPass create_color_depth_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format);
VkRenderPass create_color_render_pass(VkDevice device, VkFormat format);

// Uniforms
UniformBuffer create_uniform_buffer(const VulkanContext *ctx, u32 buffer_size);
void destroy_uniform_buffer(const VulkanContext *ctx, UniformBuffer *uniform_buffer);
void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data, UniformWrite uniform_write);
UniformBufferManager create_uniform_buffer_manager();
UniformWrite push_uniform(UniformBufferManager *uniform_buffer_manager, u32 size);

// Vertex Buffers
VkVertexInputBindingDescription create_instanced_vertex_binding_description(u32 binding, u32 stride);
VkVertexInputBindingDescription create_vertex_binding_description(u32 binding, u32 stride);

u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties);

// Descriptor Sets
void set_descriptor_set_layouts(VulkanContext *ctx, VkDescriptorSetLayout *layouts, u32 num_layouts);
void reset_descriptor_set_layouts(VulkanContext *ctx);
VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutBinding *bindings, u32 binding_count);
VkDescriptorSet
create_descriptor_set(VkDevice device, const VkDescriptorSetLayout *set_layouts, VkDescriptorPool descriptor_pool);

// Framebuffers
// Generally need a better understanding of lifetime management/ownership for these
VkFormat find_depth_format(VkPhysicalDevice physical_device);

VkFramebuffer create_framebuffer(
    VkDevice device,
    VkRenderPass render_pass,
    u32 num_attachments,
    VkImageView *image_view_attachments,
    VkExtent2D extent
);

// TODO Backend only?
ColorDepthFramebuffer
create_color_depth_framebuffer(const VulkanContext *ctx, VkExtent2D extent, VkFormat color_fmt, VkFormat depth_fmt);
void destroy_color_depth_framebuffer(VkDevice device, ColorDepthFramebuffer *color_depth_framebuffer);

// Images/Textures
// Want to have this transitioning business be entirely backend
void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

VulkanTexture create_vulkan_texture(
    VulkanContext *ctx,
    u32 width,
    u32 height,
    u32 n_channels,
    u8 *data,
    VulkanBuffer staging_buffer,
    void *ptr_to_mapped_memory
);

void destroy_vulkan_texture(VkDevice device, VulkanTexture *vulkan_texture);
VkSampler create_sampler(VkDevice device);

// TODO remove when a better system for automating these writes is in place
static inline VkWriteDescriptorSet fill_write(const VulkanMaterial *mat, u32 set_idx, u32 binding) {
  u32 len = mat->descriptor_set_write_lens[set_idx];
  const VkWriteDescriptorSet *templates = mat->descriptor_set_writes[set_idx];
  for (u32 i = 0; i < len; i++) {
    if (templates[i].dstBinding == binding) {
      VkWriteDescriptorSet w = templates[i];
      w.dstSet = mat->descriptor_sets[set_idx];
      return w;
    }
  }
  assert(false);
  return {};
}
