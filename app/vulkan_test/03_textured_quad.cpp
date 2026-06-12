#include "generated_shader_utils.h"

#include "glfw_vulkan.h"
#include "shaders.h"
#include "vulkan_base.h"
#include "window.h"

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);
  VulkanContext ctx = create_vulkan_context("Test 02: Uniform Triangle", window_info);

  VkDescriptorSetLayout layouts[NUM_DESCRIPTOR_SET_LAYOUTS];
  set_descriptor_set_layouts(&ctx, layouts, NUM_DESCRIPTOR_SET_LAYOUTS);

  VkFormat depth_format = find_depth_format(ctx.physical_device);
  VkRenderPass rp = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, depth_format);

  // Load texture
  STBHandle stb = load_texture("textures/brickwall.jpg");
  u32 size = stb.width * stb.height * stb.n_channels;
  VulkanBuffer staging_buffer = create_buffer(&ctx, BUFFER_TYPE_STAGING, size);
  void *texture_data;
  VkResult result =
      vkMapMemory(ctx.device, staging_buffer.memory, 0, staging_buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  VulkanTexture texture =
      create_vulkan_texture(&ctx, stb.width, stb.height, stb.n_channels, stb.data, staging_buffer, texture_data);
  free_stb_handle(&stb);
  vkUnmapMemory(ctx.device, staging_buffer.memory);
  destroy_vulkan_buffer(&ctx, staging_buffer);

  VulkanMaterial mat;
  init_program_spec(&ctx, rp, &common_textured_quad_bringup_program_spec, &mat);

  VkSampler sampler = create_sampler(ctx.device);
  VkDescriptorImageInfo image_info = {
      .sampler = sampler,
      .imageView = texture.image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet writes = fill_write(&mat, 0, 0);
  writes.pImageInfo = &image_info;
  vkUpdateDescriptorSets(ctx.device, 1, &writes, 0, NULL);

  VulkanMesh mesh = {
      .num_vertices = 6,
      .instance_count = 1,
  };

  // Main loop
  ViewportState viewport_state = create_viewport_state_xy(ctx.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {
      {.color = {{0.10, 0.01, 0.10, 1.0}}},
      {
          .depthStencil = {.depth = 1.0f, .stencil = 0},
      }
  };

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    // Render
    begin_frame(&ctx);
    VkCommandBuffer cmd = begin_command_buffer(&ctx);

    VkFramebuffer fb = ctx.framebuffers[ctx.image_index];
    begin_render_pass(&ctx, cmd, rp, fb, clear_values, NUM_ATTACHMENTS, viewport_state);
    render_mesh_material(cmd, &mesh, &mat);
    vkCmdEndRenderPass(cmd);

    end_frame(&ctx, cmd);
  }

  vkDeviceWaitIdle(ctx.device);
  vkDestroyRenderPass(ctx.device, rp, NULL);
  vkDestroySampler(ctx.device, sampler, NULL);
  destroy_vulkan_material(ctx.device, &mat);
  destroy_vulkan_texture(ctx.device, &texture);
  destroy_vulkan_context(&ctx);

  return 0;
}
