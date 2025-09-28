#pragma once

#include "reflector.h"

struct SpirVBytesArray {
  const u8 *bytes;
  u32 length;
};

SpirVBytesArray compile_vulkan_source_to_glsl(GLSLSource glsl_source, ShaderStage stage);
