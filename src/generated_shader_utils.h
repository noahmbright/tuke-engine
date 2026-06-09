#pragma once

// TODO would like to get this include out, and include a source of typedefs from elsewhere.
// This header is the general one. I will have per app defs and this will break down.
#include "shaders.h"

// TODO need to include vulkan base after generated headers because opengl cruft
//      Do I even want to include both of these every time? Where can I be smart and
//      pick only what I'm using?
#include "opengl_base.h"
#include "vulkan/vulkan_base.h"
#include <vulkan/vulkan_core.h>

inline VkPipeline shader_handles_to_graphics_pipeline(
    const VulkanContext *ctx,
    VkRenderPass render_pass,
    ShaderHandle vert_handle,
    ShaderHandle frag_handle,
    VkPipelineLayout pipeline_layout
) {
  const ShaderSpec *vert_spec = generated_shader_specs[vert_handle];
  VkShaderModule vertex_shader = create_shader_module(ctx->device, vert_spec->spv, vert_spec->spv_size);
  assert(vertex_shader != VK_NULL_HANDLE);

  const ShaderSpec *frag_spec = generated_shader_specs[frag_handle];
  VkShaderModule frag_shader = create_shader_module(ctx->device, frag_spec->spv, frag_spec->spv_size);
  assert(frag_shader != VK_NULL_HANDLE);

  const VkPipelineVertexInputStateCreateInfo *vertex_input_state =
      &generated_vulkan_vertex_layouts[vert_spec->vertex_layout_id];

  VkPipeline pipeline = create_default_graphics_pipeline(
      ctx, render_pass, vertex_shader, frag_shader, vertex_input_state, pipeline_layout
  );

  vkDestroyShaderModule(ctx->device, vertex_shader, NULL);
  vkDestroyShaderModule(ctx->device, frag_shader, NULL);

  return pipeline;
}

// TODO would like this render pass removed
inline void
init_program_spec(VulkanContext *ctx, VkRenderPass render_pass, const ProgramSpec *spec, VulkanMaterial *mat) {
  VkDevice dev = ctx->device;

  VkShaderModule vert_mod = create_shader_module(dev, spec->vert_spv, spec->vert_spv_size);
  assert(vert_mod != VK_NULL_HANDLE);

  VkShaderModule frag_mod = create_shader_module(dev, spec->frag_spv, spec->frag_spv_size);
  assert(frag_mod != VK_NULL_HANDLE);

  assert(spec->num_descriptor_set_layouts <= 16);
  VkDescriptorSetLayout temp_layouts[16] = {};

  for (u32 i = 0; i < spec->num_descriptor_set_layouts; i++) {
    DescriptorSetLayoutID layout_id = spec->binding_list_ids[i];
    VkDescriptorSetLayout *layout = &ctx->descriptor_set_layouts[layout_id];

    if (*layout == VK_NULL_HANDLE) {
      *layout = create_descriptor_set_layout(dev, spec->binding_lists[i], spec->binding_list_lens[i]);
    }
    temp_layouts[i] = *layout;

    VkDescriptorSet descriptor_set = create_descriptor_set(ctx->device, layout, ctx->descriptor_pool);
    mat->descriptor_sets[i] = descriptor_set;
    mat->descriptor_set_writes[i] = spec->write_templates[i];
    mat->descriptor_set_write_lens[i] = spec->binding_list_lens[i];
  }
  mat->num_descriptor_sets = spec->num_descriptor_set_layouts;

  const VkPipelineVertexInputStateCreateInfo *vertex_layout = &generated_vulkan_vertex_layouts[spec->vertex_layout_id];
  mat->pipeline_layout = create_pipeline_layout(dev, temp_layouts, spec->num_descriptor_set_layouts);
  mat->pipeline =
      create_default_graphics_pipeline(ctx, render_pass, vert_mod, frag_mod, vertex_layout, mat->pipeline_layout);

  vkDestroyShaderModule(dev, vert_mod, NULL);
  vkDestroyShaderModule(dev, frag_mod, NULL);
}

inline void init_gl_vertex_layout(VertexLayoutID vertex_layout_id, GLuint vao, GLuint *vbos, u32 num_vbos, GLuint ebo) {
  if (generated_opengl_vertex_array_initializers[vertex_layout_id]) {
    generated_opengl_vertex_array_initializers[vertex_layout_id](vao, vbos, num_vbos, ebo);
  }
}

inline u32 shader_handles_to_gl_program(ShaderHandle vertex_shader_handle, ShaderHandle fragment_shader_handle) {
  return link_gl_program(
      generated_shader_specs[vertex_shader_handle]->opengl_glsl,
      generated_shader_specs[fragment_shader_handle]->opengl_glsl
  );
}

// TODO deprecate lol
inline GLMesh create_gl_mesh_with_vertex_layout(
    const f32 *arr, f32 num_f32s, u32 num_vertices, VertexLayoutID vertex_layout_id, u32 draw_mode
) {
  GLMesh opengl_mesh = create_gl_mesh(arr, num_f32s, num_vertices, draw_mode);
  init_gl_vertex_layout(vertex_layout_id, opengl_mesh.vao, opengl_mesh.vbos, 1, 0);
  return opengl_mesh;
}

inline void init_gl_mesh_vao(GLMesh *opengl_mesh, ShaderHandle shader_handle) {
  VertexLayoutID vertex_layout_id = generated_shader_specs[shader_handle]->vertex_layout_id;
  u32 vao = create_vao();
  init_gl_vertex_layout(vertex_layout_id, vao, opengl_mesh->vbos, opengl_mesh->num_vbos, 0);
  opengl_mesh->vao = vao;
}
