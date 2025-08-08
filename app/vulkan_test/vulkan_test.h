#pragma once
#include "tuke_engine.h"
#include "vulkan_base.h"
#include <cstring>

// clang-format off
const f32 triangle_vertices[] = {
  0.0f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 
  0.5f, 0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.0f,  0.0f, 0.0f, 1.0f
};

const f32 square_vertices[] = {
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.33f, 0.0f, 0.0f, 0.0f, 1.0f,
  0.67f, 0.67f, 0.0f, 0.0f, 0.0f, 1.0f,
};

// TL, BL, BR, TR
const f32 unit_square_positions[] = {
   // x, y, z, u, v
  -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
   0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
   0.5f,  0.5f, 0.0f, 1.0f, 1.0f
};

const u16 unit_square_indices[] = {
  0, 1, 2, 0, 2, 3,
};

const u32 instanced_quad_count = 5;
const f32 quad_positions[] = {
  -0.5, -0.5
  -0.1, -0.1
  -0.1, -0.1
  -0.3, -0.3
  -0.3, -0.3
};
// clang-format on

enum TextureId {
  TEXTURE_GENERIC_GIRL,
  TEXTURE_GIRL_FACE,
  TEXTURE_GIRL_FACE_NORMAL_MAP,
  NUM_TEXTURES
};

static const char *texture_names[NUM_TEXTURES] = {
    "textures/generic_girl.jpg", "textures/girl_face.jpg",
    "textures/girl_face_normal_map.jpg"};

#define MAX_TEXTURES (4)
#define MAX_UNIFORMS (4)
#define UNIFORM_ALIGNMENT 256

u32 align_up(u32 value, u32 alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

struct UniformDescriptorUpload {
  u32 size;
  u32 offset;
};

// TODO this is not sensitive to alignment rules, ugh
// unclear if this should be handled in reflection or here
struct DescriptorSetUploader {
  u32 num_uniforms;
  UniformDescriptorUpload uniform_descriptor_uploads[MAX_UNIFORMS];

  u32 num_textures;
  u32 num_samplers;
};

DescriptorSetUploader new_descriptor_set_uploader() {
  DescriptorSetUploader uploader;

  uploader.num_uniforms = 0;
  uploader.num_textures = 0;
  uploader.num_samplers = 0;

  memset(uploader.uniform_descriptor_uploads, 0,
         sizeof(uploader.uniform_descriptor_uploads));

  return uploader;
}

void upload_uniform_descriptor(DescriptorSetUploader *uploader, u32 size) {
  u32 i = uploader->num_uniforms;
  assert(i < MAX_UNIFORMS);

  u32 offset = 0;
  if (i > 0) {
    u32 last_offset = uploader->uniform_descriptor_uploads[i - 1].offset;
    u32 last_size = uploader->uniform_descriptor_uploads[i - 1].size;
    offset = align_up(last_offset + last_size, UNIFORM_ALIGNMENT);
  }

  uploader->uniform_descriptor_uploads[i].offset = offset;
  uploader->uniform_descriptor_uploads[i].size = size;
  ++uploader->num_uniforms;
}

// a shader file that I write prescribes a bunch of stuff:
// the program itself, compiled into spirv
// the vertex layout to use
// the descriptor set to use
struct Material {
  UniformBuffer *uniform_buffers;
};
