#include "codegen.h"
#include "parser.h"

#include <assert.h>
#include <cstring>

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

TemplateStringReplacement perform_replacement(TemplateStringSlice *string_slice, GraphicsBackend backend) {

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

void asdf() {
  for (u32 backend_index = 0; backend_index < NUM_GRAPHICS_BACKENDS; backend_index++) {
  }
}

GLSLSource replace_string_slices(TemplateStringSlice *string_slices, u32 num_string_slices, GraphicsBackend backend) {
  u32 compiled_source_length = 0;

  // first pass, get replacements and accumulate lengths
  for (u32 i = 0; i < num_string_slices; i++) {
    TemplateStringSlice *string_slice = &string_slices[i];
    string_slice->replacements[backend] = perform_replacement(string_slice, backend);
    compiled_source_length += string_slice->replacements[backend].length;
  }

  u32 source_index = 0;
  char *compiled_source = (char *)malloc(sizeof(char) * (compiled_source_length + 1));

  for (u32 i = 0; i < num_string_slices; i++) {
    const TemplateStringSlice *string_slice = &string_slices[i];
    const TemplateStringReplacement *replacement = &string_slice->replacements[backend];

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
