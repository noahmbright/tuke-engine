#pragma once

#include "tuke_engine.h"

struct STBHandle {
  int width, height, n_channels;
  unsigned char *data;
};

const char *read_file(const char *path, unsigned long *size = nullptr);
STBHandle load_texture(const char *path);
STBHandle load_texture_metadata(const char *path);
void free_stb_handle(STBHandle *handle);
u32 load_texture_opengl(const char *path);
