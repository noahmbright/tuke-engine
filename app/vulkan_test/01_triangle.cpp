#include "generated_shader_utils.h"
#include "shaders.h"

#include "vulkan_test_common.h"
#include "window.h"

int main() {
  // I'd like my own Window type that can tell vulkan which extensions to use.
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  // Dynamic rendering and removing render passes
  // Why do framebuffers own render passes?
  VulkanMesh mesh = {
      .vertex_count = 3,
  };

  VulkanMaterial mat;
  init_program_spec(&t.ctx, t.rp, NULL, &common_hardcoded_triangle_program_spec, &mat);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Need this understood better. Need to address why frame start failure would happen.
    // sync around here, this whole while loop.
    // would like this to read a little bit more functionally. Need to keep state tracking robust.
    begin_frame(&t.ctx);

    // do we want to have begin_frame() return a command buffer for this frame?
    // That should all be handled internally. Maybe we need one big function to
    // do all drawing, using all data to draw in this frame.
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    // If we had textures and depth buffers, we'd want to transition their formats here.
    // Right now, render passes are baked into the context. Would need to rip that out
    // to better handle transitions through memory barriers
    // https://howtovulkan.com/#record-command-buffer

    // Recording commands can be done in another thread. Need to wire that around here.
    VkFramebuffer framebuffer = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, framebuffer, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    render_mesh(cmd, &mesh, &mat);
    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    submit_and_present(&t.ctx, cmd);
    update_frame_index(&t.ctx);
  }

  VkResult result = vkDeviceWaitIdle(t.ctx.device);
  VK_CHECK(result, "Failed to wait idle");
  destroy_vulkan_material(t.ctx.device, &mat);
  destroy_vulkan_test(&t);
  return 0;
}
