#include "c_reflector_bringup.h"
#include "camera.h"
#include "generated_shader_utils.h"
#include "opengl_base.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include <stdio.h>

void buffer_vp_matrix_to_gl_ubo(const Camera *camera, u32 ubo, u32 window_width, u32 window_height) {
  CameraMatrices camera_matrices = new_camera_matrices(camera, window_width, window_height);
  glm::mat4 vp = camera_matrices.view * camera_matrices.projection;

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

  Camera camera = new_camera(CAMERA_TYPE_2D);

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

  while (glfwWindowShouldClose(window) == false) {
    glfwPollEvents();
    glClear(GL_COLOR_BUFFER_BIT);
    int window_height, window_width;
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);

    draw_opengl_mesh(&tilemap_mesh, tilemap_material);
    buffer_vp_matrix_to_gl_ubo(&camera, vp_ubo, window_width, window_height);

    glfwSwapBuffers(window);
  }

  return 0;
}
