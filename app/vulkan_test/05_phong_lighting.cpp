#include "camera.h"
#include "generated_shader_utils.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "shaders.h"

#include "vulkan_base.h"
#include "vulkan_test_common.h"
#include "window.h"

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  BufferManager buffer_manager = create_buffer_manager();
  VulkanMesh *mesh = UPLOAD_VERTEX_ARRAY(buffer_manager, cube_vertices, num_cube_vertices);
  VulkanMesh *light_mesh = UPLOAD_VERTEX_ARRAY(buffer_manager, cube_position_vertices, num_cube_vertices);
  flush_buffers(&t.ctx, &buffer_manager);

  mesh->instance_count = 1;
  light_mesh->instance_count = 1;

  // clang-format off
  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  VkDescriptorBufferInfo *phong_model_write  = push_uniform(&ub_manager, sizeof(CubeModel));
  VkDescriptorBufferInfo *vp_write           = push_uniform(&ub_manager, sizeof(CameraVP));
  VkDescriptorBufferInfo *phong_light_write  = push_uniform(&ub_manager, sizeof(PhongLight));
  VkDescriptorBufferInfo *light_model_write  = push_uniform(&ub_manager, sizeof(ColoredPosModel));
  VkDescriptorBufferInfo *light_color_write  = push_uniform(&ub_manager, sizeof(ColoredPosColor));
  // clang-format on
  UniformBuffer ub = finalize_ub(&t.ctx, &ub_manager);

  VulkanMaterial light_mat;
  init_program_spec(&t.ctx, t.rp, &common_colored_pos_program_spec, &light_mat);

  // Have to coordinate this write with the writes in the arrays below.
  // Getting confusing.
  glm::vec4 light_color = glm::vec4(1.0);
  ColoredPosColor color_pos_color = {.col = light_color};
  PhongLight phong_light = {.color = light_color};
  write_to_uniform_buffer(&ub, &phong_light, *phong_light_write);
  write_to_uniform_buffer(&ub, &color_pos_color, *light_color_write);

  VulkanMaterial lit_mat;
  init_program_spec(&t.ctx, t.rp, &common_phong_program_spec, &lit_mat);

  // This is a bit too much work.
  // Having to look at the shaders to see the bindings is annoying.
  // Maybe could be tolerable. In a game init would be okay to get right once.
  DescriptorWrite phong_writes[] = {
      {.set_id = LAYOUT_ID_PHONG, .binding = BINDING_PHONG_CUBE_TRANSFORM, .buffer_info = *phong_model_write},
      {.set_id = LAYOUT_ID_PHONG, .binding = BINDING_PHONG_LIGHT, .buffer_info = *phong_light_write},
      {.set_id = LAYOUT_ID_PHONG, .binding = BINDING_PHONG_CAMERA_VP, .buffer_info = *vp_write},
  };
  update_vulkan_material(&t.ctx, phong_writes, ARRAY_SIZE(phong_writes), &lit_mat);

  DescriptorWrite light_writes[] = {
      {
          .set_id = LAYOUT_ID_COLORED_POS_MODEL,
          .binding = BINDING_COLORED_POS_MODEL_MODEL,
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
  camera.direction = -glm::normalize(camera.position);

  Inputs inputs;
  init_inputs(&inputs);

  f64 t_total = 0;
  f64 t0 = glfwGetTime();
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

    // Simulate
    CameraMatrices cam_mats = create_camera_matrices(&camera, width, height);
    CameraVP camera_vp = {.vp = cam_mats.projection * cam_mats.view};
    write_to_uniform_buffer(&ub, &camera_vp, *vp_write);
    update_key_inputs_glfw(&inputs, window);

    glm::vec2 dir = inputs_to_direction(&inputs);
    camera_move_3d(&camera, (f32)dt * 5.0f * dir);

    f32 r = 2.0f;
    f32 light_x = r * sinf(3.0 * t_total + 0.2);
    f32 light_y = r * sinf(2.0 * t_total + 0.2);
    f32 light_z = r * cosf(4.0 * t_total + 0.7);
    glm::vec3 light_pos3 = glm::vec3(light_x, light_y, light_z);
    phong_light.position = glm::vec4(light_pos3, 0.0);
    write_to_uniform_buffer(&ub, &phong_light, *phong_light_write);

    CubeModel cube_model = {.model = glm::mat4(1.0f)};
    write_to_uniform_buffer(&ub, &cube_model, *phong_model_write);

    ColoredPosModel light_model = {
        .mat = camera_vp.vp * glm::scale(glm::translate(glm::mat4(1.0f), light_pos3), glm::vec3(0.2f))
    };
    write_to_uniform_buffer(&ub, &light_model, *light_model_write);

    // Rendering
    begin_frame(&t.ctx);

    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    VkFramebuffer framebuffer = t.ctx.framebuffers[t.ctx.image_index];
    begin_render_pass(&t.ctx, cmd, t.rp, framebuffer, t.clear_values, NUM_ATTACHMENTS, t.viewport_state);

    render_mesh_material(cmd, mesh, &lit_mat);
    render_mesh_material(cmd, light_mesh, &light_mat);

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    submit_and_present(&t.ctx, cmd);
    update_frame_index(&t.ctx);
  }
}
