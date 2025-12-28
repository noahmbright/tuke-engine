#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include "window.h"
#include <stdio.h>

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);
  GlobalState global_state;
  glfwGetFramebufferSize(window, &global_state.window_width, &global_state.window_height);

  u32 num_tiles = tilemap.level_height * tilemap.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap, tilemap_vertices);

  u32 num_tiles1 = tilemap.level_height * tilemap.level_width;
  u32 num_vertices1 = num_tiles1 * 6;
  u32 tilemap_vertices_sizes_bytes1 = num_vertices1 * sizeof(TileVertex);
  TileVertex *tilemap1_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes1);
  tilemap_generate_vertices(&tilemap1, tilemap1_vertices);

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

  OpenGLMesh tilemap1_mesh =
      create_opengl_mesh_with_vertex_layout((f32 *)tilemap1_vertices, tilemap_vertices_sizes_bytes1, num_vertices1,
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
  OpenGLTextureConfig texture_config =
      create_default_opengl_texture_config(global_state.window_height, global_state.window_width);
  u32 fbo_texture = create_opengl_texture2d(&texture_config);
  opengl_attach_texture2d_to_framebuffer(fbo, fbo_texture);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  OpenGLMesh fullscreen_quad_mesh =
      create_opengl_mesh_with_vertex_layout(NULL, 0, 3, VERTEX_LAYOUT_NULL, GL_STATIC_DRAW);
  (void)fullscreen_quad_mesh;
  OpenGLMaterial fullscreen_quad_material = create_opengl_material(fullscreen_quad_program);
  fullscreen_quad_material.texture = fbo_texture;

  Scene0Data scene0{
      .player_pos = camera.position,
      .camera = camera,
      .tilemap = &tilemap,
      .vp_ubo = vp_ubo,
      .player_ubo = player_ubo,
      .player_mesh = player_mesh,
      .player_material = player_material,
      .tilemap_mesh = tilemap_mesh,
      .tilemap_material = tilemap_material,
  };

  // lol
  Scene0Data scene1{
      .player_pos = camera.position,
      .camera = camera,
      .tilemap = &tilemap1,
      .vp_ubo = vp_ubo,
      .player_ubo = player_ubo,
      .player_mesh = player_mesh,
      .player_material = player_material,
      .tilemap_mesh = tilemap1_mesh,
      .tilemap_material = tilemap_material,
  };

  Scene0Data *scene = &scene0;

  // main loop
  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(window) == false) {
    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;
    update_key_inputs_glfw(&inputs, window);

    if (key_pressed(&inputs, INPUT_KEY_T)) {
      scene = (scene == &scene0) ? &scene1 : &scene0;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glfwGetFramebufferSize(window, &global_state.window_width, &global_state.window_height);
    glViewport(0, 0, global_state.window_width, global_state.window_height);

    scene0_update(scene, &global_state, &inputs, dt);
    scene0_draw(scene);

    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // glClear(GL_COLOR_BUFFER_BIT);
    // draw_opengl_mesh(&fullscreen_quad_mesh, fullscreen_quad_material);

    glfwSwapBuffers(window);
  }

  glDeleteFramebuffers(1, &fbo);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
