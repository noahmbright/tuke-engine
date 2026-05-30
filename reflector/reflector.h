#pragma once

#include <stdint.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

enum ShaderStage {
  SHADER_STAGE_VERTEX = 1 << 0,
  SHADER_STAGE_FRAGMENT = 1 << 1,
  SHADER_STAGE_COMPUTE = 1 << 2,

  // TODO may want to support putting vertex and fragment shaders in one file.
  //      This is unused placeholder right now. Parser will need to understand what
  //      stage it's on after loading the file. Current approach uses filename to
  //      figure out what stage.
  SHADER_STAGE_COMBINED = 1 << 3, // Should never set this

  NUM_SHADER_STAGES
};

static const char *shader_stage_to_string[NUM_SHADER_STAGES] = {
    [SHADER_STAGE_VERTEX] = "vert",
    [SHADER_STAGE_FRAGMENT] = "frag",
    [SHADER_STAGE_COMPUTE] = "comp",
};

static const char *shader_stage_to_enum_string[NUM_SHADER_STAGES] = {
    [SHADER_STAGE_VERTEX] = "SHADER_STAGE_VERTEX",
    [SHADER_STAGE_FRAGMENT] = "SHADER_STAGE_FRAGMENT",
    [SHADER_STAGE_COMPUTE] = "SHADER_STAGE_COMPUTE",
};

struct GLSLSource {
  const char *string;
  u32 length;
};
