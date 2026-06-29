#include "assets.h"
#include "generated_shader_utils.h"
#include "shaders.h"

#include "camera.h"
#include "tuke_engine.h"
#include "vulkan_test_common.h"
#include "window.h"

#include <math.h>

enum TextureId {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  TEXTURE_BRICKWALL,
  NUM_TEXTURES
};

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/girl_face.jpg", "textures/girl_face_normal_map.jpg", "textures/brickwall.jpg"
};

int main() {
  GLFWwindow *window = create_window(true /* is_vulkan */);
  VulkanTest t = init_vulkan_test(window);

  VulkanTexture textures[NUM_TEXTURES];
  load_vulkan_textures(&t.ctx, texture_names, NUM_TEXTURES, textures);

  ColorDepthFramebuffer offscreen_framebuffer =
      create_color_depth_framebuffer(&t.ctx, t.ctx.swapchain_extent, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_D32_SFLOAT);

  BufferManager buffer_manager = create_buffer_manager();
  VulkanMesh *p_tri =
      upload_arrays_single(&buffer_manager, triangle_vertices, sizeof(triangle_vertices), 3, NULL, 0, 0);
  VulkanMesh *p_sq = upload_arrays_single(&buffer_manager, square_vertices, sizeof(square_vertices), 6, NULL, 0, 0);
  const void *iq_varrays[] = {unit_square_positions, quad_positions};
  const u64 iq_vsizes[] = {sizeof(unit_square_positions), sizeof(quad_positions)};
  VulkanMesh *p_iq =
      upload_arrays(&buffer_manager, iq_varrays, iq_vsizes, 0, 2, unit_square_indices, sizeof(unit_square_indices), 6);
  VulkanMesh *p_cube = upload_arrays_single(&buffer_manager, cube_vertices, sizeof(cube_vertices), 36, NULL, 0, 0);
  flush_buffers(&t.ctx, &buffer_manager);

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  VkDescriptorBufferInfo *mvp_write = push_uniform(&ub_manager, sizeof(MVPUniform));
  VkDescriptorBufferInfo *x_write = push_uniform(&ub_manager, sizeof(UniformBufferObject));
  VkDescriptorBufferInfo *light_pos_write = push_uniform(&ub_manager, sizeof(LightPosition));
  VkDescriptorBufferInfo *camera_vp_write = push_uniform(&ub_manager, sizeof(CameraVP));
  VkDescriptorBufferInfo *phong_light_write = push_uniform(&ub_manager, sizeof(PhongLight));
  UniformBuffer ub = finalize_ub(&t.ctx, &ub_manager);

  // triangle: SIMPLE set=0 (binding 0=x, 1=camera_vp, 2=light_position)
  VulkanMaterial triangle_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, NULL, &common_simple_program_spec, &triangle_mat);
  {
    DescriptorWrite writes[] = {
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 0, .buffer_info = *x_write},
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 1, .buffer_info = *camera_vp_write},
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 2, .buffer_info = *light_pos_write},
    };
    update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &triangle_mat);
  }

  // square: SIMPLE set=0 (binding 0=x, 1=camera_vp, 2=light_position)
  VulkanMaterial square_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, NULL, &common_simple_program_spec, &square_mat);
  {
    DescriptorWrite writes[] = {
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 0, .buffer_info = *x_write},
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 1, .buffer_info = *camera_vp_write},
        {.set_id = LAYOUT_ID_SIMPLE, .binding = 2, .buffer_info = *light_pos_write},
    };
    update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &square_mat);
  }

  // instanced_quad: INSTANCED_QUAD set=0 (binding 0=mvp, 1=x, 2=tex)
  VulkanMaterial instanced_quad_mat;
  init_program_spec(
      &t.ctx, offscreen_framebuffer.render_pass, NULL, &common_instanced_quad_program_spec, &instanced_quad_mat
  );
  {
    DescriptorWrite writes[] = {
        {.set_id = LAYOUT_ID_INSTANCED_QUAD, .binding = 0, .buffer_info = *mvp_write},
        {.set_id = LAYOUT_ID_INSTANCED_QUAD, .binding = 1, .buffer_info = *x_write},
        {.set_id = LAYOUT_ID_INSTANCED_QUAD,
         .binding = 2,
         .image_info = {
             .sampler = t.ctx.samplers[SAMPLER_LINEAR_CLAMP],
             .imageView = textures[TEXTURE_GIRL_FACE_NORMAL_MAP].image_view,
             .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         }},
    };
    update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &instanced_quad_mat);
  }

  // cube: PHONG set=0 (binding 1=light); model+vp via push constants
  VulkanMaterial cube_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, NULL, &common_phong_program_spec, &cube_mat);
  {
    DescriptorWrite writes[] = {
        {.set_id = LAYOUT_ID_PHONG, .binding = BINDING_PHONG_LIGHT, .buffer_info = *phong_light_write},
    };
    update_vulkan_material(&t.ctx, writes, ARRAY_SIZE(writes), &cube_mat);
  }

  // fullscreen_quad: PLACEHOLDER set=0 binding 0=offscreen image
  VulkanMaterial fullscreen_quad_mat;
  init_program_spec(&t.ctx, t.ctx.render_pass, NULL, &common_fullscreen_quad_program_spec, &fullscreen_quad_mat);
  {
    DescriptorWrite write = {
        .set_id = LAYOUT_ID_PLACEHOLDER,
        .binding = 0,
        .image_info = {
            .sampler = t.ctx.samplers[SAMPLER_LINEAR_CLAMP],
            .imageView = offscreen_framebuffer.color_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    update_vulkan_material(&t.ctx, &write, 1, &fullscreen_quad_mat);
  }

  VulkanMesh triangle_mesh = *p_tri;
  VulkanMesh square_mesh = *p_sq;
  VulkanMesh instanced_quad_mesh = *p_iq;
  VulkanMesh cube_mesh = *p_cube;
  VulkanMesh fullscreen_quad_mesh = {.vertex_count = 3};

  MVPUniform mvp = {.projection = mat4(), .view = mat4()};
  const Vec3 cube_translation_vector = {1.5f, -1.3f, 1.5f};

  Camera camera = create_camera(CAMERA_TYPE_3D);
  camera.position = {0.0f, 0.0f, 5.0f};
  Inputs inputs;
  init_inputs(&inputs);

  f32 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    i32 height, width;
    glfwGetWindowSize(window, &width, &height);
    update_inputs_glfw(&inputs, window);
    glfwPollEvents();

    f32 t1 = glfwGetTime();
    f32 dt = t1 - t0;
    t0 = t1;
    f32 sint = sinf(t1);

    UniformBufferObject ubo = {.x = fabsf(sint)};
    mvp.model = mat4();
    scale_m4(vec3(0.5f, 0.5f, 0.5f), &mvp.model);
    // TODO Rotation
    // rotate(mvp.model, sint, Vec3(0.0f, 0.0f, 1.0f));

    Mat4 cube_model = mat4();
    scale_m4(vec3(0.2f, 0.2f, 0.2f), &cube_model);
    translate_m4(cube_translation_vector, &cube_model);

    LightPosition light_position = {
        .position = vec4(2 * sint, sint, 0.5f, 0.0f),
        .color = vec4(1.0, 1.0, 1.0, 1.0),
    };

    Mat4 vp = make_camera_vp(&camera, f32(width) / (f32)height);

    PhongLight phong_light = {.position = light_position.position, .color = light_position.color};

    write_to_uniform_buffer(&ub, &mvp, *mvp_write);
    write_to_uniform_buffer(&ub, &ubo, *x_write);
    write_to_uniform_buffer(&ub, &light_position, *light_pos_write);
    write_to_uniform_buffer(&ub, &vp, *camera_vp_write);
    write_to_uniform_buffer(&ub, &phong_light, *phong_light_write);

    begin_frame(&t.ctx);
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    begin_render_pass(
        &t.ctx, cmd, offscreen_framebuffer.render_pass, offscreen_framebuffer.framebuffer, t.clear_values,
        NUM_ATTACHMENTS, t.viewport_state
    );
    render_mesh(cmd, &triangle_mesh, &triangle_mat);
    render_mesh(cmd, &square_mesh, &square_mat);
    render_mesh_instanced(cmd, &instanced_quad_mesh, &instanced_quad_mat, 5, NULL);
    ModelVP cube_mvp = {.model = cube_model, .vp = vp};
    push_constants_material(cmd, &cube_mat, &cube_mvp);
    render_mesh(cmd, &cube_mesh, &cube_mat);
    vkCmdEndRenderPass(cmd);

    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    begin_render_pass(
        &t.ctx, cmd, t.ctx.render_pass, t.ctx.framebuffers[t.ctx.image_index], t.clear_values, NUM_ATTACHMENTS,
        t.viewport_state
    );
    render_mesh(cmd, &fullscreen_quad_mesh, &fullscreen_quad_mat);
    vkCmdEndRenderPass(cmd);

    end_frame(&t.ctx, cmd);

    Vec2 movement_direction = inputs_to_direction(&inputs);
    camera_move_3d(&camera, scale_v2(movement_direction, dt * 10.0f));
  }

  vkDeviceWaitIdle(t.ctx.device);

  for (u32 i = 0; i < NUM_TEXTURES; i++) {
    destroy_vulkan_texture(t.ctx.device, &textures[i]);
  }
  destroy_color_depth_framebuffer(t.ctx.device, &offscreen_framebuffer);

  destroy_vulkan_material(t.ctx.device, &triangle_mat);
  destroy_vulkan_material(t.ctx.device, &square_mat);
  destroy_vulkan_material(t.ctx.device, &instanced_quad_mat);
  destroy_vulkan_material(t.ctx.device, &cube_mat);
  destroy_vulkan_material(t.ctx.device, &fullscreen_quad_mat);

  destroy_uniform_buffer(&t.ctx, &ub);
  destroy_buffer_manager(&buffer_manager);
  destroy_vulkan_test(&t);

  return 0;
}
