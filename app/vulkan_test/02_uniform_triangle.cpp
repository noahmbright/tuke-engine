#include "generated_shader_utils.h"

#include "shaders.h"
#include "vulkan_test_common.h"
#include "window.h"

TriangleTransformation simulate(f64 t) {
  TriangleTransformation tt;
  tt.mat[0][0] = sinf(t);
  tt.mat[0][1] = cosf(t);
  tt.mat[1][0] = cosf(t);
  tt.mat[1][1] = -sinf(t);
  return tt;
}

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  UniformBuffer ubs[MAX_FRAMES_IN_FLIGHT];
  VkDescriptorBufferInfo uniform_writes[MAX_FRAMES_IN_FLIGHT];
  VulkanMaterial mats[MAX_FRAMES_IN_FLIGHT];

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // Made a mistake with the wrong upload size here - used the wrong struct in the name.
    UniformBufferManager ub_manager = create_uniform_buffer_manager();
    VkDescriptorBufferInfo *uw = push_uniform(&ub_manager, sizeof(TriangleTransformation));
    ubs[i] = finalize_ub(&t.ctx, &ub_manager);
    uniform_writes[i] = *uw;

    init_program_spec(&t.ctx, t.rp, &common_uniform_bringup_program_spec, &mats[i]);
    // Know binding, descriptor count, and type from generated layout bindings - from reflection
    // These are needed for a write to particular buffer - runtime info.
    // This write stuff should all be per descriptor in the material
    DescriptorWrite write = {
        .set_id = LAYOUT_ID_TRIANGLE_TRANSFORMATION, .binding = 0, .buffer_info = uniform_writes[i]
    };
    update_vulkan_material(&t.ctx, &write, 1, &mats[i]);
  }

  VulkanMesh mesh = {.vertex_count = 3, .instance_count = 1};

  f64 t_total = 0;
  f64 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    f64 t1 = glfwGetTime();
    f64 dt = t1 - t0;
    t0 = t1;
    t_total += dt;

    TriangleTransformation tt = simulate(t_total);

    begin_frame(&t.ctx);
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    // Render passes need begun/ended. Can have multiple per frame.
    VkFramebuffer fb = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, fb, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    // This is a single draw action. Probably want some scheme for queuing this in the context
    u32 fi = t.ctx.current_frame_index;
    write_to_uniform_buffer(&ubs[fi], &tt, uniform_writes[fi]);
    render_mesh_material(cmd, &mesh, &mats[fi]);
    vkCmdEndRenderPass(cmd);

    // end_frame() should empty that queue.
    // This queue filling/emptying wraps the command buffer filling and frame state management
    // with the correct buffers for this frame index.
    end_frame(&t.ctx, cmd);
  }

  vkDeviceWaitIdle(t.ctx.device);
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_uniform_buffer(&t.ctx, &ubs[i]);
    destroy_vulkan_material(t.ctx.device, &mats[i]);
  }

  destroy_vulkan_test(&t);
  return 0;
}
