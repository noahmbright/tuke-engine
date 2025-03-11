#include "shaders.h"
#include "utils.h"

#include <OpenGL/OpenGL.h>
#include <glad/gl.h>

#define BUFFER_SIZE 512

unsigned link_shader_program(const char *vertex_shader_path,
                             const char *fragment_shader_path) {
  const char *vertex_shader_source = read_file(vertex_shader_path);
  const char *fragment_shader_source = read_file(fragment_shader_path);

  int success;
  char info_log[BUFFER_SIZE];
  unsigned vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  unsigned fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to compile vertex shader with info:\n%s", info_log);
  }

  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to compile fragment shader with info:\n%s",
            info_log);
  }

  unsigned shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to link shader program with info:\n%s", info_log);
  }

  free((void *)vertex_shader_source);
  free((void *)fragment_shader_source);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return shader_program;
}

#undef BUFFER_SIZE
