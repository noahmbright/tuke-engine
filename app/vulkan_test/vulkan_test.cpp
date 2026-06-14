#include "assets.h"
#include "generated_shader_utils.h"
#define GLM_ENABLE_EXPERIMENTAL

#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "tuke_engine.h"
#include "vulkan_test_common.h"
#include "window.h"

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

  VkSampler sampler = create_sampler(t.ctx.device);
  ColorDepthFramebuffer offscreen_framebuffer =
      create_color_depth_framebuffer(&t.ctx, t.ctx.swapchain_extent, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_D32_SFLOAT);

  BufferUploadQueue buffer_upload_queue = create_buffer_upload_queue();
  u64 triangle_vertices_offset = UPLOAD_ARRAY(buffer_upload_queue, triangle_vertices);
  u64 square_offset = UPLOAD_ARRAY(buffer_upload_queue, square_vertices);
  u64 unit_square_pos_offset = UPLOAD_ARRAY(buffer_upload_queue, unit_square_positions);
  u64 quad_pos_offset = UPLOAD_ARRAY(buffer_upload_queue, quad_positions);
  u64 index_offset = UPLOAD_ARRAY(buffer_upload_queue, unit_square_indices);
  u64 cube_offset = UPLOAD_ARRAY(buffer_upload_queue, cube_vertices);
  BufferManager buffer_manager = flush_buffers(&t.ctx, &buffer_upload_queue);

  UniformBufferManager ub_manager = create_uniform_buffer_manager();
  UniformWrite mvp_write = push_uniform(&ub_manager, sizeof(MVPUniform));
  UniformWrite cube_model_write = push_uniform(&ub_manager, sizeof(CubeModel));
  UniformWrite x_write = push_uniform(&ub_manager, sizeof(UniformBufferObject));
  UniformWrite light_pos_write = push_uniform(&ub_manager, sizeof(LightPosition));
  UniformWrite camera_vp_write = push_uniform(&ub_manager, sizeof(CameraVP));
  UniformBuffer ub = create_uniform_buffer(&t.ctx, ub_manager.current_offset);

  // triangle: SIMPLE set=0 (binding 0=x, 1=camera_vp, 2=light_position)
  VulkanMaterial triangle_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, &common_simple_program_spec, &triangle_mat);
  {
    VkDescriptorBufferInfo x_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = x_write.offset, .range = x_write.size
    };
    VkDescriptorBufferInfo vp_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = camera_vp_write.offset, .range = camera_vp_write.size
    };
    VkDescriptorBufferInfo light_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = light_pos_write.offset, .range = light_pos_write.size
    };
    VkWriteDescriptorSet writes[3];
    writes[0] = fill_write(&triangle_mat, 0, 0);
    writes[0].pBufferInfo = &x_info;
    writes[1] = fill_write(&triangle_mat, 0, 1);
    writes[1].pBufferInfo = &vp_info;
    writes[2] = fill_write(&triangle_mat, 0, 2);
    writes[2].pBufferInfo = &light_info;
    vkUpdateDescriptorSets(t.ctx.device, 3, writes, 0, NULL);
  }

  // square: GLOBAL set=0 (binding 0=x, 1=camera_vp, 2=light_position)
  VulkanMaterial square_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, &common_square_program_spec, &square_mat);
  {
    VkDescriptorBufferInfo x_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = x_write.offset, .range = x_write.size
    };
    VkDescriptorBufferInfo vp_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = camera_vp_write.offset, .range = camera_vp_write.size
    };
    VkDescriptorBufferInfo light_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = light_pos_write.offset, .range = light_pos_write.size
    };
    VkWriteDescriptorSet writes[3];
    writes[0] = fill_write(&square_mat, 0, 0);
    writes[0].pBufferInfo = &x_info;
    writes[1] = fill_write(&square_mat, 0, 1);
    writes[1].pBufferInfo = &vp_info;
    writes[2] = fill_write(&square_mat, 0, 2);
    writes[2].pBufferInfo = &light_info;
    vkUpdateDescriptorSets(t.ctx.device, 3, writes, 0, NULL);
  }

  // instanced_quad: set=0 PLACEHOLDER binding 2=tex, set=1 INSTANCED_QUAD binding 0=mvp binding 1=x
  VulkanMaterial instanced_quad_mat;
  init_program_spec(
      &t.ctx, offscreen_framebuffer.render_pass, &common_instanced_quad_program_spec, &instanced_quad_mat
  );
  {
    VkDescriptorImageInfo img_info = {
        .sampler = sampler,
        .imageView = textures[TEXTURE_GIRL_FACE_NORMAL_MAP].image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorBufferInfo mvp_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = mvp_write.offset, .range = mvp_write.size
    };
    VkDescriptorBufferInfo x_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = x_write.offset, .range = x_write.size
    };
    VkWriteDescriptorSet writes[3];
    writes[0] = fill_write(&instanced_quad_mat, 0, 2);
    writes[0].pImageInfo = &img_info;
    writes[1] = fill_write(&instanced_quad_mat, 1, 0);
    writes[1].pBufferInfo = &mvp_info;
    writes[2] = fill_write(&instanced_quad_mat, 1, 1);
    writes[2].pBufferInfo = &x_info;
    vkUpdateDescriptorSets(t.ctx.device, 3, writes, 0, NULL);
  }

  // cube: CUBE set=0 (binding 0=cube_model, 1=light_position, 2=camera_vp)
  VulkanMaterial cube_mat;
  init_program_spec(&t.ctx, offscreen_framebuffer.render_pass, &common_cube_program_spec, &cube_mat);
  {
    VkDescriptorBufferInfo model_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = cube_model_write.offset, .range = cube_model_write.size
    };
    VkDescriptorBufferInfo light_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = light_pos_write.offset, .range = light_pos_write.size
    };
    VkDescriptorBufferInfo vp_info = {
        .buffer = ub.vulkan_buffer.buffer, .offset = camera_vp_write.offset, .range = camera_vp_write.size
    };
    VkWriteDescriptorSet writes[3];
    writes[0] = fill_write(&cube_mat, 0, 0);
    writes[0].pBufferInfo = &model_info;
    writes[1] = fill_write(&cube_mat, 0, 1);
    writes[1].pBufferInfo = &light_info;
    writes[2] = fill_write(&cube_mat, 0, 2);
    writes[2].pBufferInfo = &vp_info;
    vkUpdateDescriptorSets(t.ctx.device, 3, writes, 0, NULL);
  }

  // fullscreen_quad: PLACEHOLDER set=0 binding 0=offscreen image
  VulkanMaterial fullscreen_quad_mat;
  init_program_spec(&t.ctx, t.ctx.render_pass, &common_fullscreen_quad_program_spec, &fullscreen_quad_mat);
  {
    VkDescriptorImageInfo img_info = {
        .sampler = sampler,
        .imageView = offscreen_framebuffer.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet write = fill_write(&fullscreen_quad_mat, 0, 0);
    write.pImageInfo = &img_info;
    vkUpdateDescriptorSets(t.ctx.device, 1, &write, 0, NULL);
  }

  VkBuffer vbuf = buffer_manager.buffer.buffer;
  VulkanMesh triangle_mesh = {
      .num_vertices = 3,
      .instance_count = 1,
      .num_vertex_buffers = 1,
      .vertex_buffers = {vbuf},
      .vertex_buffer_offsets = {triangle_vertices_offset},
  };
  VulkanMesh square_mesh = {
      .num_vertices = 6,
      .instance_count = 1,
      .num_vertex_buffers = 1,
      .vertex_buffers = {vbuf},
      .vertex_buffer_offsets = {square_offset},
  };
  VulkanMesh instanced_quad_mesh = {
      .instance_count = 5,
      .num_indices = 6,
      .num_vertex_buffers = 2,
      .vertex_buffers = {vbuf, vbuf},
      .vertex_buffer_offsets = {unit_square_pos_offset, quad_pos_offset},
      .index_buffer_offset = index_offset,
      .index_buffer = buffer_manager.buffer.buffer,
  };
  VulkanMesh cube_mesh = {
      .num_vertices = 36,
      .instance_count = 1,
      .num_vertex_buffers = 1,
      .vertex_buffers = {vbuf},
      .vertex_buffer_offsets = {cube_offset},
  };
  VulkanMesh fullscreen_quad_mesh = {.num_vertices = 3, .instance_count = 1};

  MVPUniform mvp = {.projection = glm::mat4(1.0f), .view = glm::mat4(1.0f)};
  const glm::vec3 cube_translation_vector = {1.5f, -1.3f, 1.5f};

  Camera camera = create_camera(CAMERA_TYPE_3D);
  camera.position = {0.0f, 0.0f, 5.0f};
  glm::mat4 camera_vp;
  Inputs inputs;
  init_inputs(&inputs);

  f32 t0 = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    i32 height, width;
    glfwGetWindowSize(window, &width, &height);
    update_key_inputs_glfw(&inputs, window);
    glfwPollEvents();

    f32 t1 = glfwGetTime();
    f32 dt = t1 - t0;
    t0 = t1;
    f32 sint = sinf(t1);

    UniformBufferObject ubo = {.x = fabsf(sint)};
    mvp.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    mvp.model = glm::rotate(mvp.model, sint, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), cube_translation_vector);
    cube_model = glm::scale(cube_model, {0.2f, 0.2f, 0.2f});

    glm::vec4 light_position(2 * sint, sint, 0.5f, 0.0f);

    CameraMatrices camera_matrices = create_camera_matrices(&camera, width, height);
    camera_vp = camera_matrices.projection * camera_matrices.view;

    write_to_uniform_buffer(&ub, &mvp, mvp_write);
    write_to_uniform_buffer(&ub, &cube_model, cube_model_write);
    write_to_uniform_buffer(&ub, &ubo, x_write);
    write_to_uniform_buffer(&ub, &light_position, light_pos_write);
    write_to_uniform_buffer(&ub, &camera_vp, camera_vp_write);

    begin_frame(&t.ctx);
    VkCommandBuffer cmd = begin_command_buffer(&t.ctx);

    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    begin_render_pass(
        &t.ctx, cmd, offscreen_framebuffer.render_pass, offscreen_framebuffer.framebuffer, t.clear_values,
        NUM_ATTACHMENTS, t.viewport_state
    );
    render_mesh_material(cmd, &triangle_mesh, &triangle_mat);
    render_mesh_material(cmd, &square_mesh, &square_mat);
    render_mesh_material(cmd, &instanced_quad_mesh, &instanced_quad_mat);
    render_mesh_material(cmd, &cube_mesh, &cube_mat);
    vkCmdEndRenderPass(cmd);

    transition_image_layout(
        cmd, offscreen_framebuffer.color_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    begin_render_pass(
        &t.ctx, cmd, t.ctx.render_pass, t.ctx.framebuffers[t.ctx.image_index], t.clear_values, NUM_ATTACHMENTS,
        t.viewport_state
    );
    render_mesh_material(cmd, &fullscreen_quad_mesh, &fullscreen_quad_mat);
    vkCmdEndRenderPass(cmd);

    end_frame(&t.ctx, cmd);

    glm::vec2 movement_direction = inputs_to_direction(&inputs);
    camera_move_3d(&camera, dt * 10.0f * movement_direction);
  }

  vkDeviceWaitIdle(t.ctx.device);

  vkDestroySampler(t.ctx.device, sampler, NULL);
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
