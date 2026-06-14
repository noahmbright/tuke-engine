#include "generated_shader_utils.h"

#include "glm/ext/matrix_transform.hpp"
#include "shaders.h"
#include "vulkan_test_common.h"
#include "window.h"

TriangleTransformation simulate(f64 t) {
  TriangleTransformation tt;
  tt.mat = glm::rotate(glm::mat4(1.0f), (f32)t, glm::vec3(0.0f, 0.0f, 1.0f));
  return tt;
}

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  // Made a mistake with the wrong upload size here - used the wrong struct in the name.
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite uniform_write = push_uniform(&ub_manager, sizeof(TriangleTransformation));
  UniformBuffer ubs[MAX_FRAMES_IN_FLIGHT];
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    ubs[i] = create_uniform_buffer(&t.ctx, ub_manager.current_offset);
  }

  VulkanMaterial mats[MAX_FRAMES_IN_FLIGHT];
  // Know binding, descriptor count, and type from generated layout bindings - from reflection
  // These are needed for a write to particular buffer - runtime info.
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    init_program_spec(&t.ctx, t.rp, &common_uniform_bringup_program_spec, &mats[i]);

    // This write stuff indexed by 0 should all be per descriptor in the material
    VkWriteDescriptorSet writes[MAX_DESCRIPTOR_SETS];
    u32 write_count = mats[i].descriptor_set_write_lens[0];
    memcpy(writes, mats[i].descriptor_set_writes[0], write_count * sizeof(VkWriteDescriptorSet));
    writes[0].dstSet = mats[i].descriptor_sets[0];

    VkDescriptorBufferInfo descriptor_buffer_info = {
        .buffer = ubs[i].vulkan_buffer.buffer,
        .offset = uniform_write.offset,
        .range = uniform_write.size,
    };
    writes[0].pBufferInfo = &descriptor_buffer_info;

    u32 copy_count = 0;
    vkUpdateDescriptorSets(t.ctx.device, write_count, writes, copy_count, NULL);
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
    f64 t1 = glfwGetTime();
    f64 dt = t1 - t0;
    t0 = t1;
    t_total += dt;

    // Simulate
    TriangleTransformation tt = simulate(t_total);

    // Render
    begin_frame(&t.ctx);
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    // Render passes need begun/ended. Can have multiple per frame.
    VkFramebuffer fb = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, fb, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    // This is a single draw action. Probably want some scheme for queuing this in the context
    write_to_uniform_buffer(&ubs[t.ctx.current_frame_index], &tt, uniform_write);
    render_mesh_material(cmd, &mesh, &mats[t.ctx.current_frame_index]);
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
