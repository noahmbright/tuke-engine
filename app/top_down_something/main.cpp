#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "scene_manager.h"
#include "tilemap.h"
#include "tuke_engine.h"
#include "utils.h"
#include <OpenGL/OpenGL.h>
#include <stdio.h>

#include "bullet_hell.h"
#include "topdown.h"

int main() {
  // Global State
  const u32 WINDOW_WIDTH = 1600;
  const u32 WINDOW_HEIGHT = 1200;
  GlobalState global_state = create_global_state(WINDOW_WIDTH, WINDOW_HEIGHT);

  // Tilemaps
  u32 num_tiles = tilemap0.level_height * tilemap0.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap0, tilemap_vertices);

  u32 num_tiles1 = tilemap1.level_height * tilemap1.level_width;
  u32 num_vertices1 = num_tiles1 * 6;
  u32 tilemap_vertices_sizes_bytes1 = num_vertices1 * sizeof(TileVertex);
  TileVertex *tilemap1_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes1);
  tilemap_generate_vertices(&tilemap1, tilemap1_vertices);

  Camera camera = create_camera(CAMERA_TYPE_2D);
  camera.position.z = OVERWORLD_CAMERA_Z0;

  // Programs
  u32 tilemap_program =
      shader_handles_to_gl_program(SHADER_HANDLE_COMMON_TILEMAP_VERT, SHADER_HANDLE_COMMON_TILEMAP_FRAG);

  u32 player_program = shader_handles_to_gl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  u32 fullscreen_quad_program = shader_handles_to_gl_program(SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_VERT,
                                                             SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_FRAG);

  u32 overworld_overlay_program = shader_handles_to_gl_program(SHADER_HANDLE_TOPDOWN_OVERWORLD_OVERLAY_VERT,
                                                               SHADER_HANDLE_TOPDOWN_OVERWORLD_OVERLAY_FRAG);

  u32 glyphs_program =
      shader_handles_to_gl_program(SHADER_HANDLE_COMMON_ASCII16X16_VERT, SHADER_HANDLE_COMMON_ASCII16X16_FRAG);

  u32 vision_cone_program = shader_handles_to_gl_program(SHADER_HANDLE_TOPDOWN_OVERWORLD_VISION_CONE_VERT,
                                                         SHADER_HANDLE_TOPDOWN_OVERWORLD_VISION_CONE_FRAG);

  gl_renderer_push_program(&global_state.renderer, SHADER_ID_TILEMAP, tilemap_program);
  gl_renderer_push_program(&global_state.renderer, SHADER_ID_PLAYER, player_program);
  gl_renderer_push_program(&global_state.renderer, SHADER_ID_FULLSCREEN_QUAD, fullscreen_quad_program);
  gl_renderer_push_program(&global_state.renderer, SHADER_ID_OVERWORLD_OVERLAY, overworld_overlay_program);
  gl_renderer_push_program(&global_state.renderer, SHADER_ID_GLYPHS, glyphs_program);
  gl_renderer_push_program(&global_state.renderer, SHADER_ID_VISION_CONE, vision_cone_program);

  // Meshes
  GLMesh tilemap_mesh =
      create_gl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes, num_vertices,
                                        VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_UINT, GL_STATIC_DRAW);

  GLMesh tilemap1_mesh =
      create_gl_mesh_with_vertex_layout((f32 *)tilemap1_vertices, tilemap_vertices_sizes_bytes1, num_vertices1,
                                        VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_UINT, GL_STATIC_DRAW);

  GLMesh player_mesh = create_gl_mesh_with_vertex_layout(player_vertices, sizeof(player_vertices), 6,
                                                         VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);

  GLMesh fullscreen_quad_mesh = create_gl_mesh_with_vertex_layout(NULL, 0, 3, VERTEX_LAYOUT_NULL, GL_STATIC_DRAW);

  gl_renderer_push_mesh(&global_state.renderer, MESH_TILEMAP, tilemap_mesh);
  gl_renderer_push_mesh(&global_state.renderer, MESH_TILEMAP1, tilemap1_mesh);
  gl_renderer_push_mesh(&global_state.renderer, MESH_PLAYER, player_mesh);
  gl_renderer_push_mesh(&global_state.renderer, MESH_FULLSCREEN_QUAD, fullscreen_quad_mesh);

  // Materials
  u32 vp_ubo = create_gl_ubo(sizeof(VPUniform), GL_DYNAMIC_DRAW);

  GLMaterial tilemap_material = create_gl_material(tilemap_program);
  gl_material_add_uniform(&tilemap_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  GLMaterial player_material = create_gl_material(player_program);
  u32 player_model_ubo = create_gl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  gl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_OVERWORLD_PLAYER_MODEL,
                          "PlayerModel");
  gl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  GLTexture arrow_texture = create_gl_texture_from_image("textures/right_arrow.jpg");
  player_material.texture = arrow_texture;

  // Glyphs
  // OpenGLTexture glyphs_texture = create_gl_texture_from_image("textures/DejaVu Sans Mono.png");

  // Framebuffer
  GLRenderTarget overworld_render_target =
      create_gl_render_target(global_state.window_height, global_state.window_width, GL_RGBA, GL_UNSIGNED_BYTE);

  GLMaterial fullscreen_quad_material = create_gl_material(fullscreen_quad_program);
  fullscreen_quad_material.texture = overworld_render_target.texture;

  // FIXME FOOTGUN if you don't bind the vp_ubo to each program, even if the binding point is already
  // configured, you will get 0 in the shader program. NEED TO SCAFFOLD AROUND THIS.
  u32 vision_cone_ubo = create_gl_ubo(sizeof(VisionCone), GL_DYNAMIC_DRAW);
  gl_bind_ubo_to_block(vision_cone_program, vision_cone_ubo, UNIFORM_BUFFER_LABEL_OVERWORLD_VISION_CONE, "VisionCone");
  gl_bind_ubo_to_block(vision_cone_program, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  gl_renderer_push_material(&global_state.renderer, MATERIAL_PLAYER, player_material);
  gl_renderer_push_material(&global_state.renderer, MATERIAL_TILEMAP, tilemap_material);
  gl_renderer_push_material(&global_state.renderer, MATERIAL_FULLSCREEN_QUAD, fullscreen_quad_material);

  const glm::vec3 PLAYER_POSITION0(camera.position.x, camera.position.y, 0.0f);
  Entities entities = create_entities();
  EntityIndex player_index = entities_add(&entities);
  entities.positions[player_index.idx] = PLAYER_POSITION0;

  // Scenes
  OverworldSceneData scene0_data{
      .player_index = player_index,
      .camera = camera,
      .tilemap = &tilemap0,
      .vp_ubo = vp_ubo,
      .player_model_ubo = player_model_ubo,
      .tilemap_mesh = tilemap_mesh,
      .tilemap_material = tilemap_material,
      .other_scene = SCENE1,
      .just_transitioned = false,
      .player_rotation_simulation = 0.0f,
      .player_rotation_render = 0.0f,
      .render_target = overworld_render_target,
      .fullscreen_quad_mesh = fullscreen_quad_mesh,
      .fullscreen_quad_material = fullscreen_quad_material,
      .vision_cone_ubo = vision_cone_ubo,
      .camera_mode = CAMERA_MODE_OVERWORLD,
  };

  OverworldSceneData scene1_data{
      .player_index = player_index,
      .camera = camera,
      .tilemap = &tilemap1,
      .vp_ubo = vp_ubo,
      .player_model_ubo = player_model_ubo,
      .tilemap_mesh = tilemap1_mesh,
      .tilemap_material = tilemap_material,
      .other_scene = SCENE0,
      .just_transitioned = false,
      .player_rotation_simulation = 0.0f,
      .player_rotation_render = 0.0f,
      .render_target = overworld_render_target,
      .fullscreen_quad_mesh = fullscreen_quad_mesh,
      .fullscreen_quad_material = fullscreen_quad_material,
      .vision_cone_ubo = vision_cone_ubo,
      .camera_mode = CAMERA_MODE_OVERWORLD,
  };

  Scene scene0 = create_scene(overworld_update, overworld_draw, &scene0_data);
  Scene scene1 = create_scene(overworld_update, overworld_draw, &scene1_data);

  BulletHellSceneData bullet_hell_scene_data = create_bullet_hell_scene(vp_ubo);
  Scene scene_bullet_hell = create_scene(bullet_hell_update, bullet_hell_draw, &bullet_hell_scene_data);

  // Register scenes
  global_state.scene_manager.scene_registry[SCENE0] = &scene0;
  global_state.scene_manager.scene_registry[SCENE1] = &scene1;
  global_state.scene_manager.scene_registry[SCENE_BULLET_HELL] = &scene_bullet_hell;

  set_base_scene(&global_state.scene_manager, &scene0);

  // Main loop
  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(global_state.window) == false) {

    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;
    update_key_inputs_glfw(&global_state.inputs, global_state.window);

    // TODO lazy resize
    glfwGetFramebufferSize(global_state.window, &global_state.window_width, &global_state.window_height);
    glViewport(0, 0, global_state.window_width, global_state.window_height);

    Scene *current_scene = get_current_scene(&global_state.scene_manager);
    assert(current_scene != NULL);
    current_scene->update(current_scene->data, &global_state, dt);
    current_scene->render(&global_state.renderer, current_scene->data);
    handle_scene_action(&global_state.scene_manager);

    glfwSwapBuffers(global_state.window);
  }

  // Cleanup
  free(bullet_hell_scene_data.bullet_manager);
  glDeleteFramebuffers(1, &overworld_render_target.fbo);
  destroy_global_state(&global_state);
  glfwTerminate();
  return 0;
}
