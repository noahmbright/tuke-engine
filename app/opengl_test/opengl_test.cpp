#include "opengl_test.h"
#include "generated_shader_utils.h"
#include "window.h"

int main() {
  GLFWwindow *window = new_window(false /* is vulkan */);

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

  while (glfwWindowShouldClose(window) == false) {
    glfwPollEvents();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
  }

  return 0;
}
