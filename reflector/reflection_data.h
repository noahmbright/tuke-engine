#pragma once

#include "reflector.h"
#include <stdio.h>

#define MAX_NUM_VERTEX_ATTRIBUTES 32
#define MAX_NUM_VERTEX_BINDINGS 8
#define MAX_NUM_STRUCT_MEMBERS 8
#define MAX_NUM_DESCRIPTOR_BINDINGS 8
#define MAX_VERTEX_LAYOUT_NAME_LENGTH 128

enum GLSLType {
  GLSL_TYPE_NULL,

  GLSL_TYPE_FLOAT,
  GLSL_TYPE_UINT,
  GLSL_TYPE_VEC2,
  GLSL_TYPE_VEC3,
  GLSL_TYPE_VEC4,
  GLSL_TYPE_MAT2,
  GLSL_TYPE_MAT3,
  GLSL_TYPE_MAT4,

  NUM_GLSL_TYPES,
};

static const char *glsl_type_to_string[NUM_GLSL_TYPES]{
    [GLSL_TYPE_NULL] = "null", [GLSL_TYPE_FLOAT] = "float", [GLSL_TYPE_UINT] = "uint",
    [GLSL_TYPE_VEC2] = "vec2", [GLSL_TYPE_VEC3] = "vec3",   [GLSL_TYPE_VEC4] = "vec4",
    [GLSL_TYPE_MAT2] = "mat2", [GLSL_TYPE_MAT3] = "mat3",   [GLSL_TYPE_MAT4] = "mat4",
};

static const u32 glsl_type_to_size[NUM_GLSL_TYPES]{
    [GLSL_TYPE_NULL] = 0,     [GLSL_TYPE_FLOAT] = 4,    [GLSL_TYPE_UINT] = 0,
    [GLSL_TYPE_VEC2] = 8,     [GLSL_TYPE_VEC3] = 12,    [GLSL_TYPE_VEC4] = 16,
    [GLSL_TYPE_MAT2] = 4 * 4, [GLSL_TYPE_MAT3] = 9 * 4, [GLSL_TYPE_MAT4] = 16 * 4,
};

enum VertexAttributeRate {
  VERTEX_ATTRIBUTE_RATE_NULL,
  VERTEX_ATTRIBUTE_RATE_VERTEX,
  VERTEX_ATTRIBUTE_RATE_INSTANCE,
  NUM_VERTEX_ATTRIBUTE_RATES,
};

static inline const char *vertex_attribute_rate_to_vulkan_enum_string(VertexAttributeRate rate) {
  switch (rate) {
  case VERTEX_ATTRIBUTE_RATE_VERTEX:
    return "VK_VERTEX_INPUT_RATE_VERTEX";
  case VERTEX_ATTRIBUTE_RATE_INSTANCE:
    return "VK_VERTEX_INPUT_RATE_INSTANCE";

  case VERTEX_ATTRIBUTE_RATE_NULL:
  default:
    fprintf(stderr, "vertex_attribute_rate_to_vulkan_enum_string got an invalid rate enum.\n");
    return NULL;
  }
}

// a vertex attribute is a single variable, like a vec3 for position
// the vulkan struct contains location, binding, offset and rate
// in opengl, calls are to
//  glVertexAttribPointer(location, num of type in next arg, GL_FLOAT, GL_FALSE, stride(bytes), (void*)offset(bytes));
struct VertexAttribute {
  u8 location;
  u8 binding;
  u32 offset;
  VertexAttributeRate rate;
  GLSLType glsl_type;
  bool is_tightly_packed;
  // treat a vertex layout as a list of MAX_NUM_VERTEX_ATTRIBUTES attributes, and only emit code for the valid ones
  bool is_valid;
};

inline void log_vertex_attribute(VertexAttribute vertex_attribute) {
  const char *rate_string;
  if (vertex_attribute.rate == VERTEX_ATTRIBUTE_RATE_VERTEX) {
    rate_string = "vertex";
  } else if (vertex_attribute.rate == VERTEX_ATTRIBUTE_RATE_INSTANCE) {
    rate_string = "instance";
  } else {
    rate_string = "invalid";
  }
  printf("Vertex Attribute is\n\tlocation: %u, binding: %u, offset: %u, rate: %s, glsl type %s\n",
         vertex_attribute.location, vertex_attribute.binding, vertex_attribute.offset, rate_string,
         glsl_type_to_string[vertex_attribute.glsl_type]);
}

// the vertex layout is the collection of all the attributes
// this is how we tell the shader how to interpret incoming data
//
// in vulkan, a vertex layout is a list of attributes and a list of bindings
// as far as the shader is concerned, lists of vertex attributes are independent of bindings
// but the vulkan api is aware of bindings
// we can reuse vertex attribute lists across shaders
//
// in opengl to declare a vertex attribute, call
// glVertexAttribPointer(location, num of type in next arg, GL_FLOAT, GL_FALSE, stride(bytes), (void*)offset(bytes));
//  stride and offset are per this binding
// if a binding is accessed per instance, call glVertexAttribDivisor(location, divisor = 1);
struct VertexLayout {
  VertexAttribute attributes[MAX_NUM_VERTEX_ATTRIBUTES];
  u16 binding_strides[MAX_NUM_VERTEX_BINDINGS];
  VertexAttributeRate binding_rates[MAX_NUM_VERTEX_BINDINGS];

  char name[MAX_VERTEX_LAYOUT_NAME_LENGTH];
  u8 name_length;

  u8 binding_count;
  u8 attribute_count;
};

inline void log_vertex_layout(const VertexLayout *vertex_layout) {
  bool found_valid = false;
  for (u32 i = 0; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
    if (vertex_layout->attributes[i].is_valid) {
      if (!found_valid) {
        found_valid = true;
        if (vertex_layout->name_length > 0) {
          printf("Logging Vertex Layout %s:\n", vertex_layout->name);
        } else {
          printf("Logging Vertex Layout with uninitialized name:\n");
        }
      }
      log_vertex_attribute(vertex_layout->attributes[i]);
    }
  }
  if (!found_valid) {
    printf("log_vertex_layout: vertex layout has no valid entries\n");
  }
}

inline bool vertex_layout_equals(const VertexLayout *left, const VertexLayout *right) {
  for (u32 i = 0; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
    bool left_valid = left->attributes[i].is_valid;
    bool right_valid = right->attributes[i].is_valid;
    if (left_valid != right_valid) {
      return false;
    }

    if (left_valid == false) {
      continue;
    }

    bool same_location = (left->attributes[i].location == right->attributes[i].location);
    bool same_type = (left->attributes[i].glsl_type == right->attributes[i].glsl_type);
    bool same_binding = (left->attributes[i].binding == right->attributes[i].binding);
    bool same_rate = (left->attributes[i].rate == right->attributes[i].rate);
    bool same_glsl_type = (left->attributes[i].glsl_type == right->attributes[i].glsl_type);
    if (!(same_type && same_location && same_binding && same_rate && same_glsl_type)) {
      return false;
    }
  }

  return true;
}

// only supporting native GLSL types, no nested structs
struct GLSLStructMember {
  const char *identifier;
  u32 array_length;
  u32 identifier_length;
  GLSLType type;
};

struct GLSLStructMemberList {
  GLSLStructMember members[MAX_NUM_STRUCT_MEMBERS];
  u16 num_members;
};

struct GLSLStruct {
  const char *type_name;
  u32 type_name_length;
  GLSLStructMemberList member_list;
};

inline void log_glsl_struct(const GLSLStruct *glsl_struct) {
  printf("Logging GLSLStruct with %u members %.*s:\n", glsl_struct->member_list.num_members,
         glsl_struct->type_name_length, glsl_struct->type_name);
  for (u32 i = 0; i < glsl_struct->member_list.num_members; i++) {
    GLSLStructMember current_member = glsl_struct->member_list.members[i];
    printf("\t%s %.*s\n", glsl_type_to_string[current_member.type], (int)current_member.identifier_length,
           current_member.identifier);
  }
}

// descriptor sets
struct DescriptorBinding {
  u32 set;
  u32 binding;
};

// these lists of bindings describe what a shader needs. these are data formats
// these can be reused across pipelines. but the descriptor set itself must know
// what numerical set index it corresponds to for vulkan API calls
struct DescriptorBindingList {
  DescriptorBinding bindings[MAX_NUM_DESCRIPTOR_BINDINGS];
  u32 num_bindings;
};

// a descriptor set is a set of all the bindings with the same set id
struct DescriptorSet {
  u32 set_index;
  DescriptorBindingList *binding_list;
};
