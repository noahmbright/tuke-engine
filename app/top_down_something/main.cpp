#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include "window.h"
#include <stdio.h>

void log_mat4(const glm::mat4 &m) {
  printf("logging mat4...\n-----\n");
  for (u32 row = 0; row < 4; ++row) {
    for (u32 col = 0; col < 4; ++col) {
      printf("%10.4f ", m[col][row]);
    }
    printf("\n");
  }
}

bool matrix_has_nan(const glm::mat4 &mat) {
  return glm::any(glm::isnan(mat[0])) || glm::any(glm::isnan(mat[1])) || glm::any(glm::isnan(mat[2])) ||
         glm::any(glm::isnan(mat[3]));
}

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

  u32 num_tiles = tilemap.level_height * tilemap.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap, tilemap_vertices);

  Camera camera = new_camera(CAMERA_TYPE_2D);
  camera.position.z = 15.0f;

  Inputs inputs;
  init_inputs(&inputs);

#if 0
  for (u32 i = 0; i < num_tiles; i++) {
    u32 j = i * 6;
    log_tile_vertex(&tilemap_vertices[j + 0]);
    log_tile_vertex(&tilemap_vertices[j + 1]);
    log_tile_vertex(&tilemap_vertices[j + 2]);
    printf("-------\n");
    log_tile_vertex(&tilemap_vertices[j + 3]);
    log_tile_vertex(&tilemap_vertices[j + 4]);
    log_tile_vertex(&tilemap_vertices[j + 5]);
    printf("\n");
  }
#endif

  u32 tilemap_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TILEMAP_VERT, SHADER_HANDLE_COMMON_TILEMAP_FRAG);

  u32 player_program =
      shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_PLAYER_VERT, SHADER_HANDLE_COMMON_PLAYER_FRAG);

  OpenGLMesh tilemap_mesh =
      create_opengl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes,
                                            VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_FLOAT, GL_STATIC_DRAW);
  tilemap_mesh.num_vertices = num_vertices;

  u32 vp_ubo = create_opengl_ubo(sizeof(VPUniform), GL_DYNAMIC_DRAW);
  OpenGLMaterial tilemap_material = create_opengl_material(tilemap_program);
  opengl_material_add_uniform(&tilemap_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  u32 player_ubo = create_opengl_ubo(sizeof(PlayerModel), GL_DYNAMIC_DRAW);
  OpenGLMesh player_mesh = create_opengl_mesh_with_vertex_layout(
      player_vertices, sizeof(player_vertices), VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  player_mesh.num_vertices = 6;
  OpenGLMaterial player_material = create_opengl_material(player_program);
  opengl_material_add_uniform(&player_material, player_ubo, UNIFORM_BUFFER_LABEL_PLAYER_MODEL, "PlayerModel");
  opengl_material_add_uniform(&player_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  // TODO need to figure out how I'm going to scale to realistic sizes/units e.g. meters
  f32 tilemap_dw = 2.0f / (f32)tilemap.level_width;
  f32 tilemap_dh = 2.0f / (f32)tilemap.level_height;
  glm::mat4 player_model_scale(1.0f);
  // player_model_scale = glm::scale(player_model_scale, glm::vec3(tilemap_dw, tilemap_dh, 1.0f));

  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(window) == false) {
    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;

    update_key_inputs_glfw(&inputs, window);

    glClear(GL_COLOR_BUFFER_BIT);
    int window_height, window_width;
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);

    const f32 speed = 5.0f;
    move_camera(&camera, speed * (f32)dt * inputs_to_movement_vector(&inputs));
    if (camera.position.z < 1.0f) {
      camera.position.z = 1.0f;
    }
    if (camera.position.z > CAMERA_PERSPECTIVE_PROJECTION_FAR_Z) {
      camera.position.z = CAMERA_PERSPECTIVE_PROJECTION_FAR_Z - EPSILON;
    }

    glm::vec3 camera_xy(camera.position.x, camera.position.y, 0.0f);
    glm::mat4 player_model = glm::translate(player_model_scale, camera_xy);
    glBindBuffer(GL_UNIFORM_BUFFER, player_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlayerModel), &player_model);

    buffer_vp_matrix_to_gl_ubo(&camera, vp_ubo, window_width, window_height);
    draw_opengl_mesh(&tilemap_mesh, tilemap_material);
    draw_opengl_mesh(&player_mesh, player_material);

    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
