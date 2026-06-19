#pragma once

#include <stdint.h>
#include <stdio.h>

#define MAX_NUM_SHADERS 128
#define MAX_NUM_VERTEX_ATTRIBUTES 32
#define MAX_NUM_VERTEX_BINDINGS 8
#define MAX_NUM_STRUCT_MEMBERS 8
#define MAX_NUM_DESCRIPTOR_BINDINGS 8
#define MAX_VERTEX_LAYOUT_NAME_LENGTH 128
#define MAX_DESCRIPTOR_SET_LAYOUT_NAME_LENGTH 256
#define MAX_NUM_DESCRIPTOR_SET_LAYOUTS 16

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

static const char *shader_stage_to_enum_suffix[NUM_SHADER_STAGES] = {
    [SHADER_STAGE_VERTEX] = "_VERT",
    [SHADER_STAGE_FRAGMENT] = "_FRAG",
    [SHADER_STAGE_COMPUTE] = "_COMP",
};

static const char *shader_stage_to_enum_string[NUM_SHADER_STAGES] = {
    [SHADER_STAGE_VERTEX] = "SHADER_STAGE_VERTEX",
    [SHADER_STAGE_FRAGMENT] = "SHADER_STAGE_FRAGMENT",
    [SHADER_STAGE_COMPUTE] = "SHADER_STAGE_COMPUTE",
};

struct GLSLSource {
  const char *string;
  u32 length;
  ShaderStage stage;
};

enum Backend { BACKEND_VULKAN, BACKEND_OPENGL, NUM_BACKENDS };

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

static const char *vertex_layout_null_string = "VERTEX_LAYOUT_NULL";

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
    [GLSL_TYPE_MAT3] = "glm::mat3", [GLSL_TYPE_MAT4] = "glm::mat4",
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
  printf(
      "Vertex Attribute is\n\tlocation: %u, binding: %u, offset: %u, rate: %s, glsl type %s\n",
      vertex_attribute.location, vertex_attribute.binding, vertex_attribute.offset, rate_string,
      glsl_type_to_string[vertex_attribute.glsl_type]
  );
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
    VertexAttribute l = left->attributes[i];
    VertexAttribute r = right->attributes[i];

    bool left_valid = l.is_valid;
    bool right_valid = r.is_valid;
    if (left_valid != right_valid) {
      return false;
    }

    if (left_valid == false) {
      continue;
    }

    bool same_location = (l.location == r.location);
    bool same_type = (l.glsl_type == r.glsl_type);
    bool same_binding = (l.binding == r.binding);
    bool same_rate = (l.rate == r.rate);
    if (!(same_type && same_location && same_binding && same_rate)) {
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
  GLSLType type; // type encodes alignment, which can vary with backend or usage
};

struct GLSLStruct {
  const char *type_name;
  u32 type_name_len;

  const char *discovered_shader_name;
  u32 discovered_shader_name_len;

  GLSLStructMember members[MAX_NUM_STRUCT_MEMBERS];
  u32 num_members;
  u32 size_in_bytes; // Size including padding. Aligned to alignement of struct.
  u32 padding;
};

inline void log_glsl_struct(FILE *dst, const GLSLStruct *glsl_struct) {
  fprintf(
      dst, "Logging GLSLStruct with %u members %.*s:\n", glsl_struct->num_members, glsl_struct->type_name_len,
      glsl_struct->type_name
  );
  for (u32 i = 0; i < glsl_struct->num_members; i++) {
    GLSLStructMember member = glsl_struct->members[i];
    const char *type = glsl_type_to_string[member.type];
    fprintf(dst, "    %s %.*s", type, member.identifier_length, member.identifier);
    if (member.array_length > 0) {
      fprintf(dst, "[%u]", member.array_length);
    }
    fprintf(dst, "\n");
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
  DESCRIPTOR_TYPE_INVALID,
  DESCRIPTOR_TYPE_UNIFORM,
  DESCRIPTOR_TYPE_SAMPLER2D,

  NUM_DESCRIPTOR_TYPES,
};

// Fat struct for uniform and samplers
// To describe a uniform binding, need offset/range
// VkDescriptorSetLayoutBinding is:
//  binding, type(sampler, uniform, etc.), count, stage, immutable sampler*
struct DescriptorBinding {
  DescriptorType type;
  u32 descriptor_count; // For uniform/texture arrays
  u32 stage_flags;

  const GLSLStruct *glsl_struct; // For uniforms

  // Descriptors all give an identifier, the name of the texture or uniform instance
  const char *name;
  u32 name_length;

  const char *discovered_shader_name;
  u32 discovered_shader_name_len;
};

struct DescriptorSetLayout {
  char name[MAX_DESCRIPTOR_SET_LAYOUT_NAME_LENGTH];
  u32 name_len;

  DescriptorBinding bindings[MAX_NUM_DESCRIPTOR_BINDINGS];
  u32 num_bindings;
};
