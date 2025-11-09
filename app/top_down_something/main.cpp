#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include "utils.h"
#include "window.h"
#include <stdio.h>

#define PLAYER_SIDE_LENGTH_METERS (0.6f)

void buffer_vp_matrix_to_gl_ubo(const Camera *camera, u32 ubo, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices = new_camera_matrices(camera, window_width, window_height);
  glm::mat4 vp = camera_matrices.projection * camera_matrices.view;

#ifndef NDEBUG
  if (matrix_has_nan(camera_matrices.view)) {
    exit(1);
  }
#endif

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(vp), &vp);
}

glm::vec3 inputs_to_movement_vector(const Inputs *inputs) {
  glm::vec3 movement_vector = inputs_to_direction(inputs);

  f32 xy_plane_speed = 2.0f;
  if (key_held(inputs, INPUT_LEFT_SHIFT)) {
    xy_plane_speed += xy_plane_speed; // 2x speed
  }

  f32 zoom_speed = 10.0f;
  movement_vector.z = zoom_speed * inputs->scroll_dy;

  return movement_vector;
}

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);
  int window_height, window_width;
  glfwGetFramebufferSize(window, &window_width, &window_height);

  u32 num_tiles = tilemap.level_height * tilemap.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap, tilemap_vertices);
  // assuming the tilemap is centered on 0, 0
  f32 tilemap_top_left_x = -0.5f * TILE_SIDE_LENGTH_METERS * tilemap.level_width;
  f32 tilemap_top_left_y = 0.5f * TILE_SIDE_LENGTH_METERS * tilemap.level_height;
  glm::vec3 tilemap_top_left(tilemap_top_left_x, tilemap_top_left_y, 0.0f);

  Camera camera = new_camera(CAMERA_TYPE_2D);
  camera.position.z = 15.0f;

  Inputs inputs;
  init_inputs(&inputs);

  u32 tilemap_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TILEMAP_VERT, SHADER_HANDLE_COMMON_TILEMAP_FRAG);

  u32 player_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  u32 fullscreen_quad_program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_VERT,
                                                                 SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_FRAG);

  OpenGLMesh tilemap_mesh =
      create_opengl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes, num_vertices,
                                            VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_UINT, GL_STATIC_DRAW);

  u32 vp_ubo = create_opengl_ubo(sizeof(VPUniform), GL_DYNAMIC_DRAW);
  OpenGLMaterial tilemap_material = create_opengl_material(tilemap_program);
  opengl_material_add_uniform(&tilemap_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  u32 player_ubo = create_opengl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  OpenGLMesh player_mesh = create_opengl_mesh_with_vertex_layout(
      player_vertices, sizeof(player_vertices), 6, VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_ubo, UNIFORM_BUFFER_LABEL_PLAYER_MODEL, "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  u32 fbo = create_opengl_framebuffer();
  OpenGLTextureConfig texture_config = create_default_opengl_texture_config(window_height, window_width);
  u32 fbo_texture = create_opengl_texture2d(&texture_config);
  opengl_attach_texture2d_to_framebuffer(fbo, fbo_texture);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  OpenGLMesh fullscreen_quad_mesh =
      create_opengl_mesh_with_vertex_layout(NULL, 0, 3, VERTEX_LAYOUT_NULL, GL_STATIC_DRAW);
  OpenGLMaterial fullscreen_quad_material = create_opengl_material(fullscreen_quad_program);
  fullscreen_quad_material.texture = fbo_texture;

  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(window) == false) {
    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;

    update_key_inputs_glfw(&inputs, window);

    glClear(GL_COLOR_BUFFER_BIT);
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);

    const f32 speed = 5.0f;
    glm::vec3 last_camera_pos = camera.position;
    move_camera(&camera, speed * (f32)dt * inputs_to_movement_vector(&inputs));
    camera.position.z = clamp_f32(camera.position.z, 1.0f, CAMERA_PERSPECTIVE_PROJECTION_FAR_Z - EPSILON);
    if (tilemap_check_collision(&tilemap, tilemap_top_left, camera.position, glm::vec3(PLAYER_SIDE_LENGTH_METERS))) {
      camera.position = last_camera_pos;
    }

    // TODO separate player position and camera position
    // TODO will i consider the players position within a tilemap? within the whole world?
    glm::vec3 camera_xy(camera.position.x, camera.position.y, 0.0f);

    glm::mat4 player_model = glm::translate(glm::mat4(1.0f), camera_xy);
    player_model = glm::scale(player_model, glm::vec3(PLAYER_SIDE_LENGTH_METERS));
    glBindBuffer(GL_UNIFORM_BUFFER, player_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

    buffer_vp_matrix_to_gl_ubo(&camera, vp_ubo, window_width, window_height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_opengl_mesh(&tilemap_mesh, tilemap_material);
    draw_opengl_mesh(&player_mesh, player_material);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_opengl_mesh(&fullscreen_quad_mesh, fullscreen_quad_material);

    glfwSwapBuffers(window);
  }

  glDeleteFramebuffers(1, &fbo);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
