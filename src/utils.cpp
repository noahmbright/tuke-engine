#include <glad/gl.h>

#include "camera.h"
#include "stb_image.h"
#include "utils.h"

const char *read_file(const char *path, unsigned long *size) {
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

  if (size)
    *size = bytes_read;
  return buffer;
}

unsigned load_texture(const char *path) {
  stbi_set_flip_vertically_on_load(true);
  int tex_w, tex_h, n_channels;
  unsigned char *tex_data = stbi_load(path, &tex_w, &tex_h, &n_channels, 0);

  unsigned texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (tex_data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_w, tex_h, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, tex_data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    fprintf(stderr, "stbi_load returned nullptr\n");
  }
  stbi_image_free(tex_data);

  return texture;
}
