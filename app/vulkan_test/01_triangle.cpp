#include "c_reflector_bringup.h"
#include "generated_shader_utils.h"
#include "glfw_vulkan.h"
#include "tuke_engine.h"
#include "vulkan/vulkan_base.h"
#include "window.h"

int main() {
  // I'd like if this were condensed and window system agnostic.
  // I'd like my own Window type that can tell vulkan which extensions to use.
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanWindowInfo window_info = create_glfw_vulkan_window_info(window);
  VulkanContext context = create_vulkan_context("Tuke", window_info);

  // All this should be in the backend.
  // I'd like to be able to specify a clear color from the application.
  // I need to make this hot reload amenable.
  // I want cleaner enums from the reflection system. That stuff is all helpful but verbose.
  //    Also my ego doesn't want to throw the whole thing away :(
  init_generated_shader_vk_modules(context.device);
  ViewportState viewport_state = create_viewport_state_xy(context.swapchain_extent, 0, 0);
  const VkClearValue clear_values[NUM_ATTACHMENTS] = {{.color = {{0.01, 0.01, 0.01, 1.0}}},
                                                      {.depthStencil = {.depth = 1.0f, .stencil = 0}}};
  // VkDescriptorPool descriptor_pool = create_descriptor_pool(context.device, generated_pool_sizes, pool_size_count,
  // max_descriptor_sets);

  u32 num_layouts = 0;
  VkPipelineLayout pipeline_layout = create_pipeline_layout(context.device, NULL, num_layouts);

  // TODO actual design decision - dynamic rendering and removing render passes
  // Why do framebuffers own render passes?
  // Should I do a slang/wgsl like thing in the reflector where I can just write both shaders
  // in one file?
  VkPipeline triangle_pipeline =
      shader_handles_to_graphics_pipeline(&context, context.render_pass, SHADER_HANDLE_COMMON_HARDCODED_TRIANGLE_VERT,
                                          SHADER_HANDLE_COMMON_HARDCODED_TRIANGLE_FRAG, pipeline_layout);

  // TODO this is too much state to wire up manually every time, especially when there
  // are descriptors (which is the common case).
  RenderCall render_call = {
      .num_vertices = 3,
      .instance_count = 1,
      .graphics_pipeline = triangle_pipeline,
      .pipeline_layout = pipeline_layout,
      .is_indexed = false,
  };

  // TODO actual todo - want swapchain recreation and resizeable window in this test.
  u64 current_frame = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // TODO need this understood better. Need to address why frame start failure
    // would happen. Need to handle different cases (maybe). Also should I handle this
    // in the backend?
    // TODO sync around here.
    begin_frame(&context);

    // This loop is too complicated to wire manually. I forget to end render passes
    // and command buffers in the right order.
    // All of these are raw vulkan calls that belong in the backend.
    VkCommandBuffer command_buffer = begin_command_buffer(&context);
    begin_render_pass(&context, command_buffer, context.render_pass, context.framebuffers[context.image_index],
                      clear_values, NUM_ATTACHMENTS, viewport_state.scissor.offset);

    vkCmdSetViewport(command_buffer, 0, 1, &viewport_state.viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_state.scissor);

    render_mesh(command_buffer, &render_call);
    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");
    submit_and_present(&context, command_buffer);

    current_frame++;
    context.current_frame_index = current_frame % MAX_FRAMES_IN_FLIGHT;
  }

  return 0;
}
