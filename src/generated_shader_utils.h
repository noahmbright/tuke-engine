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

inline void
update_vulkan_material(const VulkanContext *ctx, const DescriptorWrite *writes, u32 num_writes, VulkanMaterial *mat) {
  assert(num_writes < 16);
  VkWriteDescriptorSet vk_writes[16] = {};

  for (u32 write_idx = 0; write_idx < num_writes; write_idx++) {
    const DescriptorWrite *write = &writes[write_idx];

    // Find set
    u32 set = mat->num_descriptor_sets;
    for (u32 i = 0; i < mat->num_descriptor_sets; i++) {
      if (write->set_id == mat->indirection_table[i]) {
        set = i;
        break;
      }
    }
    assert(set != mat->num_descriptor_sets);

    const VkWriteDescriptorSet *templates = mat->descriptor_set_writes[set];

    u32 len = mat->descriptor_set_write_lens[set];
    for (u32 i = 0; i < len; i++) {
      if (templates[i].dstBinding == write->binding) {
        vk_writes[write_idx] = templates[i];
        vk_writes[write_idx].dstSet = mat->descriptor_sets[set];

        if (vk_writes[write_idx].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
          vk_writes[write_idx].pBufferInfo = &writes[write_idx].buffer_info;
        } else if (vk_writes[write_idx].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
          vk_writes[write_idx].pImageInfo = &writes[write_idx].image_info;
        }
      }
    }
  }

  vkUpdateDescriptorSets(ctx->device, num_writes, vk_writes, 0, NULL);
}

// TODO would like this render pass removed
inline void init_program_spec(
    VulkanContext *ctx,
    VkRenderPass render_pass,
    const PipelineConfig *config,
    const ProgramSpec *spec,
    VulkanMaterial *mat
) {
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

    mat->indirection_table[i] = spec->binding_list_ids[i];
  }
  mat->num_descriptor_sets = spec->num_descriptor_set_layouts;

  const VkPipelineVertexInputStateCreateInfo *vertex_layout = &generated_vulkan_vertex_layouts[spec->vertex_layout_id];

  u32 pc_size = spec->push_constant_size;
  VkPushConstantRange range = {.offset = 0, .size = pc_size, .stageFlags = spec->push_constant_stage_flags};
  VkPushConstantRange *range_ptr = pc_size > 0 ? &range : NULL;
  u32 num_push_constants = pc_size > 0 ? 1 : 0;
  mat->push_constant_stage_flags = spec->push_constant_stage_flags;
  mat->push_constant_size = pc_size;

  mat->pipeline_layout =
      create_pipeline_layout(dev, temp_layouts, spec->num_descriptor_set_layouts, range_ptr, num_push_constants);
  mat->pipeline = create_graphics_pipeline(
      ctx->device, render_pass, vert_mod, frag_mod, vertex_layout, mat->pipeline_layout, ctx->pipeline_cache, config
  );

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
