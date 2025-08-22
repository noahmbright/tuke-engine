#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "pong.h"
#include "vulkan_base.h"
#include <vulkan/vulkan_core.h>

struct InstanceData {
  glm::vec4 position;
};

struct AllInstanceData {
  InstanceData left_paddle;
  // InstanceData right_paddle;
  // InstanceData ball;
};

struct CoherentStreamingBuffer {
  VulkanBuffer vulkan_buffer;
  u8 *data;
  u32 size;
  u32 head;
};

CoherentStreamingBuffer
create_coherent_streaming_buffer(const VulkanContext *ctx, u32 size) {
  CoherentStreamingBuffer coherent_streaming_buffer;

  coherent_streaming_buffer.vulkan_buffer =
      create_buffer(ctx, BUFFER_TYPE_COHERENT_STREAMING, size);

  vkMapMemory(ctx->device, coherent_streaming_buffer.vulkan_buffer.memory, 0,
              size, 0, (void **)&coherent_streaming_buffer.data);

  coherent_streaming_buffer.size = size;
  coherent_streaming_buffer.head = 0;

  return coherent_streaming_buffer;
}

void write_to_streaming_buffer(
    CoherentStreamingBuffer *coherent_streaming_buffer, void *data, u32 size) {

  assert(size <= coherent_streaming_buffer->size);

  u8 *dest = coherent_streaming_buffer->data;
  u32 offset = coherent_streaming_buffer->head;
  bool overflowed = (offset + size > coherent_streaming_buffer->size);
  if (overflowed) {
    offset = 0;
  }

  memcpy(dest + offset, data, size);

  coherent_streaming_buffer->head = overflowed ? size : offset + size;
}

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;
  glm::vec3 camera_pos{0.0f, 0.0f, 30.0f};
  Camera camera = new_camera(CameraType::Camera2D, camera_pos);
  camera.y_needs_inverted = true;

  const u32 num_instances = 1;
  u32 streaming_buffer_size = sizeof(InstanceData) * num_instances;

  AllInstanceData all_instance_data;
  all_instance_data.left_paddle.position = left_paddle_pos0;

  // TODO how to get model onto GPU? uniform? something else?
  glm::mat4 arena_model = glm::scale(glm::mat4(1.0f), arena_dimensions0);

  // TODO make camera matrices only on camera movement
  // TODO buffer only on resize
  // TODO when can I most efficiently multiply matrices together, view/proj?
  //    When would that not be possible?
  int height, width;
  glfwGetFramebufferSize(ctx->window, &width, &height);
  const CameraMatrices camera_matrices =
      new_camera_matrices(&camera, width, height);
  glm::mat4 camera_vp = camera_matrices.projection * camera_matrices.view;
  glm::mat4 player_paddle_model = glm::mat4(1.0f);

  // uniform buffer structure: camera vp, background model, paddle model
  write_to_uniform_buffer(&state.uniform_buffer, &camera_vp,
                          state.uniform_writes.camera_vp);
  write_to_uniform_buffer(&state.uniform_buffer, &arena_model,
                          state.uniform_writes.arena_model);
  write_to_uniform_buffer(&state.uniform_buffer, &player_paddle_model,
                          state.uniform_writes.player_paddle_model);

  // main loop
  f64 t_prev = glfwGetTime();
  while (!glfwWindowShouldClose(ctx->window)) {
    glfwPollEvents();
    f64 t = glfwGetTime();
    f64 dt = t - t_prev;
    t_prev = t;
    (void)dt;

    process_inputs(&state);
    render(&state);
  }

  destroy_state(&state);
  return 0;
}
