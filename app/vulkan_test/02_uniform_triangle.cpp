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
  VulkanContext ctx = create_vulkan_context("Test 02: Uniform Triangle", window_info);

  ViewportState viewport_state = create_viewport_state_xy(ctx.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {
      {.color = {{0.01, 0.01, 0.01, 1.0}}}, {.depthStencil = {.depth = 1.0f, .stencil = 0}}
  };

  // This is a first step in seeing what I need to move to get render pass out of the context
  // Don't like needing to call find_depth_format() here. Considering moving inside the create()
  VkFormat depth_format = find_depth_format(ctx.physical_device);
  VkRenderPass rp = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, depth_format);

  VkDescriptorPool descriptor_pool =
      create_descriptor_pool(ctx.device, generated_pool_sizes, pool_size_count, max_descriptor_sets);

  // Made a mistake with the wrong upload size here - used the wrong struct in the name.
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite handle = push_uniform(&ub_manager, sizeof(TriangleTransformation));
  UniformBuffer ub = create_uniform_buffer(&ctx, ub_manager.current_offset);

  // REAL FOCUS Descriptor Sets
  const ProgramSpec program = common_uniform_bringup_program_spec;
  VkDescriptorSetLayout descriptor_set_layout =
      create_descriptor_set_layout(ctx.device, program.binding_lists[0], program.binding_list_lens[0]);
  VkDescriptorSet descriptor_set = create_descriptor_set(ctx.device, &descriptor_set_layout, descriptor_pool);

  // Know binding, descriptor count, and type from generated layout bindings - from reflection
  VkWriteDescriptorSet *write = TRIANGLE_TRANSFORMATION_write_templates;
  write->dstSet = descriptor_set;

  // These are needed for a write to particular buffer - runtime info.
  VkDescriptorBufferInfo descriptor_buffer_info = {
      .buffer = ub.vulkan_buffer.buffer,
      .offset = handle.offset,
      .range = handle.size,
  };
  write->pBufferInfo = &descriptor_buffer_info;

  u32 write_count = 1;
  u32 copy_count = 0;
  vkUpdateDescriptorSets(ctx.device, write_count, write, copy_count, NULL);

  u32 num_desc_set_layouts = 1;
  VkPipelineLayout pipeline_layout = create_pipeline_layout(ctx.device, &descriptor_set_layout, num_desc_set_layouts);
  // END FOCUS

  VkPipeline pipeline = shader_handles_to_graphics_pipeline(
      &ctx, rp, SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_VERT, SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_FRAG, pipeline_layout
  );

  // Still don't like configuring these manually. This is like the fundamental issue.
  // Made a mistake in configuring descriptor sets.
  // Should have descriptor stuff lumped together.
  RenderCall render_call = {
      .num_vertices = 3,
      .instance_count = 1,
      .graphics_pipeline = pipeline,
      .pipeline_layout = pipeline_layout,
      .num_descriptor_sets = 1,
      .descriptor_sets = {descriptor_set},
      .is_indexed = false,
  };

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

  vkDeviceWaitIdle(ctx.device);

  vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
  vkDestroyRenderPass(ctx.device, rp, NULL);
  vkDestroyDescriptorSetLayout(ctx.device, descriptor_set_layout, NULL);
  vkDestroyPipeline(ctx.device, pipeline, NULL);
  destroy_uniform_buffer(&ctx, &ub);
  vkDestroyDescriptorPool(ctx.device, descriptor_pool, NULL);

  destroy_vulkan_context(&ctx);

  return 0;
}
