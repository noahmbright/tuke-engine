#pragma once

#include "reflector.h"
#include <stdio.h>

#define MAX_NUM_VERTEX_ATTRIBUTES 8
#define MAX_NUM_STRUCT_MEMBERS 8

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
    [GLSL_TYPE_MAT2] = "mac2", [GLSL_TYPE_MAT3] = "mac3",   [GLSL_TYPE_MAT4] = "mac4",

};

enum VertexAttributeRate { VERTEX_ATTRIBUTE_RATE_VERTEX, VERTEX_ATTRIBUTE_RATE_INSTANCE, NUM_VERTEX_ATTRIBUTE_RATES };

// a vertex attribute is a single variable, like a vec3 for position
struct VertexAttribute {
  u8 location;
  u8 binding;
  u32 offset;
  VertexAttributeRate rate;
  GLSLType glsl_type;
  bool is_tightly_packed;
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

inline bool vertex_attribute_equal(VertexAttribute left, VertexAttribute right) {
  bool location_equals = (left.location == right.location);
  bool binding_equals = (left.binding == right.binding);
  bool offset_equals = (left.offset == right.offset);
  bool glsl_type_equals = (left.glsl_type == right.glsl_type);
  bool rate_equals = (left.rate == right.rate);
  return location_equals && binding_equals && offset_equals && glsl_type_equals && rate_equals;
}

// the vertex layout is the collection of all the attributes
// this is how we tell the shader how to interpret incoming data
struct VertexLayout {
  VertexAttribute attributes[MAX_NUM_VERTEX_ATTRIBUTES];
  u8 num_attributes;
};

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
