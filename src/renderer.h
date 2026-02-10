#pragma once

// TODO parse out uniform bindings in reflector

enum RenderObjectFlags {
  RENDER_OBJECT_USES_MESH = (1 << 0),
  RENDER_OBJECT_USES_TEXTURE = (1 << 1),
  RENDER_OBJECT_IS_INSTANCED = (1 << 2),
  RENDER_OBJECT_IS_INDEXED = (1 << 3),
};

struct RenderObject {};
