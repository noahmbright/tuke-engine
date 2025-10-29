#pragma once

#include "tilemap.h"
#include "tuke_engine.h"

const u32 width_in_tiles = 3;
const u32 height_in_tiles = 2;

// clang-format off
const u8 tilemap_data[width_in_tiles * height_in_tiles] = {
    0, 0, 1,
    1, 1, 1
};

const Tilemap tilemap = {
    .level_map = &tilemap_data[0],
    .level_width = width_in_tiles,
    .level_height = height_in_tiles
};
// clang-format on
