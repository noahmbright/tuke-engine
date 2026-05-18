#pragma once

#include "stb_image.h"
#include "tuke_engine.h"

struct ImageData {
  u32 width;
  u32 height;
  u32 n_channels;
  u8 *data;
};
