#include "opengl_test.h"
#include "c_reflector_bringup.h"
#include "generated_shader_utils.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "opengl_base.h"
#include "utils.h"
#include "window.h"

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  u32 program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TRIANGLE_BRINGUP_VERT,
                                                 SHADER_HANDLE_COMMON_TRIANGLE_BRINGUP_FRAG);

  u32 transformed_program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_VERT,
                                                             SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_FRAG);

  u32 textured_quad_program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TEXTURED_QUAD_BRINGUP_VERT,
                                                               SHADER_HANDLE_COMMON_TEXTURED_QUAD_BRINGUP_FRAG);
  printf("Compiled programs\n");

  // meshes
  OpenGLMesh triangle_mesh = create_opengl_mesh_with_vertex_layout(triangle_vertices, sizeof(triangle_vertices),
                                                                   VERTEX_LAYOUT_BINDING0VERTEX_VEC3, GL_STATIC_DRAW);
  triangle_mesh.num_vertices = 3;
  OpenGLMaterial triangle_material = create_opengl_material(program);

  OpenGLMesh transformed_mesh = create_opengl_mesh_with_vertex_layout(
      triangle_vertices, sizeof(triangle_vertices), VERTEX_LAYOUT_BINDING0VERTEX_VEC3, GL_STATIC_DRAW);
  transformed_mesh.num_vertices = 3;

  u32 transformation_ubo = create_opengl_ubo(sizeof(TriangleTransformation), GL_DYNAMIC_DRAW);
  OpenGLMaterial transformed_material = create_opengl_material(transformed_program);
  opengl_material_add_uniform(&transformed_material, transformation_ubo, UNIFORM_BUFFER_LABEL_TRIANGLE_TRANSFORMATION,
                              "TriangleTransformation");

  OpenGLMesh textured_quad_mesh = create_opengl_mesh_with_vertex_layout(
      textured_quad_vertices, sizeof(textured_quad_vertices), VERTEX_LAYOUT_BINDING0VERTEX_VEC3_VEC2, GL_STATIC_DRAW);
  textured_quad_mesh.num_vertices = 6;
  OpenGLMaterial textured_quad_material = create_opengl_material(textured_quad_program);
  textured_quad_material.texture = load_texture_opengl("textures/girl_face.jpg");

  TriangleTransformation triangle_transformation;
  const glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f));

  f64 t0 = glfwGetTime();
  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  while (glfwWindowShouldClose(window) == false) {
    glfwPollEvents();

    f64 t = glfwGetTime();
    triangle_transformation.mat = glm::rotate(translation, (f32)(t - t0), glm::vec3(0.0f, 0.0f, 1.0f));

    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, transformation_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TriangleTransformation), &triangle_transformation);

    draw_opengl_mesh(&triangle_mesh, triangle_material);
    draw_opengl_mesh(&transformed_mesh, transformed_material);
    draw_opengl_mesh(&textured_quad_mesh, textured_quad_material);

    glfwSwapBuffers(window);
  }

  return 0;
}
