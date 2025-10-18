#pragma once

#include "reflector.h"
#include <cstring>
#include <stdio.h>

#define MAX_NUM_VERTEX_ATTRIBUTES 32
#define MAX_NUM_VERTEX_BINDINGS 8
#define MAX_NUM_STRUCT_MEMBERS 8
#define MAX_NUM_DESCRIPTOR_BINDINGS 8
#define MAX_VERTEX_LAYOUT_NAME_LENGTH 128
#define MAX_DESCRIPTOR_SET_LAYOUT_NAME_LENGTH 256

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
    [GLSL_TYPE_NULL] = 0,     [GLSL_TYPE_FLOAT] = 4,    [GLSL_TYPE_UINT] = 4,
    [GLSL_TYPE_VEC2] = 8,     [GLSL_TYPE_VEC3] = 12,    [GLSL_TYPE_VEC4] = 16,
    [GLSL_TYPE_MAT2] = 4 * 4, [GLSL_TYPE_MAT3] = 9 * 4, [GLSL_TYPE_MAT4] = 16 * 4,
};

// https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
// page 146 has the alignment rules
static const u32 glsl_type_to_alignment[NUM_GLSL_TYPES]{
    [GLSL_TYPE_NULL] = 0,  [GLSL_TYPE_FLOAT] = 4, [GLSL_TYPE_UINT] = 4,  [GLSL_TYPE_VEC2] = 8,  [GLSL_TYPE_VEC3] = 16,
    [GLSL_TYPE_VEC4] = 16, [GLSL_TYPE_MAT2] = 16, [GLSL_TYPE_MAT3] = 16, [GLSL_TYPE_MAT4] = 16,
};

static const char *glsl_type_to_c_type[] = {
    [GLSL_TYPE_FLOAT] = "float",    [GLSL_TYPE_UINT] = "unsigned",  [GLSL_TYPE_VEC2] = "glm::vec2",
    [GLSL_TYPE_VEC3] = "glm::vec3", [GLSL_TYPE_VEC4] = "glm::vec4", [GLSL_TYPE_MAT2] = "glm::mat2",
    [GLSL_TYPE_MAT3] = "glm::mat4", [GLSL_TYPE_MAT4] = "glm::mat4",
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

static const char *vertex_layout_null_string = "VERTEX_LAYOUT_NULL";

// only supporting native GLSL types, no nested structs
struct GLSLStructMember {
  const char *identifier;
  u32 array_length;
  u32 identifier_length;
  GLSLType type; // type encodes alignment, which can vary with backend or usage
};

struct GLSLStructMemberList {
  GLSLStructMember members[MAX_NUM_STRUCT_MEMBERS];
  u32 num_members;
  u32 size;
};

struct GLSLStruct {
  const char *type_name;
  u32 type_name_length;

  const char *discovered_shader_name;
  u32 discovered_shader_name_length;

  GLSLStructMemberList member_list;
};

inline bool glsl_struct_member_list_equals(const GLSLStructMemberList *left, const GLSLStructMemberList *right) {
  if (left->num_members != right->num_members) {
    return false;
  }

  for (u32 i = 0; i < left->num_members; i++) {
    const GLSLStructMember *left_member = &left->members[i];
    const GLSLStructMember *right_member = &right->members[i];

    bool name_lens_are_same = (left_member->identifier_length == right_member->identifier_length);
    if (!name_lens_are_same) {
      return false;
    }

    bool names_are_same =
        (strncmp(left_member->identifier, right_member->identifier, left_member->identifier_length) == 0);
    bool array_lens_are_same = left_member->array_length == right_member->array_length;
    bool types_are_same = left_member->type == right_member->type;
    if (!names_are_same || !array_lens_are_same || !types_are_same) {
      return false;
    }
  }

  return true;
}

inline void log_glsl_struct(FILE *destination, const GLSLStruct *glsl_struct) {
  fprintf(destination, "Logging GLSLStruct with %u members %.*s:\n", glsl_struct->member_list.num_members,
          glsl_struct->type_name_length, glsl_struct->type_name);
  for (u32 i = 0; i < glsl_struct->member_list.num_members; i++) {
    GLSLStructMember current_member = glsl_struct->member_list.members[i];
    fprintf(destination, "\t%s %.*s\n", glsl_type_to_string[current_member.type], (int)current_member.identifier_length,
            current_member.identifier);
  }
}

// Descriptor Sets
//
// A descriptor set layout if the list of all the bindings that a single set uses
//  i.e., set 0 could be camera mvps, so 3 bindings at set 0
// in the vulkan API, you bind the resources at the set indices to make a DescriptorSet
// if a vertex shader and fragment shader are linked, I enforce that if they explictly
// use the same set, that set must have the same list of bindings
enum DescriptorType {
  DESCRIPTOR_TYPE_UNIFORM,
  DESCRIPTOR_TYPE_SAMPLER2D,

  NUM_DESCRIPTOR_TYPES,
  DESCRIPTOR_TYPE_INVALID,
};

// fat struct with all possible data
struct DescriptorBinding {
  DescriptorType type;

  // descriptors all give an identifier, the name of the texture or unform instance
  const char *name;
  u32 name_length;
  u32 descriptor_count;

  // for uniforms
  const GLSLStruct *glsl_struct;
  ShaderStage shader_stage;

  bool is_valid;
};

struct DescriptorSetLayout {
  char name[MAX_DESCRIPTOR_SET_LAYOUT_NAME_LENGTH];
  u8 name_length;

  DescriptorBinding bindings[MAX_NUM_DESCRIPTOR_BINDINGS];
  ShaderStage stage;
  u8 num_bindings;
  u8 set_index; // numerically, which number set this is
};

inline bool descriptor_set_layout_equals(const DescriptorSetLayout *left, const DescriptorSetLayout *right) {
  if (left->num_bindings != right->num_bindings) {
    return false;
  }

  for (u32 i = 0; i < MAX_NUM_DESCRIPTOR_BINDINGS; i++) {
    const DescriptorBinding *left_binding = &left->bindings[i];
    const DescriptorBinding *right_binding = &right->bindings[i];
    if (left_binding->is_valid != right_binding->is_valid) {
      continue;
    }

    // don't bother comparing stage, will merge at pipeline creation time
    bool type_equals = (left_binding->type == right_binding->type);
    bool count_equals = (left_binding->descriptor_count == right_binding->descriptor_count);
    if (!type_equals || !count_equals) {
      return false;
    }
  }

  return true;
}
