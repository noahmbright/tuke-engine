#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"
#include "reflection_data.h"
#include "reflector.h"
#include "subprocess.h"

#include <assert.h>
#include <cstring>

static const u32 max_num_vertex_attributes = 4;

static u32 align_up(u32 offset, u32 alignment) { return (offset + alignment - 1) & ~(alignment - 1); }

static u32 compute_glsl_struct_size(const GLSLStruct *glsl_struct) {
  u32 max_alignment = 0;
  u32 size = 0;
  for (u32 i = 0; i < glsl_struct->member_list.num_members; i++) {
    const GLSLStructMember *member = &glsl_struct->member_list.members[i];
    u32 alignment = glsl_type_to_alignment[member->type];
    if (alignment > max_alignment)
      max_alignment = alignment;
    size = align_up(size, alignment);
    size += glsl_type_to_size[member->type];
  }
  return align_up(size, max_alignment);
}

// line with version is #version {{ VERSION }}
TemplateStringReplacement directive_replacement_version(GraphicsBackend backend) {
  TemplateStringReplacement replacement;

  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
    replacement.string = "410 core\n\n";
    break;
  case GRAPHICS_BACKEND_VULKAN:
    replacement.string = "450\n\n";
    break;
  default:
    assert(false);
  }

  replacement.length = strlen(replacement.string);
  return replacement;
}

TemplateStringReplacement directive_replacement_vertex_index(GraphicsBackend backend) {
  TemplateStringReplacement replacement;

  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
    replacement.string = "gl_VertexID";
    break;
  case GRAPHICS_BACKEND_VULKAN:
    replacement.string = "gl_VertexIndex";
    break;
  default:
    assert(false);
  }

  replacement.length = strlen(replacement.string);
  return replacement;
}

TemplateStringReplacement directive_replacement_instance_index(GraphicsBackend backend) {
  TemplateStringReplacement replacement;

  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
    replacement.string = "gl_InstanceID";
    break;
  case GRAPHICS_BACKEND_VULKAN:
    replacement.string = "gl_InstanceIndex";
    break;
  default:
    assert(false);
  }

  replacement.length = strlen(replacement.string);
  return replacement;
}

// {{ LOCATION 0 BINDING 0 RATE_VERTEX OFFSET TIGHTLY_PACKED }}
// turns into
// layout(location = 0) for both opengl and vulkan
TemplateStringReplacement directive_replacement_location(GraphicsBackend backend) {
  TemplateStringReplacement replacement;
  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
  case GRAPHICS_BACKEND_VULKAN: {
    replacement.string = "layout(location = __) ";
    replacement.length = strlen(replacement.string);
    return replacement;
  }

  default:
    assert(false);
  }

  return replacement;
}

// only supporting set and binding numbers < 10
// {{ SET_BINDING 0 0 SET_LABEL GLOBAL }}
// turns into
// layout(set = 0, binding = 0) for vulkan
// layout(binding = 0) for opengl
TemplateStringReplacement directive_replacement_set_binding(GraphicsBackend backend, DescriptorType descriptor_type) {
  TemplateStringReplacement replacement;

  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
    if (descriptor_type == DESCRIPTOR_TYPE_SAMPLER2D) {
      replacement.string = "";
      replacement.length = 0;
    } else if (descriptor_type == DESCRIPTOR_TYPE_UNIFORM) {
      replacement.string = "layout(std140) ";
      replacement.length = strlen(replacement.string);
    }
    return replacement;

  case GRAPHICS_BACKEND_VULKAN: {
    replacement.string = "layout(set = _, binding = _) ";
    replacement.length = strlen(replacement.string);
    return replacement;
  }

  default:
    assert(false);
  }

  return replacement;
}

TemplateStringReplacement perform_replacement(const TemplateStringSlice *string_slice, GraphicsBackend backend) {

  TemplateStringReplacement replacement;
  replacement.string = NULL;
  replacement.length = 0;

  switch (string_slice->type) {
  case DIRECTIVE_TYPE_VERSION:
    return directive_replacement_version(backend);
  case DIRECTIVE_TYPE_VERTEX_INDEX:
    return directive_replacement_vertex_index(backend);
  case DIRECTIVE_TYPE_INSTANCE_INDEX:
    return directive_replacement_instance_index(backend);
  case DIRECTIVE_TYPE_LOCATION:
    return directive_replacement_location(backend);
  case DIRECTIVE_TYPE_SET_BINDING:
    return directive_replacement_set_binding(backend, string_slice->descriptor_type);
  case DIRECTIVE_TYPE_PUSH_CONSTANT:
    // return replacement;
  case DIRECTIVE_TYPE_GLSL_SOURCE:
    replacement.string = string_slice->start;
    replacement.length = u32(string_slice->end - string_slice->start);
    return replacement;
  }

  return replacement;
}

GLSLSource replace_string_slices(const ParsedShader *sliced_shader, GraphicsBackend backend) {
  u32 compiled_source_length = 0;
  u32 num_string_slices = sliced_shader->num_template_slices;
  const TemplateStringSlice *string_slices = sliced_shader->template_slices;
  TemplateStringReplacement replacements[MAX_NUM_STRING_SLICES];
  memset(&replacements, 0, sizeof(replacements));

  // first pass, get replacements and accumulate lengths
  for (u32 i = 0; i < num_string_slices; i++) {
    const TemplateStringSlice *string_slice = &string_slices[i];
    replacements[i] = perform_replacement(string_slice, backend);
    compiled_source_length += replacements[i].length;
  }

  u32 source_index = 0;
  char *compiled_source = (char *)malloc(sizeof(char) * (compiled_source_length + 1));

  for (u32 i = 0; i < num_string_slices; i++) {
    const TemplateStringSlice *string_slice = &string_slices[i];
    const TemplateStringReplacement *replacement = &replacements[i];

    char *current_start = compiled_source + source_index;
    memcpy(current_start, replacement->string, replacement->length);

    if (string_slice->type == DIRECTIVE_TYPE_LOCATION) {
      assert(string_slice->location < 100);
      if (string_slice->location < 10) {
        // layout(location = __)
        // first _ is [18]
        current_start[18] = ' ';
        current_start[19] = '0' + string_slice->location;
      } else {
        current_start[18] = '0' + string_slice->location / 10;
        current_start[19] = '0' + string_slice->location % 10;
      }
    } else if (string_slice->type == DIRECTIVE_TYPE_SET_BINDING) {
      assert(string_slice->set < 10);
      assert(string_slice->binding < 10);

      if (backend == GRAPHICS_BACKEND_VULKAN) {
        // layout(set = _, binding = _) _ at 13, 26
        current_start[13] = '0' + string_slice->set;
        current_start[26] = '0' + string_slice->binding;
      }
    }

    source_index += replacement->length;
  }

  compiled_source[compiled_source_length] = '\0';

  GLSLSource glsl_source = {
      .length = compiled_source_length,
      .string = compiled_source,
  };
  return glsl_source;
}

inline void codegen_compiled_shader_header(FILE *dst) {
  fprintf(dst, "// Generated shader header, do not edit\n");
  fprintf(dst, "#pragma once\n");
  fprintf(dst, "#include \"glad/gl.h\"\n");
  fprintf(dst, "#include <assert.h>\n");
  fprintf(dst, "#include <stddef.h>\n");
  fprintf(dst, "#include <stdint.h>\n");
  fprintf(dst, "#include <stdio.h>\n");
  fprintf(dst, "#include <vulkan/vulkan.h>\n");
  fprintf(dst, "#include <glm/glm.hpp>\n");
  fprintf(dst, "\n");

  fprintf(dst, "#define MAX_NUM_VERTEX_BINDINGS %u\n", MAX_NUM_VERTEX_BINDINGS);
  fprintf(dst, "#define MAX_NUM_VERTEX_ATTRIBUTES %u\n", max_num_vertex_attributes);
  fprintf(dst, "\n");
}

static inline void codegen_shader_spec_struct_definition(FILE *dst) {
  fprintf(dst, "struct ShaderSpec {\n");
  fprintf(dst, "  const char* name;\n");
  fprintf(dst, "  const char* opengl_glsl;\n");
  fprintf(dst, "  const uint32_t* spv;\n");
  fprintf(dst, "  const uint32_t spv_size;\n");
  fprintf(dst, "  VertexLayoutID vertex_layout_id;\n");
  fprintf(dst, "};\n");
  fprintf(dst, "\n");
}

inline char ascii_to_upper(char c) {
  // lowercase letters are uppercase + 32, difference is in 5th bit
  // to unset the 5th bit, & with 1111....01111
  // 0s will stay 0s, 1s will stay 1s, except for the 5th bit
  u8 is_lower = (u8)(c - 'a') < 26;
  return c & ~(is_lower << 5);
}

inline void print_name_in_caps(FILE *dst, const char *name) {
  for (const char *s = name; *s; s++) {
    fputc(ascii_to_upper(*s), dst);
  }
}

inline void print_name_in_caps_n(FILE *dst, const char *s, u32 n) {
  for (u32 i = 0; i < n; i++) {
    fputc(ascii_to_upper(s[i]), dst);
  }
}

const char *glsl_type_to_vulkan_format(GLSLType type) {
  switch (type) {

  case GLSL_TYPE_VEC2:
    return "VK_FORMAT_R32G32_SFLOAT";
  case GLSL_TYPE_VEC3:
    return "VK_FORMAT_R32G32B32_SFLOAT";
  case GLSL_TYPE_FLOAT:
    return "VK_FORMAT_R32_SFLOAT";
  case GLSL_TYPE_UINT:
    return "VK_FORMAT_R32_UINT";

  case GLSL_TYPE_VEC4:
  case GLSL_TYPE_MAT2:
  case GLSL_TYPE_MAT3:
  case GLSL_TYPE_MAT4:
  case GLSL_TYPE_NULL:
  default:
    fprintf(stderr, "glsl_type_to_vulkan_format got an invalid GLSLType enum.\n");
    return NULL;
  }
}

void generate_vulkan_vertex_layout_array(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "//////////////////// VULKAN VERTEX LAYOUTS ///////////////\n");

  // static arrays, needed bc initializing VkPipelineVertexInputStateCreateInfo as const requires pointing arrays
  for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &ir->vertex_layouts[i];
    if (vertex_layout->attribute_count == 0 && vertex_layout->binding_count == 0) {
      continue;
    }

    // bindings
    fprintf(dst, "const VkVertexInputBindingDescription vertex_bindings_%s[] = {\n", vertex_layout->name);
    for (u32 i = 0; i < MAX_NUM_VERTEX_BINDINGS; i++) {
      bool has_stride = (vertex_layout->binding_strides[i] > 0);
      bool has_rate = (vertex_layout->binding_rates[i] != VERTEX_ATTRIBUTE_RATE_NULL);
      if (has_stride != has_rate) {
        fprintf(
            stderr,
            "Generating binding code for vertex layout %s, but rate and stride are not both present for binding "
            "%u.\n",
            vertex_layout->name, i
        );
        if (!has_stride) {
          fprintf(stderr, "  stride is missing.\n");
        } else if (!has_rate) {
          fprintf(stderr, "  rate is missing.\n");
        } else {
          fprintf(stderr, "  both stride and rate are missing.\n");
        }
        continue;
      }

      if (!has_rate || vertex_layout->binding_count == 0) {
        continue;
      }

      fprintf(dst, "{ .binding = %u, ", i);
      fprintf(dst, ".stride = %u, ", vertex_layout->binding_strides[i]);
      fprintf(
          dst, ".inputRate = %s },\n", vertex_attribute_rate_to_vulkan_enum_string(vertex_layout->binding_rates[i])
      );
    }
    fprintf(dst, "};\n\n"); // close array of bindings

    // attributes
    fprintf(dst, "const VkVertexInputAttributeDescription vertex_attributes_%s[] = {\n", vertex_layout->name);
    for (u32 i = 0; i < vertex_layout->attribute_count; i++) {
      fprintf(dst, "  {  .location = %u, ", vertex_layout->attributes[i].location);
      fprintf(dst, ".binding = %u, ", vertex_layout->attributes[i].binding);
      fprintf(dst, ".format = %s,", glsl_type_to_vulkan_format(vertex_layout->attributes[i].glsl_type));
      fprintf(dst, ".offset = %u, }\n,", vertex_layout->attributes[i].offset);
    }
    fprintf(dst, "};\n\n"); // close attributes
  }

  // array init
  fprintf(
      dst, "const VkPipelineVertexInputStateCreateInfo generated_vulkan_vertex_layouts[NUM_VERTEX_LAYOUT_IDS] = {\n"
  );
  for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &ir->vertex_layouts[i];

    if (vertex_layout->attribute_count > max_num_vertex_attributes) {
      fprintf(
          stderr, "Vertex Layout %s has %u attributes, exceeding the max of %u.\n", vertex_layout->name,
          vertex_layout->attribute_count, max_num_vertex_attributes
      );
      continue;
    }
    if (vertex_layout->binding_count > MAX_NUM_VERTEX_BINDINGS) {
      fprintf(
          stderr, "Vertex Layout %s has %u bindings, exceeding the max of %u.\n", vertex_layout->name,
          vertex_layout->binding_count, MAX_NUM_VERTEX_BINDINGS
      );
      continue;
    }

    fprintf(dst, "  [%s] = {\n", vertex_layout->name);
    // boilerplate
    fprintf(dst, "    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,\n");
    fprintf(dst, "    .pNext = NULL,\n");
    fprintf(dst, "    .flags = 0,\n");

    // bindings
    fprintf(dst, "    .vertexBindingDescriptionCount  = %u,\n", vertex_layout->binding_count);
    if (vertex_layout->binding_count != 0) {
      fprintf(dst, "    .pVertexBindingDescriptions  = vertex_bindings_%s,\n", vertex_layout->name);
    } else {
      fprintf(dst, "    .pVertexBindingDescriptions  = NULL,\n");
    }

    // attributes
    fprintf(dst, "    .vertexAttributeDescriptionCount  = %u,\n", vertex_layout->attribute_count);
    if (vertex_layout->attribute_count != 0) {
      fprintf(dst, "    .pVertexAttributeDescriptions  = vertex_attributes_%s,\n", vertex_layout->name);
    } else {
      fprintf(dst, "    .pVertexAttributeDescriptions  = NULL,\n");
    }

    fprintf(dst, "  },\n"); // close this vertex layout
  }
  fprintf(dst, "};\n\n"); // close entire vulkan vertex layout array init
}

static u32 glsl_type_to_number_of_floats(GLSLType type) {
  switch (type) {

  case GLSL_TYPE_UINT:
  case GLSL_TYPE_FLOAT:
    return 1;

  case GLSL_TYPE_VEC2:
    return 2;

  case GLSL_TYPE_VEC3:
    return 3;

  case GLSL_TYPE_MAT2:
  case GLSL_TYPE_VEC4:
    return 4;

  case GLSL_TYPE_MAT3:
    return 9;

  case GLSL_TYPE_MAT4:
    return 16;

  case GLSL_TYPE_NULL:
  default:
    fprintf(stderr, "glsl_type_to_number_of_floats got an GLSLType.\n");
    return 0;
  }
}

void generate_opengl_vertex_layout_array(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "//////////////////// OPENGL VERTEX LAYOUTS ///////////////\n");

  const char *parameter_list = "GLuint vao, GLuint* vbos, uint32_t num_vbos, GLuint ebo";

  for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &ir->vertex_layouts[i];
    if (vertex_layout->binding_count == 0 && vertex_layout->attribute_count == 0) {
      continue;
    }

    // emit init function definition
    fprintf(dst, "inline void init_vertex_layout%s(%s){\n", vertex_layout->name, parameter_list);
    fprintf(
        dst,
        "  if(num_vbos != %u){\n    fprintf(stderr, \"opengl %s vertex layout got wrong number of bindings\");\n"
        "  return;\n  }\n\n",
        vertex_layout->binding_count, vertex_layout->name
    );

    fprintf(dst, "  glBindVertexArray(vao);\n");
    fprintf(dst, "  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);\n\n");

    // emit attributes
    u32 current_binding = -1; // impossible sentinel value, hopefully
    for (u32 i = 0; i < vertex_layout->attribute_count; i++) {
      const VertexAttribute *attribute = &vertex_layout->attributes[i];
      u32 binding_stride = vertex_layout->binding_strides[attribute->binding];

      if (attribute->binding != current_binding) {
        fprintf(dst, "  glBindBuffer(GL_ARRAY_BUFFER, vbos[%u]);\n", attribute->binding);
        current_binding = attribute->binding;
      }
      fprintf(dst, "  glEnableVertexAttribArray(%u);\n", attribute->location);

      // supporting GL_FLOAT and GL_UNSIGNED_INT for now
      // only konw that in uint takes glVertexAttribIPointer(n, m, GL_UNSIGNED_INT,.. ) for now
      if (attribute->glsl_type == GLSL_TYPE_UINT) {
        fprintf(
            dst, "  glVertexAttribIPointer(%u, %u, GL_UNSIGNED_INT, %u, (void*)%u);\n", attribute->location,
            glsl_type_to_number_of_floats(attribute->glsl_type), binding_stride, attribute->offset
        );
      } else {
        fprintf(
            dst, "  glVertexAttribPointer(%u, %u, GL_FLOAT, GL_FALSE, %u, (void*)%u);\n", attribute->location,
            glsl_type_to_number_of_floats(attribute->glsl_type), binding_stride, attribute->offset
        );
      }

      if (attribute->rate == VERTEX_ATTRIBUTE_RATE_INSTANCE) {
        fprintf(dst, "  glVertexAttribDivisor(%u, 1);\n", attribute->location);
      } else {
        fprintf(dst, "\n");
      }
    }

    fprintf(dst, "\n  glBindVertexArray(0);\n");
    fprintf(dst, "  glBindBuffer(GL_ARRAY_BUFFER, 0);\n");
    fprintf(dst, "  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);\n");
    fprintf(dst, "}\n\n");
  }

  // populate function pointer array
  fprintf(dst, "typedef void (*VertexLayoutInitFn)(%s);\n", parameter_list);
  fprintf(dst, "const VertexLayoutInitFn generated_opengl_vertex_array_initializers[NUM_VERTEX_LAYOUT_IDS] = {\n");
  for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &ir->vertex_layouts[i];
    if (vertex_layout->attribute_count == 0 && vertex_layout->binding_count == 0) {
      fprintf(dst, "  [%s] = NULL,\n", vertex_layout->name);
    } else {
      fprintf(dst, "  [%s] = init_vertex_layout%s,\n", vertex_layout->name, vertex_layout->name);
    }
  }
  fprintf(dst, "};\n\n");
}

static inline const char *get_stage_flags_string(u32 stage_flags) {
  bool vertex_visible = stage_flags & SHADER_STAGE_VERTEX;
  bool fragment_visible = stage_flags & SHADER_STAGE_FRAGMENT;
  bool compute_visible = stage_flags & SHADER_STAGE_COMPUTE;

  const char *flags_string;
  if (compute_visible && fragment_visible && vertex_visible) {
    flags_string = "VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT";
  } else if (fragment_visible && vertex_visible) {
    flags_string = "VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT";
  } else if (fragment_visible && compute_visible) {
    flags_string = "VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT";
  } else if (vertex_visible && compute_visible) {
    flags_string = "VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT";
  } else if (vertex_visible) {
    flags_string = "VK_SHADER_STAGE_VERTEX_BIT";
  } else if (fragment_visible) {
    flags_string = "VK_SHADER_STAGE_FRAGMENT_BIT";
  } else if (compute_visible) {
    flags_string = "VK_SHADER_STAGE_COMPUTE_BIT";
  } else {
    assert(false && "stage_flags = 0");
    flags_string = NULL;
  }

  return flags_string;
}

static void generate_vulkan_descriptor_set_binding_lists(FILE *dst, const ParsedShadersIR *ir) {
  for (u32 i = 0; i < ir->num_descriptor_set_layouts; i++) {
    const DescriptorSetLayout *layout = &ir->descriptor_set_layouts[i];
    fprintf(
        dst, "static VkDescriptorSetLayoutBinding %.*s_descriptor_set_layout_bindings[] = {\n", layout->name_length,
        layout->name
    );

    for (u32 j = 0; j < MAX_NUM_DESCRIPTOR_BINDINGS; j++) {
      DescriptorBinding binding = layout->bindings[j];
      if (binding.type == DESCRIPTOR_TYPE_INVALID) {
        continue;
      }

      // Support for descriptor types is limited currently.
      // Would really like to better understand how to separate samplers from images.
      // Might just move type string into its own routine now.
      const char *type_string = binding.type == DESCRIPTOR_TYPE_UNIFORM ? "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER"
                                                                        : "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
      const char *flags_string = get_stage_flags_string(binding.stage_flags);

      fprintf(dst, "  {\n");
      fprintf(dst, "    .binding = %u,\n", j);
      fprintf(dst, "    .descriptorType = %s,\n", type_string);
      fprintf(dst, "    .descriptorCount = %u,\n", binding.descriptor_count);
      fprintf(dst, "    .stageFlags = %s,\n", flags_string);
      fprintf(dst, "    .pImmutableSamplers = NULL,\n");
      fprintf(dst, "  },\n");
    }

    fprintf(dst, "};\n");

    // Size
    fprintf(
        dst, "static uint32_t %.*s_num_descriptor_set_layout_bindings = %u;\n\n", layout->name_length, layout->name,
        layout->num_bindings
    );
  }
}

static void generate_vulkan_descriptor_write_templates(FILE *dst, const ParsedShadersIR *ir) {
  for (u32 i = 0; i < ir->num_descriptor_set_layouts; i++) {
    const DescriptorSetLayout *layout = &ir->descriptor_set_layouts[i];

    fprintf(dst, "static VkWriteDescriptorSet %.*s_write_templates[] = {\n", layout->name_length, layout->name);
    for (u32 j = 0; j < MAX_NUM_DESCRIPTOR_BINDINGS; j++) {
      const DescriptorBinding *binding = &layout->bindings[j];
      if (binding->type == DESCRIPTOR_TYPE_INVALID) {
        continue;
      }

      const char *type_string = binding->type == DESCRIPTOR_TYPE_UNIFORM ? "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER"
                                                                         : "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
      fprintf(dst, "  {\n");
      fprintf(dst, "    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,\n");
      fprintf(dst, "    .pNext = NULL,\n");
      fprintf(dst, "    .dstSet = VK_NULL_HANDLE,\n");
      fprintf(dst, "    .dstBinding = %u,\n", j);
      fprintf(dst, "    .dstArrayElement = 0,\n");
      fprintf(dst, "    .descriptorCount = %u,\n", binding->descriptor_count);
      fprintf(dst, "    .descriptorType = %s,\n", type_string);
      fprintf(dst, "    .pImageInfo = NULL,\n");
      fprintf(dst, "    .pBufferInfo = NULL,\n");
      fprintf(dst, "    .pTexelBufferView = NULL,\n");
      fprintf(dst, "  },\n");
    }
    fprintf(dst, "};\n\n");

    fprintf(dst, "static VkDeviceSize %.*s_ranges[] = {\n", layout->name_length, layout->name);
    for (u32 j = 0; j < MAX_NUM_DESCRIPTOR_BINDINGS; j++) {
      const DescriptorBinding *binding = &layout->bindings[j];
      if (binding->type == DESCRIPTOR_TYPE_INVALID) {
        continue;
      }

      bool has_struct = (binding->type == DESCRIPTOR_TYPE_UNIFORM && binding->glsl_struct != NULL);
      u32 size = has_struct ? compute_glsl_struct_size(binding->glsl_struct) : 0;
      fprintf(dst, "  %u,\n", size);
    }
    fprintf(dst, "};\n\n");
  }
}

static void generate_vulkan_descriptor_pool_size_array(FILE *dst, const ParsedShadersIR *ir) {
  u32 max_sets = 0;
  for (u32 i = 0; i < NUM_DESCRIPTOR_TYPES; i++) {
    u32 num_sets = ir->descriptor_binding_types[i];
    max_sets = (max_sets > num_sets) ? max_sets : num_sets;
  }

  // TODO Subtract 1 for DESCRIPTOR_TYPE_INVALID - would like to have less implicit coupling.
  // Keeping DESCRIPTOR_TYPE_INVALID = 0 in the enum makes 0 init nice, which is also implicit.
  fprintf(dst, "const uint32_t pool_size_count = %u;\n", NUM_DESCRIPTOR_TYPES - 1);
  fprintf(dst, "const uint32_t max_descriptor_sets = %u;\n", max_sets);
  fprintf(dst, "const VkDescriptorPoolSize generated_pool_sizes[%u] = {\n", NUM_DESCRIPTOR_TYPES);

  fprintf(
      dst, "  { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = %u },\n",
      ir->descriptor_binding_types[DESCRIPTOR_TYPE_UNIFORM]
  );

  fprintf(
      dst, "  { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = %u },\n",
      ir->descriptor_binding_types[DESCRIPTOR_TYPE_SAMPLER2D]
  );
  fprintf(dst, "};\n\n");
}

static void codegen_struct_defintions(FILE *dst, const GLSLStruct *glsl_structs, u32 num_glsl_structs) {

  for (u32 i = 0; i < num_glsl_structs; i++) {
    const GLSLStruct *glsl_struct = &glsl_structs[i];

    // precompute stuff
    u32 max_alignment = 0;
    u32 struct_size = 0;
    for (u32 j = 0; j < glsl_struct->member_list.num_members; j++) {
      const GLSLStructMember *member = &glsl_struct->member_list.members[j];
      if (member->array_length > 1) {
        fprintf(
            dst, "const uint32_t %.*s_%.*s_array_size = %u;\n", glsl_struct->type_name_length, glsl_struct->type_name,
            member->identifier_length, member->identifier, member->array_length
        );
      }

      u32 alignment = glsl_type_to_alignment[member->type];
      max_alignment = (max_alignment > alignment) ? max_alignment : alignment;
      u32 next_aligned_size = align_up(struct_size, alignment);
      struct_size = next_aligned_size;
      struct_size += glsl_type_to_size[member->type];
    }

    // codegen
    // All structs must be aligned to 16 bytes, I think.
    fprintf(dst, "typedef struct alignas(16) {\n");
    for (u32 j = 0; j < glsl_struct->member_list.num_members; j++) {
      const GLSLStructMember *member = &glsl_struct->member_list.members[j];
      fprintf(
          dst, "  alignas(%u) %s %.*s", glsl_type_to_alignment[member->type], glsl_type_to_c_type[member->type],
          member->identifier_length, member->identifier
      );
      if (member->array_length > 1) {
        fprintf(dst, "[%u];\n", member->array_length);
      } else {
        fprintf(dst, ";\n");
      }
    }

    u32 final_aligned_size = align_up(struct_size, max_alignment);
    u32 struct_alignment_size_difference = final_aligned_size - struct_size;
    if (struct_alignment_size_difference > 0) {
      fprintf(dst, "  unsigned char _padding[%u];\n", struct_alignment_size_difference);
    }

    fprintf(dst, "} %.*s;\n\n", glsl_struct->type_name_length, glsl_struct->type_name);
  }
}

static void print_c_string_with_newlines(FILE *destination, const char *s) {
  for (; *s; s++) {
    if (*s == '\n') {
      if (s[1] && s[1] == '\n') {
        fputs("\\n\"\n\"", destination);
      } else {
        fputs("\\n\"\n\"", destination);
      }
    } else {
      fputc(*s, destination);
    }
  }
}

static void print_bytes_array(FILE *destination, const SpirVBytesArray *bytes_array) {
  for (u32 i = 0; i < bytes_array->length; i += 4) {
    u32 word = (bytes_array->bytes[i + 0] << 0) | (bytes_array->bytes[i + 1] << 8) | (bytes_array->bytes[i + 2] << 16) |
               (bytes_array->bytes[i + 3] << 24);

    fprintf(destination, "0x%08X, ", word);
    u8 new_line = 8;
    if (((i / 4) + 1) % new_line == 0)
      fprintf(destination, "\n");
  }
  fprintf(destination, "\n");
}

static bool make_full_shader_name(char *buf, size_t buf_size, const char *name, const char *stage_suffix) {
  int len = snprintf(buf, buf_size, "%s_%s", name, stage_suffix);
  if (len < 0 || (size_t)len >= buf_size) {
    fprintf(stderr, "Shader name '%s_%s' exceeds buffer size %zu.\n", name, stage_suffix, buf_size);
    return false;
  }
  return true;
}

inline void codegen_shader_spec(
    FILE *dst,
    const ParsedShader *parsed_shader,
    const GLSLSource *opengl_glsl_source,
    const SpirVBytesArray *spirv_bytes_array
) {
  const char *shader_name = parsed_shader->name;
  const char *stage_suffix = shader_stage_to_string[parsed_shader->stage];

  char full_name[256];
  if (!make_full_shader_name(full_name, sizeof(full_name), shader_name, stage_suffix))
    return;

  // bytes
  fprintf(dst, "const uint32_t %s_spv[] = {\n", full_name);
  print_bytes_array(dst, spirv_bytes_array);
  fprintf(dst, "};\n\n");

  // opengl glsl
  fprintf(dst, "static const char* %s_opengl_glsl = \"", full_name);
  print_c_string_with_newlines(dst, opengl_glsl_source[GRAPHICS_BACKEND_OPENGL].string);
  fprintf(dst, "\";\n\n");

  // vulkan glsl
  fprintf(dst, "static const char* %s_vulkan_glsl = \"", full_name);
  print_c_string_with_newlines(dst, opengl_glsl_source[GRAPHICS_BACKEND_VULKAN].string);
  fprintf(dst, "\";\n\n");

  // struct definition
  fprintf(dst, "const ShaderSpec %s_shader_spec = {\n", full_name);
  fprintf(dst, "  .name = \"%s\",\n", full_name);
  fprintf(dst, "  .opengl_glsl = %s_opengl_glsl,\n", full_name);
  fprintf(dst, "  .spv = %s_spv,\n", full_name);
  fprintf(dst, "  .spv_size = sizeof(%s_spv),\n", full_name);

  const char *vertex_layout_name = parsed_shader->vertex_layout->name;
  if (parsed_shader->stage != SHADER_STAGE_VERTEX) {
    vertex_layout_name = vertex_layout_null_string;
  }
  fprintf(dst, "  .vertex_layout_id = %s,\n", vertex_layout_name);

  // fprintf(dst, "  .stage = %s,\n", shader_stage_to_enum_string[parsed_shader->stage]);
  fprintf(dst, "};\n\n");
}

static void codegen_footer(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "const uint32_t num_generated_specs = %u;\n", ir->num_parsed_shaders);

  fprintf(dst, "static const ShaderSpec* generated_shader_specs[] = {\n");
  for (u32 i = 0; i < ir->num_parsed_shaders; i++) {
    const ParsedShader *shader = &ir->parsed_shaders[i];
    const char *shader_name = shader->name;
    const char *stage_suffix = shader_stage_to_string[shader->stage];

    char full_name[256];
    if (!make_full_shader_name(full_name, sizeof(full_name), shader_name, stage_suffix))
      return;

    fprintf(dst, "  &%s_shader_spec,\n", full_name);
  }
  fprintf(dst, "};\n\n");

  fprintf(dst, "extern VkShaderModule shader_modules[NUM_SHADER_HANDLES];\n");
}

static void codegen_buffer_label_enum(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "typedef enum {\n");
  for (u32 i = 0; i < ir->num_descriptor_set_layouts; i++) {
    const DescriptorSetLayout *layout = &ir->descriptor_set_layouts[i];
    fprintf(dst, "  UNIFORM_BUFFER_LABEL_");
    print_name_in_caps_n(dst, layout->name, layout->name_length);
    fprintf(dst, ",\n");
  }
  fprintf(dst, "\n  NUM_UNIFORM_BUFFER_LABELS\n");
  fprintf(dst, "} UniformBufferLabel; \n\n");
}

static void codegen_shader_handle_enum(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "typedef enum {\n");
  for (u32 shaders_index = 0; shaders_index < ir->num_parsed_shaders; shaders_index++) {
    const ParsedShader *shader = &ir->parsed_shaders[shaders_index];
    const char *stage_suffix = shader_stage_to_enum_suffix[shader->stage];
    fprintf(dst, "  SHADER_HANDLE_");
    print_name_in_caps(dst, shader->name);
    fprintf(dst, "%s,\n", stage_suffix);
  }
  fprintf(dst, "\n  NUM_SHADER_HANDLES\n} ShaderHandle;\n\n");
}

static void codegen_descriptor_set_enum(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "typedef enum {\n");
  for (u32 i = 0; i < ir->num_descriptor_set_layouts; i++) {
    fprintf(dst, "  %s,\n", ir->descriptor_set_layouts[i].name);
  }
  fprintf(dst, "\n  NUM_DESCRIPTOR_SET_LAYOUTS\n} DescriptorSetLayoutIDs;\n\n");
}

// Only going to call this after validating that we have matching vert/frag pairs.
static void codegen_shader_program_enum(FILE *dst, const ShaderNameValid *names, u32 num_names) {
  fprintf(dst, "typedef enum {\n");

  for (u32 i = 0; i < num_names; i++) {
    fprintf(dst, "  SHADER_PROGRAM_");
    print_name_in_caps(dst, names[i].name);
    fprintf(dst, ",\n");
  }
  fprintf(dst, "\n  NUM_SHADER_PROGRAMS\n} ShaderProgram;\n\n");
}

static void codegen_vertex_layout_enum(FILE *dst, const ParsedShadersIR *ir) {
  fprintf(dst, "typedef enum {\n");
  for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
    fprintf(dst, "  %s,\n", ir->vertex_layouts[i].name);
  }
  fprintf(dst, "\n  NUM_VERTEX_LAYOUT_IDS\n} VertexLayoutID;\n\n");
}

static bool validate_vertex_fragment_pairs(const ParsedShadersIR *ir, ShaderNameValid *names, u32 *num_names) {
  u32 found_names = 0;

  bool valid = true;

  for (u32 j = 0; j < ir->num_parsed_shaders; j++) {
    const ParsedShader *shader = &ir->parsed_shaders[j];
    bool is_vert = (shader->stage == SHADER_STAGE_VERTEX);
    bool is_frag = (shader->stage == SHADER_STAGE_FRAGMENT);
    bool is_comp = (shader->stage == SHADER_STAGE_COMPUTE);

    bool found = false;
    for (u32 i = 0; i < found_names; i++) {
      if (strcmp(shader->name, names[i].name) != 0)
        continue;

      ShaderNameValid *entry = &names[i];

      if ((is_vert && entry->vert) || (is_frag && entry->frag) || (is_comp && entry->comp)) {
        fprintf(stderr, "Shader %s has a duplicate stage.\n", shader->name);
        valid = false;
      }
      if (is_comp && (entry->vert || entry->frag)) {
        fprintf(stderr, "Shader %s mixes compute with vertex/fragment stages.\n", shader->name);
        valid = false;
      }
      if ((is_vert || is_frag) && entry->comp) {
        fprintf(stderr, "Shader %s mixes vertex/fragment with compute stage.\n", shader->name);
        valid = false;
      }

      entry->vert |= is_vert;
      entry->frag |= is_frag;
      entry->comp |= is_comp;
      found = true;
      break;
    }

    if (!found) {
      ShaderNameValid *entry = &names[found_names++];
      entry->name = shader->name;
      entry->vert = is_vert;
      entry->frag = is_frag;
      entry->comp = is_comp;
    }
  }

  // Validation pass over found pairs.
  for (u32 i = 0; i < found_names; i++) {
    ShaderNameValid *entry = &names[i];
    if (entry->comp) {
      continue;
    }
    if (!entry->vert || !entry->frag) {
      fprintf(stderr, "Shader %s is missing a %s stage.\n", entry->name, !entry->vert ? "vertex" : "fragment");
      valid = false;
    }
  }

  *num_names = found_names;
  return valid;
}

// The real deal!
bool codegen(const char *output_filepath, const ParsedShadersIR *ir) {

  // Init
  GLSLSource glsl_sources[MAX_NUM_SHADERS][NUM_GRAPHICS_BACKENDS];
  memset(glsl_sources, 0, sizeof(glsl_sources));

  SpirVBytesArray spirv_bytes_arrays[MAX_NUM_SHADERS];
  memset(spirv_bytes_arrays, 0, sizeof(spirv_bytes_arrays));

  // First pass over shaders to check spirv compilation success and perform slice replacements
  bool should_codegen = true;
  for (u32 shaders_idx = 0; shaders_idx < ir->num_parsed_shaders; shaders_idx++) {
    GLSLSource *source = glsl_sources[shaders_idx];
    const ParsedShader *current_shader = &ir->parsed_shaders[shaders_idx];

    // Replace string slices
    for (u32 backend_idx = 0; backend_idx < NUM_GRAPHICS_BACKENDS; backend_idx++) {
      source[backend_idx] = replace_string_slices(current_shader, (GraphicsBackend)backend_idx);
    }

    // Compile GLSL to spirv
    spirv_bytes_arrays[shaders_idx] =
        compile_vulkan_source_to_spirv(source[GRAPHICS_BACKEND_VULKAN], current_shader->stage);
    if (spirv_bytes_arrays[shaders_idx].bytes == NULL) {
      printf("Compilation for %s failed.\n", current_shader->name);
      should_codegen = false;
    }
  }

  ShaderNameValid program_names[MAX_NUM_SHADERS];
  memset(program_names, 0, sizeof(program_names));
  u32 num_names;
  bool pairs_valid = validate_vertex_fragment_pairs(ir, program_names, &num_names);
  if (!pairs_valid) {
    should_codegen = false;
  }

  if (!should_codegen) {
    fprintf(stderr, "Shader compilation failed. Not writing %s. Stopping.\n", output_filepath);
    return false;
  }

  // Codegen
  // TODO file walking in main is incorrect. gen/shaders sees all buffers.
  FILE *dst = fopen(output_filepath, "w");

  codegen_compiled_shader_header(dst);
  codegen_descriptor_set_enum(dst, ir);
  codegen_shader_handle_enum(dst, ir); // TODO deprecate
  codegen_shader_program_enum(dst, program_names, num_names);
  codegen_vertex_layout_enum(dst, ir);
  codegen_buffer_label_enum(dst, ir);

  generate_vulkan_vertex_layout_array(dst, ir);
  generate_opengl_vertex_layout_array(dst, ir);

  generate_vulkan_descriptor_set_binding_lists(dst, ir);
  generate_vulkan_descriptor_write_templates(dst, ir);
  generate_vulkan_descriptor_pool_size_array(dst, ir);

  codegen_struct_defintions(dst, ir->glsl_structs, ir->num_glsl_structs);
  codegen_shader_spec_struct_definition(dst);
  for (u32 i = 0; i < ir->num_parsed_shaders; i++) {
    codegen_shader_spec(dst, &ir->parsed_shaders[i], glsl_sources[i], &spirv_bytes_arrays[i]);
  }

  codegen_footer(dst, ir);
  return true;
}
