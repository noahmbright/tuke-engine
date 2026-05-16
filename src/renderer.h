#pragma once

// TODO I think I want the reflector to consume type definitions from a nongenerated
// header. Those definitions can be shared here.

#include "tuke_engine.h"

// TODO parse out uniform bindings in reflector
enum RendererPrimitive {
  RENDERER_PRIMITIVE_TRIANGLES,
  RENDERER_PRIMITIVE_TRIANGLE_FAN,
};

// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml
enum BlendFunc {
  BLEND_FUNC_SRC_ALPHA,
  BLEND_FUNC_ONE_MINUS_SRC_ALPHA,
};

enum RenderObjectFlags {
  RENDER_OBJECT_USES_MESH = (1 << 0),
  RENDER_OBJECT_USES_TEXTURE = (1 << 1),
  RENDER_OBJECT_IS_INSTANCED = (1 << 2),
  RENDER_OBJECT_IS_INDEXED = (1 << 3),
};

struct RenderPipeline {
  bool blend;

  bool depth;
  BlendFunc src_blend_func;
  BlendFunc dst_blend_func;
};

struct DrawCall {
  // Shader - single program, or vertex and fragment?
};
