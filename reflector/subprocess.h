#pragma once

#include "reflector.h"

struct SpirVBytesArray {
  const u8 *bytes;
  u32 length;
};

SpirVBytesArray compile_to_spirv(GLSLSource glsl_source, ShaderStage stage);
