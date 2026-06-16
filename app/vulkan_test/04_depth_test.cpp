#include "generated_shader_utils.h"
#include "glm/ext/matrix_transform.hpp"
#include "shaders.h"

#include "vulkan_base.h"
#include "vulkan_test_common.h"
#include "window.h"

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  BufferManager buffer_manager = create_buffer_manager();
  VulkanMesh *mesh = UPLOAD_ARRAYS(buffer_manager, f_vertices, f_indices, num_f_indices);
  flush_buffers(&t.ctx, &buffer_manager);
  mesh->instance_count = 1;

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite model_write = push_uniform(&ub_manager, sizeof(ColoredPosModel));
  UniformWrite color_write = push_uniform(&ub_manager, sizeof(ColoredPosColor));

  // Drawing 2 F's
  VulkanMaterial mats[2];
  UniformBuffer ubs[2];

  f64 t_total = 0;
  f64 t0 = glfwGetTime();
  for (u32 i = 0; i < 2; i++) {
    ubs[i] = create_uniform_buffer(&t.ctx, ub_manager.current_offset);
    init_program_spec(&t.ctx, t.rp, &common_colored_pos_program_spec, &mats[i]);

    VkBuffer buffer = ubs[i].vulkan_buffer.buffer;
    DescriptorWrite writes[] = {
        {
            .set_id = LAYOUT_ID_COLORED_POS_MODEL,
            .binding = 0,
            .buffer_info = {.buffer = buffer, .offset = model_write.offset, .range = model_write.size},
        },
        {
            .set_id = LAYOUT_ID_COLORED_POS_COLOR,
            .binding = 0,
            .buffer_info = {.buffer = buffer, .offset = color_write.offset, .range = color_write.size},
        },
    };
    update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &mats[i]);

    ColoredPosColor color = {.col = glm::vec4(f32(i))};
    write_to_uniform_buffer(&ubs[i], &color, color_write);
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    f64 t1 = glfwGetTime();
    f64 dt = t1 - t0;
    t0 = t1;
    t_total += dt;

    begin_frame(&t.ctx);

    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    VkFramebuffer framebuffer = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, framebuffer, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    for (u32 i = 0; i < 2; i++) {
      f32 phase = i * 1.57;
      f32 x = 0.5 * cosf(t_total + phase);
      f32 z = 0.1 * sinf(t_total + phase);

      ColoredPosModel model = {.mat = glm::translate(glm::mat4(1.0), glm::vec3(x, -0.5f, 0.5f + z))};
      write_to_uniform_buffer(&ubs[i], &model, model_write);
      render_mesh_material(cmd, mesh, &mats[i]);
    }
    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    submit_and_present(&t.ctx, cmd);
    update_frame_index(&t.ctx);
  }
}
