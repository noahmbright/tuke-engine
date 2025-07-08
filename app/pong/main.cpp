#include "pong.h"
#include "vulkan_base.h"

int main() {
  VulkanContext context;

  const u32 paddle_vertices_size = sizeof(paddle_vertices);
  const u32 indices_size = sizeof(unit_square_indices);
  const u32 total_size = paddle_vertices_size + indices_size;

  VulkanBuffer vertex_buffer =
      create_buffer(&context, BUFFER_TYPE_VERTEX, total_size);
  VulkanBuffer index_buffer =
      create_buffer(&context, BUFFER_TYPE_INDEX, total_size);

  StagingArena staging_arena = create_staging_arena(&context, total_size);
  u32 paddle_vertices_offset =
      stage_data_explicit(&context, &staging_arena, paddle_vertices,
                          sizeof(paddle_vertices), vertex_buffer.buffer, 0);
  u32 quad_indices_offset =
      stage_data_explicit(&context, &staging_arena, unit_square_indices,
                          sizeof(unit_square_indices), index_buffer.buffer, 0);

  return 0;
}
