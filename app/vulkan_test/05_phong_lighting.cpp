#include "camera.h"
#include "generated_shader_utils.h"
#include "shaders.h"

#include "vulkan_base.h"
#include "vulkan_test_common.h"
#include "window.h"
#include <math.h>

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  BufferManager buffer_manager = create_buffer_manager();
  VulkanMesh *mesh = UPLOAD_VERTEX_ARRAY(buffer_manager, cube_vertices, num_cube_vertices);
  VulkanMesh *light_mesh = UPLOAD_VERTEX_ARRAY(buffer_manager, cube_position_vertices, num_cube_vertices);
  flush_buffers(&t.ctx, &buffer_manager);

  // These should be at the draw call.
  mesh->instance_count = 1;
  light_mesh->instance_count = 1;

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  VkDescriptorBufferInfo *phong_light_write = push_uniform(&ub_manager, sizeof(PhongLight));
  VkDescriptorBufferInfo *light_model_write = push_uniform(&ub_manager, sizeof(ColoredPosMVP));
  VkDescriptorBufferInfo *light_color_write = push_uniform(&ub_manager, sizeof(ColoredPosColor));
  UniformBuffer ub = finalize_ub(&t.ctx, &ub_manager);

  VulkanMaterial light_mat;
  init_program_spec(&t.ctx, t.rp, &common_colored_pos_program_spec, &light_mat);

  Vec4 light_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  ColoredPosColor color_pos_color = {.col = light_color};
  PhongLight phong_light = {.color = light_color};
  write_to_uniform_buffer(&ub, &phong_light, *phong_light_write);
  write_to_uniform_buffer(&ub, &color_pos_color, *light_color_write);

  VulkanMaterial lit_mat;
  init_program_spec(&t.ctx, t.rp, &common_phong_program_spec, &lit_mat);

  DescriptorWrite phong_writes[] = {
      {.set_id = LAYOUT_ID_PHONG, .binding = BINDING_PHONG_LIGHT, .buffer_info = *phong_light_write},
  };
  update_vulkan_material(&t.ctx, phong_writes, ARRAY_SIZE(phong_writes), &lit_mat);

  DescriptorWrite light_writes[] = {
      {
          .set_id = LAYOUT_ID_COLORED_POS_MVP,
          .binding = BINDING_COLORED_POS_MVP_MVP,
          .buffer_info = *light_model_write,
      },
      {
          .set_id = LAYOUT_ID_COLORED_POS_COLOR,
          .binding = BINDING_COLORED_POS_COLOR_COLOR,
          .buffer_info = *light_color_write,
      },
  };
  update_vulkan_material(&t.ctx, light_writes, ARRAY_SIZE(light_writes), &light_mat);

  Camera camera = create_camera(CAMERA_TYPE_3D);
  camera.position = {4.0f, -6.0f, 10.0f};
  camera.direction = normalize_v3(sub_v3(vec3(0.0f, 0.0f, 0.0f), camera.position));

  Inputs inputs;
  init_inputs(&inputs);

  f64 t_total = 0;
  f64 t0 = glfwGetTime();
  ModelVP mvp = {.model = mat4()};
  while (!glfwWindowShouldClose(window)) {
    // This is a lot of boilerplate.
    // I am writing a lot of tests, so this skews my perception a bit.
    // Maybe I should intersperse some game dev in with the tests too.
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    f64 t1 = glfwGetTime();
    f64 dt = t1 - t0;
    t0 = t1;
    t_total += dt;

    update_key_inputs_glfw(&inputs, window);

    // Simulate
    f32 aspect = f32(width) / f32(height);
    CameraMatrices cam_mats = create_camera_matrices(&camera, aspect);
    mult_m4(&cam_mats.projection, &cam_mats.view, &mvp.vp);

    Vec2 dir = inputs_to_direction(&inputs);
    camera_move_3d(&camera, scale_v2(dir, (f32)dt * 5.0f));

    f32 r = 2.0f;
    f32 light_x = r * sinf(3.0 * t_total + 0.2);
    f32 light_y = r * sinf(2.0 * t_total + 0.2);
    f32 light_z = r * cosf(4.0 * t_total + 0.7);
    Vec3 light_pos = vec3(light_x, light_y, light_z);
    phong_light.position = vec4(light_x, light_y, light_z, 0.0);
    write_to_uniform_buffer(&ub, &phong_light, *phong_light_write);

    ColoredPosMVP light_mvp;
    Mat4 light_model = mat4();
    scale_m4(vec3(0.2f, 0.2f, 0.2f), &light_model);
    translate_m4(light_pos, &light_model);
    mult_m4(&mvp.vp, &light_model, &light_mvp.mat);
    write_to_uniform_buffer(&ub, &light_mvp, *light_model_write);

    // Rendering
    begin_frame(&t.ctx);

    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    VkFramebuffer framebuffer = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, framebuffer, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    push_constants_material(cmd, &lit_mat, &mvp);
    render_mesh_material(cmd, mesh, &lit_mat);

    render_mesh_material(cmd, light_mesh, &light_mat);

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    submit_and_present(&t.ctx, cmd);
    update_frame_index(&t.ctx);
  }
}
