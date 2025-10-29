#include "generated_shader_utils.h"
#include "tilemap.h"
#include "topdown.h"
#include "tuke_engine.h"
#include <stdio.h>

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);

  u32 num_tiles = tilemap.level_height * tilemap.level_width;
  u32 num_vertices = num_tiles * 6;
  u32 tilemap_vertices_sizes_bytes = num_vertices * sizeof(TileVertex);
  TileVertex *tilemap_vertices = (TileVertex *)malloc(tilemap_vertices_sizes_bytes);
  tilemap_generate_vertices(&tilemap, tilemap_vertices);
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

  u32 program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TILEMAP_VERT, SHADER_HANDLE_COMMON_TILEMAP_FRAG);
  OpenGLMesh mesh = create_opengl_mesh_with_vertex_layout((f32 *)tilemap_vertices, tilemap_vertices_sizes_bytes,
                                                          VERTEX_LAYOUT_BINDING0VERTEX_VEC2_VEC3_FLOAT, GL_STATIC_DRAW);
  mesh.num_vertices = num_vertices;
  OpenGLMaterial material = create_opengl_material(program);

  while (glfwWindowShouldClose(window) == false) {
    glfwPollEvents();
    glClear(GL_COLOR_BUFFER_BIT);
    draw_opengl_mesh(&mesh, material);
    glfwSwapBuffers(window);
  }

  return 0;
}
