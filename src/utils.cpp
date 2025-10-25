#include <glad/gl.h>

#include "camera.h"
#include "stb_image.h"
#include "utils.h"

const char *read_file(const char *path, unsigned long *size) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s\n", path);
    exit(1);
  }

  fseek(fp, 0L, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp);
  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate buffer for %s\n", path);
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

STBHandle load_texture_metadata(const char *path) {
  STBHandle handle;
  if (!stbi_info(path, &handle.width, &handle.height, &handle.n_channels)) {
    fprintf(stderr, "load_texture_metadata: Failed to load %s, for reason %s\n", path, stbi_failure_reason());
  }
  handle.data = NULL;
  return handle;
}

STBHandle load_texture(const char *path) {
  STBHandle handle;
  stbi_set_flip_vertically_on_load(true);
  handle.data = stbi_load(path, &handle.width, &handle.height, &handle.n_channels, 4);
  handle.n_channels = 4;
  if (!handle.data) {
    fprintf(stderr, "load_texture: Failed to stbi_load %s\n", path);
    exit(1);
  }
  return handle;
}

void free_stb_handle(STBHandle *handle) {
  if (handle->data) {
    stbi_image_free(handle->data);
  } else {
    fprintf(stderr, "free_stb_handle: Tried to free NULL STBHandle");
  }
  handle->data = NULL;
}

unsigned load_texture_opengl(const char *path) {
  STBHandle handle = load_texture(path);
  GLenum format = (handle.n_channels == 4) ? GL_RGBA : GL_RGB;
  unsigned texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (handle.data) {
    glTexImage2D(GL_TEXTURE_2D, 0, format, handle.width, handle.height, 0, format, GL_UNSIGNED_BYTE, handle.data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    fprintf(stderr, "stbi_load returned nullptr\n");
  }

  free_stb_handle(&handle);

  return texture;
}
