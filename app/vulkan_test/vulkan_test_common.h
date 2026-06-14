#include "shaders.h"
#include "tuke_engine.h"

#include "glfw_vulkan.h"
#include "vulkan/vulkan_base.h"

typedef struct {
  VulkanContext ctx;
  ViewportState viewport_state;
  VkRenderPass rp;
  VkClearValue clear_values[NUM_ATTACHMENTS];
  VkDescriptorSetLayout layouts[NUM_DESCRIPTOR_SET_LAYOUTS];
} VulkanTest;

static inline VulkanTest init_vulkan_test(GLFWwindow *window) {
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);
  VulkanContext ctx = create_vulkan_context("Test 02: Uniform Triangle", window_info);
  ViewportState viewport_state = create_viewport_state_xy(ctx.swapchain_extent, 0, 0);
  VkRenderPass rp = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, ctx.depth_format);

  VulkanTest test = {
      .ctx = ctx,
      .viewport_state = viewport_state,
      .rp = rp,
      .clear_values = {{.color = {{0.10, 0.01, 0.10, 1.0}}}, {.depthStencil = {.depth = 1.0f, .stencil = 0}}},
  };

  set_descriptor_set_layouts(&test.ctx, test.layouts, NUM_DESCRIPTOR_SET_LAYOUTS);
  return test;
}

static inline void destroy_vulkan_test(VulkanTest *t) {
  vkDestroyRenderPass(t->ctx.device, t->rp, NULL);
  destroy_vulkan_context(&t->ctx);
}

// clang-format off
static const f32 triangle_vertices[] = {
  0.0f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 
  0.5f, 0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.0f,  0.0f, 0.0f, 1.0f
};

static const f32 square_vertices[] = {
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
};

// TL, BL, BR, TR
static const f32 unit_square_positions[] = {
   // x, y, z, u, v
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

static const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};

static const f32 quad_positions[] = {
  -0.5, -0.5
  -0.1, -0.1
  -0.1, -0.1
  -0.3, -0.3
  -0.3, -0.3
};

static const f32 cube_vertices[] = {
    // Front face (+Z)
    // xyz,              nx, ny, nz, u, v
    -0.5f, -0.5f,  0.5f, 0, 0, 1, 0, 0,
     0.5f, -0.5f,  0.5f, 0, 0, 1, 1, 0,
     0.5f,  0.5f,  0.5f, 0, 0, 1, 1, 1,

    -0.5f, -0.5f,  0.5f, 0, 0, 1, 0, 0,
     0.5f,  0.5f,  0.5f, 0, 0, 1, 1, 1,
    -0.5f,  0.5f,  0.5f, 0, 0, 1, 0, 1,

     //Back face (-Z)
     0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0,
    -0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0,
    -0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 1,

     0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0,
    -0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 1,
     0.5f,  0.5f, -0.5f, 0, 0, -1, 0, 1,

     //Left face (-X)
    -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0,
    -0.5f, -0.5f,  0.5f, -1, 0, 0, 1, 0,
    -0.5f,  0.5f,  0.5f, -1, 0, 0, 1, 1,

    -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0,
    -0.5f,  0.5f,  0.5f, -1, 0, 0, 1, 1,
    -0.5f,  0.5f, -0.5f, -1, 0, 0, 0, 1,

     //Right face (+X)
     0.5f, -0.5f,  0.5f, 1, 0, 0, 0, 0,
     0.5f, -0.5f, -0.5f, 1, 0, 0, 1, 0,
     0.5f,  0.5f, -0.5f, 1, 0, 0, 1, 1,

     0.5f, -0.5f,  0.5f, 1, 0, 0, 0, 0,
     0.5f,  0.5f, -0.5f, 1, 0, 0, 1, 1,
     0.5f,  0.5f,  0.5f, 1, 0, 0, 0, 1,

     //Top face (+Y)
    -0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0,
     0.5f,  0.5f,  0.5f, 0, 1, 0, 1, 0,
     0.5f,  0.5f, -0.5f, 0, 1, 0, 1, 1,

    -0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0,
     0.5f,  0.5f, -0.5f, 0, 1, 0, 1, 1,
    -0.5f,  0.5f, -0.5f, 0, 1, 0, 0, 1,

     //Bottom face (-Y)
    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0,
     0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 0,
     0.5f, -0.5f,  0.5f, 0, -1, 0, 1, 1,

    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0,
     0.5f, -0.5f,  0.5f, 0, -1, 0, 1, 1,
    -0.5f, -0.5f,  0.5f, 0, -1, 0, 0, 1,
};

static const f32 f_vertices[] = {
    // Left column
    0.0, 0.0, 0.0,
    0.1, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.1, 1.0, 0.0,

    // Top rung
    0.1, 0.0, 0.0,
    0.5, 0.0, 0.0,
    0.1, 0.1, 0.0,
    0.5, 0.1, 0.0,

    // Middle rung 
    0.1, 0.4, 0.0,
    0.3, 0.4, 0.0,
    0.1, 0.5, 0.0,
    0.3, 0.5, 0.0,
};

static const u32 num_f_vertices = ARRAY_SIZE(f_vertices) / 3;

static const u16 f_indices[] = {
    0, 1, 2,   2, 1,  3,
    4, 5, 6,   6, 5,  7,
    8, 9, 10, 10, 9, 11,
};

static const u32 num_f_indices = ARRAY_SIZE(f_indices);
// clang-format on
