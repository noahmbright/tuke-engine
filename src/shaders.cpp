#include "shaders.h"

#include <OpenGL/OpenGL.h>
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 512

const char *read_file(const char *path) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s\n", path);
    exit(1);
  }

  fseek(fp, 0L, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp);
  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocte buffer for %s\n", path);
    exit(1);
  }

  long bytes_read = fread(buffer, sizeof(char), file_size, fp);
  if (bytes_read < file_size) {
    fprintf(stderr, "Failed to read correct number of bytes from %s\n", path);
    exit(1);
  }

  buffer[bytes_read] = '\0';
  fclose(fp);
  return buffer;
}

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
