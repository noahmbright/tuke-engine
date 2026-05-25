#include "c_reflector_bringup.h"
#include "generated_shader_utils.h"
#include "glfw_vulkan.h"
#include "glm/ext/matrix_transform.hpp"
#include "tuke_engine.h"
#include "vulkan/vulkan_base.h"
#include "window.h"
#include <vulkan/vulkan_core.h>

TriangleTransformation simulate(f64 t) {
  TriangleTransformation tt;
  tt.mat = glm::rotate(glm::mat4(1.0f), (f32)t, glm::vec3(0.0f, 0.0f, 1.0f));
  return tt;
}

int main() {
  // Init woes documented in 01_triangle
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);
  VulkanContext ctx = create_vulkan_context("Tuke", window_info);
  init_generated_shader_vk_modules(ctx.device);
  ViewportState viewport_state = create_viewport_state_xy(ctx.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {
      {.color = {{0.01, 0.01, 0.01, 1.0}}}, {.depthStencil = {.depth = 1.0f, .stencil = 0}}
  };

  // This is a first step in seeing what I need to move to get render pass out of the context
  // Don't like needing to call find_depth_format() here. Considering moving inside the create()
  VkFormat depth_format = find_depth_format(ctx.physical_device);
  VkRenderPass rp = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, depth_format);

  // Made a mistake with the wrong upload size here - used the wrong struct in the name.
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite handle = push_uniform(&ub_manager, sizeof(TriangleTransformation));
  UniformBuffer ub = create_uniform_buffer(&ctx, ub_manager.current_offset);

  // This is so awkward to use
  DescriptorSetBuilder set_builder = create_descriptor_set_builder(&ctx);
  add_uniform_buffer_descriptor_set(
      &set_builder, &ub, handle.offset, handle.size, 0 /*binding*/, 1 /*count*/, VK_SHADER_STAGE_VERTEX_BIT,
      false /* dynamic */
  );

  // Not sure about this. Feels verbose
  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(ctx.device, generated_pool_sizes, pool_size_count, max_descriptor_sets);
  DescriptorSetHandle descriptor = build_descriptor_set(&set_builder, descriptor_pool);

  // Questioning if my descriptor set thing can be more transparent.
  // Forgot about how it owns a layout
  u32 num_desc_set_layouts = 1;
  VkPipelineLayout pipeline_layout =
      create_pipeline_layout(ctx.device, &descriptor.descriptor_set_layout, num_desc_set_layouts);

  VkPipeline pipeline = shader_handles_to_graphics_pipeline(
      &ctx, rp, SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_VERT, SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_FRAG, pipeline_layout
  );

  // Still don't like configuring these manually. This is like the fundamental issue.
  // Made a mistake in configuring descriptor sets.
  // Should have descriptr stuff lumped together.
  // What is the right abstraction?
  RenderCall render_call = {
      .num_vertices = 3,
      .instance_count = 1,
      .graphics_pipeline = pipeline,
      .pipeline_layout = pipeline_layout,
      .num_descriptor_sets = 1,
      // Same issue with not liking the descriptor's transparency
      .descriptor_sets = {descriptor.descriptor_set},
      .is_indexed = false,
  };

  // Where is a typical place to manage time? This is probably fine. The "game" is this file,
  // and we have a file global t_total. Not so different from throwing this into a game state
  // struct.
  f64 t_total = 0;
  f64 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    // Input
    glfwPollEvents();
    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;
    t_total += dt;

    // Simulate
    TriangleTransformation tt = simulate(t_total);

    // Render
    begin_frame(&ctx);
    VkCommandBuffer cmd = begin_command_buffer(&ctx);

    // Render passes need begun/ended. Can have multiple per frame.
    VkFramebuffer fb = ctx.framebuffers[ctx.image_index];
    begin_render_pass(&ctx, cmd, rp, fb, clear_values, NUM_ATTACHMENTS, viewport_state);

    // Probably need an array of pointers to per frame data in the VulkanContext.
    // Maybe the app could own. Just put pointers in the context for flexibility.
    // These are the things that go between begin and end frame()

    // This is a single draw action. Probably want some scheme for queuing this in the context
    write_to_uniform_buffer(&ub, &tt, handle);
    render_mesh(cmd, &render_call);
    vkCmdEndRenderPass(cmd);

    // end_frame() should empty that queue.
    // This queue filling/emptying wraps the command buffer filling and frame state management
    // with the correct buffers for this frame index.
    end_frame(&ctx, cmd);
  }

  return 0;
}
