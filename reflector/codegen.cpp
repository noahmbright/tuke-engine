#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"
#include "reflection_data.h"
#include "reflector.h"
#include "subprocess.h"

#include <assert.h>

static const u32 max_num_vertex_attributes = 4;
static const u32 max_num_vertex_bindings = 4;

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

// {{ LOCATION 0 BINDING 0 RATE_VERTEX OFFSET TIGHTLY_PACKED }}
// turns into
// layout(location = 0) for both opengl and vulkan
TemplateStringReplacement directive_replacement_location(GraphicsBackend backend) {
  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
  case GRAPHICS_BACKEND_VULKAN: {
    TemplateStringReplacement replacement;
    replacement.string = "layout(location = __) ";
    replacement.length = strlen(replacement.string);
    return replacement;
  }

  default:
    assert(false);
  }
}

// only supporting set and binding numbers < 10
// {{ SET_BINDING 0 0 BUFFER_LABEL GLOBAL }}
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
}

TemplateStringReplacement perform_replacement(const TemplateStringSlice *string_slice, GraphicsBackend backend) {

  TemplateStringReplacement replacement;
  replacement.string = NULL;
  replacement.length = 0;

  switch (string_slice->type) {
  case DIRECTIVE_TYPE_VERSION:
    return directive_replacement_version(backend);
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

  GLSLSource glsl_source;
  glsl_source.length = compiled_source_length;
  glsl_source.string = compiled_source;
  return glsl_source;
}

inline void codegen_compiled_shader_header(FILE *destination) {
  fprintf(destination, "// Generated shader header, do not edit\n");
  fprintf(destination, "#pragma once\n");
  fprintf(destination, "#include \"glad/gl.h\"\n");
  fprintf(destination, "#include <assert.h>\n");
  fprintf(destination, "#include <stddef.h>\n");
  fprintf(destination, "#include <stdint.h>\n");
  fprintf(destination, "#include <stdio.h>\n");
  fprintf(destination, "#include <vulkan/vulkan.h>\n");
  fprintf(destination, "#include <glm/glm.hpp>\n");

  fprintf(destination, "enum ShaderStage {\n");
  fprintf(destination, "  SHADER_STAGE_VERTEX,\n");
  fprintf(destination, "  SHADER_STAGE_FRAGMENT,\n");
  fprintf(destination, "  SHADER_STAGE_COMPUTE,\n");
  fprintf(destination, "};\n\n");
}

inline char ascii_to_upper(char c) {
  // lowercase letters are uppercase + 32, difference is in 5th bit
  // to unset the 5th bit, & with 1111....01111
  // 0s will stay 0s, 1s will stay 1s, except for the 5th bit
  u8 is_lower = (u8)(c - 'a') < 26;
  return c & ~(is_lower << 5);
}

inline void print_name_in_caps(FILE *destination, const char *name) {
  for (const char *s = name; *s; s++) {
    fputc(ascii_to_upper(*s), destination);
  }
}

const char *glsl_type_to_vulkan_format(GLSLType type) {
  switch (type) {

  case GLSL_TYPE_VEC2:
    return "VK_FORMAT_R32G32_SFLOAT";
  case GLSL_TYPE_VEC3:
    return "VK_FORMAT_R32G32B32_SFLOAT";

    // TODO?
  case GLSL_TYPE_FLOAT:
  case GLSL_TYPE_UINT:
  case GLSL_TYPE_VEC4:
  case GLSL_TYPE_MAT2:
  case GLSL_TYPE_MAT3:
  case GLSL_TYPE_MAT4:

  case GLSL_TYPE_NULL:
  default:
    fprintf(stderr, "vertex_attribute_rate_to_vulkan_enum_string got an invalid rate enum.\n");
    return NULL;
  }
}

void generate_vulkan_vertex_layout_array(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {
  // static arrays, needed bc initializing VkPipelineVertexInputStateCreateInfo as const requires pointing arrays
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];
    if (vertex_layout->attribute_count == 0 && vertex_layout->binding_count == 0) {
      continue;
    }

    // bindings
    fprintf(destination, "const VkVertexInputBindingDescription vertex_bindings_%s[] = {\n", vertex_layout->name);
    for (u32 i = 0; i < MAX_NUM_VERTEX_BINDINGS; i++) {
      bool has_stride = (vertex_layout->binding_strides[i] > 0);
      bool has_rate = (vertex_layout->binding_rates[i] != VERTEX_ATTRIBUTE_RATE_NULL);
      if (has_stride != has_rate) {
        fprintf(
            stderr,
            "Generating binding code for vertex layout %s, but rate and stride are not both present for binding %u.\n",
            vertex_layout->name, i);
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

      fprintf(destination, "{ .binding = %u, ", i);
      fprintf(destination, ".stride = %u, ", vertex_layout->binding_strides[i]);
      fprintf(destination, ".inputRate = %s },\n",
              vertex_attribute_rate_to_vulkan_enum_string(vertex_layout->binding_rates[i]));
    }
    fprintf(destination, "};\n\n"); // close array of bindings

    // attributes
    fprintf(destination, "const VkVertexInputAttributeDescription vertex_attributes_%s[] = {\n", vertex_layout->name);
    for (u32 i = 0; i < vertex_layout->attribute_count; i++) {
      fprintf(destination, "  { .binding = %u, ", vertex_layout->attributes[i].binding);
      fprintf(destination, ".location = %u, ", vertex_layout->attributes[i].location);
      fprintf(destination, ".offset = %u, ", vertex_layout->attributes[i].offset);
      fprintf(destination, ".format = %s },\n", glsl_type_to_vulkan_format(vertex_layout->attributes[i].glsl_type));
    }
    fprintf(destination, "};\n\n"); // close attributes
  }

  // array init
  fprintf(destination,
          "const VkPipelineVertexInputStateCreateInfo generated_vulkan_vertex_layouts[NUM_VERTEX_LAYOUT_IDS] = {\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];

    if (vertex_layout->attribute_count > max_num_vertex_attributes) {
      fprintf(stderr, "Vertex Layout %s has %u attributes, exceeding the max of %u.\n", vertex_layout->name,
              vertex_layout->attribute_count, max_num_vertex_attributes);
      continue;
    }
    if (vertex_layout->binding_count > max_num_vertex_bindings) {
      fprintf(stderr, "Vertex Layout %s has %u bindings, exceeding the max of %u.\n", vertex_layout->name,
              vertex_layout->binding_count, max_num_vertex_bindings);
      continue;
    }

    fprintf(destination, "  [%s] = {\n", vertex_layout->name);
    // boilerplate
    fprintf(destination, "    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,\n");
    fprintf(destination, "    .pNext = NULL,\n");
    fprintf(destination, "    .flags = 0,\n");

    // bindings
    fprintf(destination, "    .vertexBindingDescriptionCount  = %u,\n", vertex_layout->binding_count);
    if (vertex_layout->binding_count != 0) {
      fprintf(destination, "    .pVertexBindingDescriptions  = vertex_bindings_%s,\n", vertex_layout->name);
    } else {
      fprintf(destination, "    .pVertexBindingDescriptions  = NULL,\n");
    }

    // attributes
    fprintf(destination, "    .vertexAttributeDescriptionCount  = %u,\n", vertex_layout->attribute_count);
    if (vertex_layout->attribute_count != 0) {
      fprintf(destination, "    .pVertexAttributeDescriptions  = vertex_attributes_%s,\n", vertex_layout->name);
    } else {
      fprintf(destination, "    .pVertexAttributeDescriptions  = NULL,\n");
    }

    fprintf(destination, "  },\n"); // close this vertex layout
  }
  fprintf(destination, "};\n\n"); // close entire vulkan vertex layout array init
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
    fprintf(stderr, "vertex_attribute_rate_to_vulkan_enum_string got an invalid rate enum.\n");
    return 0;
  }
}

void generate_opengl_vertex_layout_array(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {
  const char *parameter_list = "GLuint vao, GLuint* vbos, uint32_t num_vbos, GLuint ebo";

  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];
    if (vertex_layout->binding_count == 0 && vertex_layout->attribute_count == 0) {
      continue;
    }

    // emit init function definition
    fprintf(destination, "inline void init_vertex_layout%s(%s){\n", vertex_layout->name, parameter_list);
    fprintf(destination,
            "  if(num_vbos != %u){\n    fprintf(stderr, \"opengl %s vertex layout got wrong number of bindings\");\n"
            "  return;\n  }\n\n",
            vertex_layout->binding_count, vertex_layout->name);

    fprintf(destination, "  glBindVertexArray(vao);\n");
    fprintf(destination, "  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);\n\n");

    // emit attributes
    u32 current_binding = -1; // impossible sentinel value, hopefully
    for (u32 i = 0; i < vertex_layout->attribute_count; i++) {
      const VertexAttribute *attribute = &vertex_layout->attributes[i];
      u32 binding_stride = vertex_layout->binding_strides[attribute->binding];

      // only supporting GL_FLOAT for now
      if (attribute->binding != current_binding) {
        fprintf(destination, "  glBindBuffer(GL_ARRAY_BUFFER, vbos[%u]);\n", attribute->binding);
        current_binding = attribute->binding;
      }
      fprintf(destination, "  glEnableVertexAttribArray(%u);\n", attribute->location);
      fprintf(destination, "  glVertexAttribPointer(%u, %u, GL_FLOAT, GL_FALSE, %u, (void*)%u);\n", attribute->location,
              glsl_type_to_number_of_floats(attribute->glsl_type), binding_stride, attribute->offset);

      if (attribute->rate == VERTEX_ATTRIBUTE_RATE_INSTANCE) {
        fprintf(destination, "  glVertexAttribDivisor(%u, 1);\n", attribute->location);
      } else {
        fprintf(destination, "\n");
      }
    }

    fprintf(destination, "\n  glBindVertexArray(0);\n");
    fprintf(destination, "  glBindBuffer(GL_ARRAY_BUFFER, 0);\n");
    fprintf(destination, "  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);\n");
    fprintf(destination, "}\n\n");
  }

  // populate function pointer array
  fprintf(destination, "typedef void (*VertexLayoutInitFn)(%s);\n", parameter_list);
  fprintf(destination,
          "const VertexLayoutInitFn generated_opengl_vertex_array_initializers[NUM_VERTEX_LAYOUT_IDS] = {\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];
    if (vertex_layout->attribute_count == 0 && vertex_layout->binding_count == 0) {
      fprintf(destination, "  [%s] = NULL,\n", vertex_layout->name);
    } else {
      fprintf(destination, "  [%s] = init_vertex_layout%s,\n", vertex_layout->name, vertex_layout->name);
    }
  }
  fprintf(destination, "};\n\n");
}

void generate_vulkan_descriptor_pool_size_array(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {
  u32 max_sets = 0;
  for (u32 i = 0; i < NUM_DESCRIPTOR_TYPES; i++) {
    u32 num_sets = parsed_shaders_ir->descriptor_binding_types[i];
    max_sets = (max_sets > num_sets) ? max_sets : num_sets;
  }

  fprintf(destination, "const uint32_t pool_size_count = %u;\n", NUM_DESCRIPTOR_TYPES);
  fprintf(destination, "const uint32_t max_descriptor_sets = %u;\n", max_sets);
  fprintf(destination, "const VkDescriptorPoolSize generated_pool_sizes[%u] = {\n", NUM_DESCRIPTOR_TYPES);

  fprintf(destination, "  { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = %u },\n",
          parsed_shaders_ir->descriptor_binding_types[DESCRIPTOR_TYPE_UNIFORM]);

  fprintf(destination, "  { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = %u },\n",
          parsed_shaders_ir->descriptor_binding_types[DESCRIPTOR_TYPE_SAMPLER2D]);
  fprintf(destination, "};\n\n");
}

void generate_vulkan_descriptor_set_layout_binding_lists(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {
  (void)destination;

  for (u32 i = 0; i < parsed_shaders_ir->num_descriptor_set_layouts; i++) {
    const DescriptorSetLayout *layout = &parsed_shaders_ir->descriptor_set_layouts[i];
    for (u32 j = 0; j < layout->num_bindings; j++) {
    }
  }
}

inline void codegen_shader_spec_definition(FILE *destination) {
  fprintf(destination, "struct ShaderSpec {\n");
  fprintf(destination, "  const char* name;\n");
  fprintf(destination, "  const char* opengl_glsl;\n");
  fprintf(destination, "  const uint32_t* spv;\n");
  fprintf(destination, "  const uint32_t spv_size;\n");
  fprintf(destination, "  VertexLayoutID vertex_layout_id;\n");
  fprintf(destination, "  ShaderStage stage;\n");
  fprintf(destination, "};\n\n");
}

static u32 align_up(u32 offset, u32 alignment) { return (offset + alignment - 1) & ~(alignment - 1); }

static void codegen_struct_defintions(FILE *destination, const GLSLStruct *glsl_structs, u32 num_glsl_structs) {
  for (u32 i = 0; i < num_glsl_structs; i++) {
    const GLSLStruct *glsl_struct = &glsl_structs[i];

    // precompute stuff
    u32 max_alignment = 0;
    u32 struct_size = 0;
    for (u32 j = 0; j < glsl_struct->member_list.num_members; j++) {
      const GLSLStructMember *member = &glsl_struct->member_list.members[j];
      if (member->array_length > 1) {
        fprintf(destination, "const uint32_t %.*s_%.*s_array_size = %u;\n", glsl_struct->type_name_length,
                glsl_struct->type_name, member->identifier_length, member->identifier, member->array_length);
      }

      u32 alignment = glsl_type_to_alignment[member->type];
      max_alignment = (max_alignment > alignment) ? max_alignment : alignment;
      u32 next_aligned_size = align_up(struct_size, alignment);
      struct_size = next_aligned_size;
      struct_size += glsl_type_to_size[member->type];
    }

    // codegen
    fprintf(destination, "typedef struct alignas(%u) {\n", max_alignment);
    for (u32 j = 0; j < glsl_struct->member_list.num_members; j++) {
      const GLSLStructMember *member = &glsl_struct->member_list.members[j];
      fprintf(destination, "  alignas(%u) %s %.*s", glsl_type_to_alignment[member->type],
              glsl_type_to_c_type[member->type], member->identifier_length, member->identifier);
      if (member->array_length > 1) {
        fprintf(destination, "[%u];\n", member->array_length);
      } else {
        fprintf(destination, ";\n");
      }
    }

    u32 final_aligned_size = align_up(struct_size, max_alignment);
    u32 struct_alignment_size_difference = final_aligned_size - struct_size;
    if (struct_alignment_size_difference > 0) {
      fprintf(destination, "  unsigned char _padding[%u];\n", struct_alignment_size_difference);
    }

    fprintf(destination, "} %.*s;\n\n", glsl_struct->type_name_length, glsl_struct->type_name);
  }
}

inline void codegen_shader_spec(FILE *destination, const ParsedShader *parsed_shader,
                                const GLSLSource *opengl_glsl_source, const SpirVBytesArray *spirv_bytes_array) {
  // bytes
  fprintf(destination, "const uint32_t %s_spv[] = {\n", parsed_shader->name);
  print_bytes_array(destination, spirv_bytes_array);
  fprintf(destination, "};\n\n");

  // opengl glsl
  fprintf(destination, "static const char* %s_opengl_glsl = \"", parsed_shader->name);
  print_c_string_with_newlines(destination, opengl_glsl_source[GRAPHICS_BACKEND_OPENGL].string);
  fprintf(destination, "\";\n\n");

  // vulkan glsl
  fprintf(destination, "static const char* %s_vulkan_glsl = \"", parsed_shader->name);
  print_c_string_with_newlines(destination, opengl_glsl_source[GRAPHICS_BACKEND_VULKAN].string);
  fprintf(destination, "\";\n\n");

  // struct definition
  fprintf(destination, "const ShaderSpec %s_shader_spec = {\n", parsed_shader->name);
  fprintf(destination, "  .name = \"%s\",\n", parsed_shader->name);
  fprintf(destination, "  .opengl_glsl = %s_opengl_glsl,\n", parsed_shader->name);
  fprintf(destination, "  .spv = %s_spv,\n", parsed_shader->name);
  fprintf(destination, "  .spv_size = sizeof(%s_spv),\n", parsed_shader->name);

  const char *vertex_layout_name = parsed_shader->vertex_layout->name;
  if (parsed_shader->stage != SHADER_STAGE_VERTEX) {
    vertex_layout_name = vertex_layout_null_string;
  }
  fprintf(destination, "  .vertex_layout_id = %s,\n", vertex_layout_name);

  fprintf(destination, "  .stage = %s,\n", shader_stage_to_enum_string[parsed_shader->stage]);
  fprintf(destination, "};\n\n");
}

void codegen_vulkan_shader_module_creation(FILE *destination) {
  fprintf(destination, "inline void init_generated_shader_vk_modules(VkDevice device){\n");
  fprintf(destination, "  for(uint32_t i = 0; i < NUM_SHADER_HANDLES; i++) {\n");
  fprintf(destination, "    VkShaderModuleCreateInfo shader_module_create_info;\n");
  fprintf(destination, "    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;\n");
  fprintf(destination, "    shader_module_create_info.pNext = NULL;\n");
  fprintf(destination, "    shader_module_create_info.flags = 0;\n");
  fprintf(destination, "    shader_module_create_info.codeSize = generated_shader_specs[i]->spv_size;\n");
  fprintf(destination, "    shader_module_create_info.pCode = generated_shader_specs[i]->spv;\n\n");
  fprintf(
      destination,
      "    VkResult result = vkCreateShaderModule(device, &shader_module_create_info, NULL, &shader_modules[i]);\n");
  fprintf(destination, "    if (result != VK_SUCCESS) {\n");
  fprintf(destination, "      fprintf(stderr, \"Creating shader module for generated shader %%s failed.\\n\", "
                       "generated_shader_specs[i]->name);\n");
  fprintf(destination, "    }\n");
  fprintf(destination, "  }\n");
  fprintf(destination, "}\n\n");
}

void codegen_footer(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {
  fprintf(destination, "const uint32_t num_generated_specs = %u;\n", parsed_shaders_ir->num_parsed_shaders);
  fprintf(destination, "static const ShaderSpec* generated_shader_specs[] = {\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_parsed_shaders; i++) {
    fprintf(destination, "  &%s_shader_spec,\n", parsed_shaders_ir->parsed_shaders[i].name);
  }
  fprintf(destination, "};\n\n");

  fprintf(destination, "extern VkShaderModule shader_modules[NUM_SHADER_HANDLES];\n");
  // codegen_vulkan_shader_module_creation(destination);
}

// the real deal
void codegen(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {

  // init
  GLSLSource glsl_sources[MAX_NUM_SHADERS][NUM_GRAPHICS_BACKENDS];
  memset(glsl_sources, 0, sizeof(glsl_sources));
  SpirVBytesArray spirv_bytes_arrays[MAX_NUM_SHADERS];
  memset(spirv_bytes_arrays, 0, sizeof(spirv_bytes_arrays));

  codegen_compiled_shader_header(destination);

  // replace string slices, compile vulkan glsl to spirv, and emit the enum with shader IDs
  fprintf(destination, "enum ShaderHandle {\n");

  for (u32 shaders_index = 0; shaders_index < parsed_shaders_ir->num_parsed_shaders; shaders_index++) {
    const ParsedShader *current_parsed_shader = &parsed_shaders_ir->parsed_shaders[shaders_index];
    // compile all backends
    for (u32 backend_index = 0; backend_index < NUM_GRAPHICS_BACKENDS; backend_index++) {
      glsl_sources[shaders_index][backend_index] =
          replace_string_slices(current_parsed_shader, (GraphicsBackend)backend_index);
    }

    spirv_bytes_arrays[shaders_index] = compile_vulkan_source_to_glsl(
        glsl_sources[shaders_index][GRAPHICS_BACKEND_VULKAN], current_parsed_shader->stage);
    if (spirv_bytes_arrays[shaders_index].bytes == NULL) {
      printf("Compilation for %s failed.\n", current_parsed_shader->name);
    }

    fputs("  SHADER_HANDLE_", destination);
    print_name_in_caps(destination, current_parsed_shader->name);
    fputs(",\n", destination);
  }
  fprintf(destination, "\n  NUM_SHADER_HANDLES\n};\n\n");

  // codegen for vertex layouts
  // struct definition
  fprintf(destination, "#define MAX_NUM_VERTEX_BINDINGS %u\n", max_num_vertex_bindings);
  fprintf(destination, "#define MAX_NUM_VERTEX_ATTRIBUTES %u\n", max_num_vertex_attributes);
  // enum
  fprintf(destination, "enum VertexLayoutID{\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    fprintf(destination, "  %s,\n", parsed_shaders_ir->vertex_layouts[i].name);
  }
  fprintf(destination, "\n  NUM_VERTEX_LAYOUT_IDS\n};\n\n");

  fprintf(destination, "//////////////////// VULKAN VERTEX LAYOUTS ///////////////\n");
  generate_vulkan_vertex_layout_array(destination, parsed_shaders_ir);
  fprintf(destination, "//////////////////// OPENGL VERTEX LAYOUTS ///////////////\n");
  generate_opengl_vertex_layout_array(destination, parsed_shaders_ir);

  // descriptor sets
  // enum
  fprintf(destination, "enum DescriptorSetLayoutIDs{\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_descriptor_set_layouts; i++) {
    fprintf(destination, "  %s,\n", parsed_shaders_ir->descriptor_set_layouts[i].name);
  }
  fprintf(destination, "\n  NUM_DESCRIPTOR_SET_LAYOUTS\n};\n\n");
  generate_vulkan_descriptor_pool_size_array(destination, parsed_shaders_ir);
  generate_vulkan_descriptor_set_layout_binding_lists(destination, parsed_shaders_ir);
  codegen_struct_defintions(destination, parsed_shaders_ir->glsl_structs, parsed_shaders_ir->num_glsl_structs);

  // for individual shaders, the spirv bytes, the opengl glsl
  codegen_shader_spec_definition(destination);
  for (u32 i = 0; i < parsed_shaders_ir->num_parsed_shaders; i++) {
    codegen_shader_spec(destination, &parsed_shaders_ir->parsed_shaders[i], glsl_sources[i], &spirv_bytes_arrays[i]);
  }

  codegen_footer(destination, parsed_shaders_ir);
}
