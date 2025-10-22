#include "opengl_test.h"
#include "c_reflector_bringup.h"
#include "generated_shader_utils.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "window.h"

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  u32 vbo, vao;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices), triangle_vertices, GL_STATIC_DRAW);
  u32 program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_TRIANGLE_BRINGUP_VERT,
                                                 SHADER_HANDLE_COMMON_TRIANGLE_BRINGUP_FRAG);
  glUseProgram(program);
  init_opengl_vertex_layout(SHADER_HANDLE_COMMON_TRIANGLE_BRINGUP_VERT, vao, &vbo, 1, 0);
  glBindVertexArray(vao);

  u32 transformed_vao;
  glGenVertexArrays(1, &transformed_vao);
  u32 transformed_program = shader_handles_to_opengl_program(SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_VERT,
                                                             SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_FRAG);
  glUseProgram(transformed_program);
  init_opengl_vertex_layout(SHADER_HANDLE_COMMON_UNIFORM_BRINGUP_VERT, transformed_vao, &vbo, 1, 0);

  u32 ubo;
  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(TriangleTransformation), NULL, GL_DYNAMIC_DRAW);

  u32 block_index = glGetUniformBlockIndex(transformed_program, "TriangleTransformation");
  glUniformBlockBinding(transformed_program, block_index, 0);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

  TriangleTransformation triangle_transformation;
  const glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f));

  f64 t0 = glfwGetTime();
  while (glfwWindowShouldClose(window) == false) {
    glfwPollEvents();

    f64 t = glfwGetTime();
    triangle_transformation.mat = glm::rotate(translation, (f32)(t - t0), glm::vec3(0.0f, 0.0f, 1.0f));

    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TriangleTransformation), &triangle_transformation);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(transformed_program);
    glBindVertexArray(transformed_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
  }

  return 0;
}
