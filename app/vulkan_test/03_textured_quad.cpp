#include "generated_shader_utils.h"

#include "shaders.h"
#include "tuke_engine.h"
#include "vulkan_test_common.h"
#include "window.h"

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  // Load texture
  STBHandle stb = load_texture("textures/brickwall.jpg");
  u32 size = stb.width * stb.height * stb.n_channels;
  VulkanBuffer staging_buffer = create_buffer(&t.ctx, BUFFER_TYPE_STAGING, size);
  void *texture_data;
  VkResult result =
      vkMapMemory(t.ctx.device, staging_buffer.memory, 0, staging_buffer.memory_requirements.size, 0, &texture_data);
  VK_CHECK(result, "Failed to map staging buffer memory");

  VulkanTexture texture =
      create_vulkan_texture(&t.ctx, stb.width, stb.height, stb.n_channels, stb.data, staging_buffer, texture_data);
  free_stb_handle(&stb);
  vkUnmapMemory(t.ctx.device, staging_buffer.memory);
  destroy_vulkan_buffer(&t.ctx, staging_buffer);

  VulkanMaterial mat;
  init_program_spec(&t.ctx, t.rp, &common_textured_quad_bringup_program_spec, &mat);

  DescriptorWrite writes[] = {
      {
          .set_id = LAYOUT_ID_TEXTURED_QUAD_BRINGUP,
          .binding = 0,
          .image_info = {
              .sampler = t.ctx.samplers[SAMPLER_LINEAR_CLAMP],
              .imageView = texture.image_view,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
          },
      },
  };

  update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &mat);

  VulkanMesh mesh = {
      .vertex_count = 6,
      .instance_count = 1,
  };

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    // Render
    begin_frame(&t.ctx);
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    VkFramebuffer fb = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, fb, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);
    render_mesh_material(cmd, &mesh, &mat);
    vkCmdEndRenderPass(cmd);

    end_frame(&t.ctx, cmd);
  }

  vkDeviceWaitIdle(t.ctx.device);
  destroy_vulkan_material(t.ctx.device, &mat);
  destroy_vulkan_texture(t.ctx.device, &texture);
  destroy_vulkan_test(&t);

  return 0;
}
