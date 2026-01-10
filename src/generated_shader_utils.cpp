#include "generated_shader_utils.h"

#define BUFFER_SIZE 512

VkShaderModule shader_modules[NUM_SHADER_HANDLES] = {0};

VkPipeline shader_handles_to_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                               ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle,
                                               VkPipelineLayout pipeline_layout) {

  VkShaderModule vertex_shader = shader_modules[vertex_shader_handle];
  VkShaderModule fragment_shader = shader_modules[fragment_shader_handle];
  assert(vertex_shader != VK_NULL_HANDLE);
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state =
      &generated_vulkan_vertex_layouts[generated_shader_specs[vertex_shader_handle]->vertex_layout_id];

  return create_default_graphics_pipeline(context, render_pass, vertex_shader, fragment_shader, vertex_input_state,
                                          pipeline_layout);
}

u32 link_shader_program(const char *vertex_shader_source, const char *fragment_shader_source) {
  int success;
  char info_log[BUFFER_SIZE];
  unsigned vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  unsigned fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to compile vertex shader with info:\n%s", info_log);
  }

  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to compile fragment shader with info:\n%s", info_log);
  }

  unsigned shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, BUFFER_SIZE, NULL, info_log);
    fprintf(stderr, "Failed to link shader program with info:\n%s", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return shader_program;
}

#undef BUFFER_SIZE
