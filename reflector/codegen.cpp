#include "codegen.h"
#include "filesystem_utils.h"
#include "parser.h"
#include "reflection_data.h"
#include "subprocess.h"

#include <assert.h>
#include <cstdio>

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
TemplateStringReplacement directive_replacement_set_binding(GraphicsBackend backend) {
  TemplateStringReplacement replacement;

  switch (backend) {
  case GRAPHICS_BACKEND_OPENGL:
    replacement.string = "layout(binding = _) ";
    replacement.length = strlen(replacement.string);
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
    return directive_replacement_set_binding(backend);
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

      if (backend == GRAPHICS_BACKEND_OPENGL) {
        // layout(binding = _) _ is 17
        current_start[17] = '0' + string_slice->binding;
      } else if (backend == GRAPHICS_BACKEND_VULKAN) {
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
  fprintf(destination, "#include \"vulkan_base.h\"\n");
  fprintf(destination, "#include <stddef.h>\n");
  fprintf(destination, "#include <stdint.h>\n\n");
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

// the real deal
void codegen(FILE *destination, const ParsedShadersIR *parsed_shaders_ir) {

  // init
  GLSLSource glsl_sources[MAX_NUM_SHADERS][NUM_GRAPHICS_BACKENDS];
  memset(glsl_sources, 0, sizeof(glsl_sources));
  SpirVBytesArray spirv_bytes_arrays[MAX_NUM_SHADERS];
  memset(spirv_bytes_arrays, 0, sizeof(spirv_bytes_arrays));
  u32 num_shaders = parsed_shaders_ir->num_parsed_shaders;

  codegen_compiled_shader_header(destination);

  // replace string slices, compile vulkan glsl to spirv, and emit the enum with shader IDs
  fprintf(destination, "enum ShaderHandle {\n");

  for (u32 shaders_index = 0; shaders_index < num_shaders; shaders_index++) {
    const ParsedShader *current_parsed_shader = &parsed_shaders_ir->parsed_shaders[shaders_index];
    // printf("replacing and compiling %s\n\n", current_sliced_shader->name);

    // compile all backends
    for (u32 backend_index = 0; backend_index < NUM_GRAPHICS_BACKENDS; backend_index++) {
      glsl_sources[shaders_index][backend_index] =
          replace_string_slices(current_parsed_shader, (GraphicsBackend)backend_index);
      // printf("%s\n\n", glsl_sources[shaders_index][backend_index].string);
    }

    spirv_bytes_arrays[shaders_index] = compile_vulkan_source_to_glsl(
        glsl_sources[shaders_index][GRAPHICS_BACKEND_VULKAN], current_parsed_shader->stage);
    if (spirv_bytes_arrays[shaders_index].bytes == NULL) {
      printf("Compilation for %s failed.\n", current_parsed_shader->name);
    }

    fputs("\tSHADER_HANDLE_", destination);
    print_name_in_caps(destination, current_parsed_shader->name);
    fputs(",\n", destination);
  }
  fprintf(destination, "\n\tNUM_SHADER_HANDLES\n};\n\n");

  // codegen for vertex layouts
  // enum
  fprintf(destination, "enum VertexLayoutID{\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];
    fprintf(destination, "\t%s,\n", vertex_layout->name);
  }
  fprintf(destination, "\n\tNUM_VERTEX_LAYOUT_IDS\n};\n\n");

  // vulkan vertex layout array
  fprintf(destination, "const VulkanVertexLayout generated_vulkan_vertex_layouts[NUM_VERTEX_LAYOUT_IDS] = {\n");
  for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
    const VertexLayout *vertex_layout = &parsed_shaders_ir->vertex_layouts[i];
    fprintf(destination, "\t[%s] = {\n", vertex_layout->name);

    // bindings
    fprintf(destination, "\t\t.binding_count = %u,\n", vertex_layout->binding_count);
    fprintf(destination, "\t\t.bindings = {\n");
    for (u32 i = 0; i < MAX_NUM_VERTEX_BINDINGS; i++) {
      bool has_stride = (vertex_layout->binding_strides[i] > 0);
      bool has_rate = (vertex_layout->binding_rates[i] != VERTEX_ATTRIBUTE_RATE_NULL);
      if (has_stride != has_rate) {
        fprintf(stderr, "Generating binding code for vertex layout %s, but rate and stride are not both present.\n",
                vertex_layout->name);
        continue;
      }

      if (!has_rate) {
        continue;
      }

      fprintf(destination, "\t\t\t{ .binding = %u,\n", i);
      fprintf(destination, "\t\t\t.stride = %u,\n", vertex_layout->binding_strides[i]);
      fprintf(destination, "\t\t\t.inputRate = %s },\n",
              vertex_attribute_rate_to_vulkan_enum_string(vertex_layout->binding_rates[i]));
    }
    fprintf(destination, "\t\t},\n"); // close array of bindings

    // attributes
    fprintf(destination, "\t\t.attribute_count = %u,\n", vertex_layout->attribute_count);
    fprintf(destination, "\t\t.attributes = {\n");
    for (u32 i = 0; i < vertex_layout->attribute_count; i++) {
      fprintf(destination, "\t\t\t{ .binding = %u,\n", vertex_layout->attributes[i].binding);
      fprintf(destination, "\t\t\t.location = %u,\n", vertex_layout->attributes[i].location);
      fprintf(destination, "\t\t\t.offset = %u,\n", vertex_layout->attributes[i].offset);
      fprintf(destination, "\t\t\t.format = %s },\n",
              glsl_type_to_vulkan_format(vertex_layout->attributes[i].glsl_type));
    }
    fprintf(destination, "\t\t}\n"); // close attributes
    fprintf(destination, "\t},\n");  // close this vertex layout
  }
  fprintf(destination, "};\n\n"); // close entire vulkan vertex layout array init

  // for individual shaders, the spirv bytes, the opengl glsl
}
