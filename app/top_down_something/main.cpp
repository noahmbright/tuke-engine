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
  GlobalState global_state;

  const u32 WINDOW_WIDTH = 1600;
  const u32 WINDOW_HEIGHT = 1200;
  global_state.window = create_window(false /* is vulkan */, "topdown", WINDOW_WIDTH, WINDOW_HEIGHT);
  glfwGetFramebufferSize(global_state.window, &global_state.window_width, &global_state.window_height);

  init_inputs(&global_state.inputs);
  memset(&global_state.scene_manager, 0, sizeof(SceneManager));
  global_state.t = 0.0;
  global_state.game_state = GAME_STATE_RUNNING;

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
  shader_registry[SHADER_ID_TILEMAP] =
      shader_handles_to_gl_program(SHADER_HANDLE_COMMON_TILEMAP_VERT, SHADER_HANDLE_COMMON_TILEMAP_FRAG);

  shader_registry[SHADER_ID_PLAYER] =
      shader_handles_to_gl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  shader_registry[SHADER_ID_FULLSCREEN_QUAD] = shader_handles_to_gl_program(SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_VERT,
                                                                            SHADER_HANDLE_COMMON_FULLSCREEN_QUAD_FRAG);

  shader_registry[SHADER_ID_OVERWORLD_OVERLAY] = shader_handles_to_gl_program(
      SHADER_HANDLE_TOPDOWN_OVERWORLD_OVERLAY_VERT, SHADER_HANDLE_TOPDOWN_OVERWORLD_OVERLAY_FRAG);

  shader_registry[SHADER_ID_GLYPHS] =
      shader_handles_to_gl_program(SHADER_HANDLE_COMMON_ASCII16X16_VERT, SHADER_HANDLE_COMMON_ASCII16X16_FRAG);

  shader_registry[SHADER_ID_VISION_CONE] = shader_handles_to_gl_program(
      SHADER_HANDLE_TOPDOWN_OVERWORLD_VISION_CONE_VERT, SHADER_HANDLE_TOPDOWN_OVERWORLD_VISION_CONE_FRAG);

  // Tilemaps
  OpenGLMesh tilemap_mesh =
      create_gl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes, num_vertices,
                                        VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_UINT, GL_STATIC_DRAW);

  OpenGLMesh tilemap1_mesh =
      create_gl_mesh_with_vertex_layout((f32 *)tilemap1_vertices, tilemap_vertices_sizes_bytes1, num_vertices1,
                                        VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_UINT, GL_STATIC_DRAW);

  // Meshes
  u32 vp_ubo = create_gl_ubo(sizeof(VPUniform), GL_DYNAMIC_DRAW);

  // Tilemaps
  OpenGLMaterial tilemap_material = create_gl_material(shader_registry[SHADER_ID_TILEMAP]);
  gl_material_add_uniform(&tilemap_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // Player
  u32 player_model_ubo = create_gl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  OpenGLMesh player_mesh = create_gl_mesh_with_vertex_layout(player_vertices, sizeof(player_vertices), 6,
                                                             VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  OpenGLMaterial player_material = create_gl_material(shader_registry[SHADER_ID_PLAYER]);
  gl_material_add_uniform(&player_material, player_model_ubo, UNIFORM_BUFFER_LABEL_TOPDOWN_OVERWORLD_PLAYER_MODEL,
                          "PlayerModel");
  gl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  OpenGLTexture arrow_texture = create_gl_texture_from_image("textures/right_arrow.jpg");
  player_material.texture = arrow_texture;

  // Glyphs
  // OpenGLTexture glyphs_texture = create_gl_texture_from_image("textures/DejaVu Sans Mono.png");

  // Framebuffer
  OpenGLRenderTarget overworld_render_target =
      create_gl_render_target(global_state.window_height, global_state.window_width, GL_RGBA, GL_UNSIGNED_BYTE);

  OpenGLMesh fullscreen_quad_mesh = create_gl_mesh_with_vertex_layout(NULL, 0, 3, VERTEX_LAYOUT_NULL, GL_STATIC_DRAW);
  OpenGLMaterial fullscreen_quad_material = create_gl_material(shader_registry[SHADER_ID_FULLSCREEN_QUAD]);
  fullscreen_quad_material.texture = overworld_render_target.texture;

  // FIXME FOOTGUN if you don't bind the vp_ubo to each program, even if the binding point is already
  // configured, you will get 0 in the shader program. NEED TO SCAFFOLD AROUND THIS.
  u32 vision_cone_ubo = create_gl_ubo(sizeof(VisionCone), GL_DYNAMIC_DRAW);
  gl_bind_ubo_to_block(shader_registry[SHADER_ID_VISION_CONE], vision_cone_ubo,
                       UNIFORM_BUFFER_LABEL_OVERWORLD_VISION_CONE, "VisionCone");
  gl_bind_ubo_to_block(shader_registry[SHADER_ID_VISION_CONE], vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

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
      .player_mesh = player_mesh,
      .player_material = player_material,
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
      .player_mesh = player_mesh,
      .player_material = player_material,
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
    current_scene->render(current_scene->data);
    handle_scene_action(&global_state.scene_manager);

    glfwSwapBuffers(global_state.window);
  }

  // Cleanup
  free(bullet_hell_scene_data.bullet_manager);
  glDeleteFramebuffers(1, &overworld_render_target.fbo);
  glfwDestroyWindow(global_state.window);
  glfwTerminate();
  return 0;
}
