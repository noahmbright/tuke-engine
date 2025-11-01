#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "glm/common.hpp"
#include "opengl_base.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include <stdio.h>

void print_mat4(const glm::mat4 &m) {
  for (u32 row = 0; row < 4; ++row) {
    for (u32 col = 0; col < 4; ++col) {
      printf("%f ", m[col][row]);
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

  glm::mat4 vp;
  vp = glm::mat4(1.0f);
  vp = camera_matrices.projection;
  vp = camera_matrices.view;
  vp = camera_matrices.projection * camera_matrices.view;
  if (matrix_has_nan(camera_matrices.view)) {
    exit(1);
  }
  print_mat4(vp);

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(vp), &vp);
}

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);

  u32 num_tiles = tilemap.level_height * tilemap.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap, tilemap_vertices);

  Camera camera = new_camera(CAMERA_TYPE_3D);
  camera.position.z = 2.0f;

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

  OpenGLMesh tilemap_mesh =
      create_opengl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes,
                                            VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_FLOAT, GL_STATIC_DRAW);
  tilemap_mesh.num_vertices = num_vertices;

  u32 vp_ubo = create_opengl_ubo(sizeof(VPUniform), GL_DYNAMIC_DRAW);
  OpenGLMaterial tilemap_material = create_opengl_material(tilemap_program);
  opengl_material_add_uniform(&tilemap_material, vp_ubo, UNIFORM_BUFFER_LABEL_CAMERA_VP, "VPUniform");

  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(window) == false) {
    f64 t = glfwGetTime();
    f64 dt = t - t0;
    t0 = t;
    f32 sint = sinf(t);
    // camera.move_camera_function(&camera, 20 * dt, glm::vec4(5 * cosf(t), 0.0f, 0.0f, 1.0f));
    // printf("%f, ", camera.position.z);

    glfwPollEvents();
    glClear(GL_COLOR_BUFFER_BIT);
    int window_height, window_width;
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);

    buffer_vp_matrix_to_gl_ubo(&camera, vp_ubo, window_width, window_height);
    draw_opengl_mesh(&tilemap_mesh, tilemap_material);

    glfwSwapBuffers(window);
  }

  return 0;
}
