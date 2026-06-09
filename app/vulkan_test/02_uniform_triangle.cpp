#include "generated_shader_utils.h"

#include "glfw_vulkan.h"
#include "glm/ext/matrix_transform.hpp"
#include "shaders.h"
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
      {.color = {{0.10, 0.01, 0.10, 1.0}}},
      {
          .depthStencil = {.depth = 1.0f, .stencil = 0},
      }
  };

  // This is a first step in seeing what I need to move to get render pass out of the context
  // Don't like needing to call find_depth_format() here. Considering moving inside the create()
  VkFormat depth_format = find_depth_format(ctx.physical_device);
  VkRenderPass rp = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, depth_format);

  // Made a mistake with the wrong upload size here - used the wrong struct in the name.
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite handle = push_uniform(&ub_manager, sizeof(TriangleTransformation));
  UniformBuffer ubs[MAX_FRAMES_IN_FLIGHT];
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    ubs[i] = create_uniform_buffer(&ctx, ub_manager.current_offset);
  }

  VkDescriptorSetLayout layouts[NUM_DESCRIPTOR_SET_LAYOUTS];
  set_descriptor_set_layouts(&ctx, layouts, NUM_DESCRIPTOR_SET_LAYOUTS);

  VulkanMaterial mats[MAX_FRAMES_IN_FLIGHT];
  // Know binding, descriptor count, and type from generated layout bindings - from reflection
  // These are needed for a write to particular buffer - runtime info.
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    init_program_spec(&ctx, rp, &common_uniform_bringup_program_spec, &mats[i]);

    // This write stuff indexed by 0 should all be per descriptor in the material
    VkWriteDescriptorSet writes[MAX_DESCRIPTOR_SETS];
    u32 write_count = mats[i].descriptor_set_write_lens[0];
    memcpy(writes, mats[i].descriptor_set_writes[0], write_count * sizeof(VkWriteDescriptorSet));
    writes[0].dstSet = mats[i].descriptor_sets[0];

    VkDescriptorBufferInfo descriptor_buffer_info = {
        .buffer = ubs[i].vulkan_buffer.buffer,
        .offset = handle.offset,
        .range = handle.size,
    };
    writes[0].pBufferInfo = &descriptor_buffer_info;

    u32 copy_count = 0;
    vkUpdateDescriptorSets(ctx.device, write_count, writes, copy_count, NULL);
  }

  VulkanMesh mesh = {
      .num_vertices = 3,
      .instance_count = 1,
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

    // This is a single draw action. Probably want some scheme for queuing this in the context
    write_to_uniform_buffer(&ubs[ctx.current_frame_index], &tt, handle);
    render_mesh_material(cmd, &mesh, &mats[ctx.current_frame_index]);
    vkCmdEndRenderPass(cmd);

    // end_frame() should empty that queue.
    // This queue filling/emptying wraps the command buffer filling and frame state management
    // with the correct buffers for this frame index.
    end_frame(&ctx, cmd);
  }

  vkDeviceWaitIdle(ctx.device);
  vkDestroyRenderPass(ctx.device, rp, NULL);
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_uniform_buffer(&ctx, &ubs[i]);
    destroy_vulkan_material(ctx.device, &mats[i]);
  }

  destroy_vulkan_context(&ctx);

  return 0;
}
