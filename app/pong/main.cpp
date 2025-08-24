#include "camera.h"
#include "pong.h"
#include "vulkan_base.h"
#include <vulkan/vulkan_core.h>

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;
  glm::vec3 camera_pos{0.0f, 0.0f, 30.0f};
  Camera camera = new_camera(CameraType::Camera2D, camera_pos);
  camera.y_needs_inverted = true;

  // TODO how to get model onto GPU? uniform? something else? push constant?
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

  // uniform buffer structure: camera vp, background model, paddle model
  write_to_uniform_buffer(&state.uniform_buffer, &camera_vp,
                          state.uniform_writes.camera_vp);
  write_to_uniform_buffer(&state.uniform_buffer, &arena_model,
                          state.uniform_writes.arena_model);

  // main loop
  f64 t_prev = glfwGetTime();
  while (!glfwWindowShouldClose(ctx->window)) {
    glfwPollEvents();
    f64 t = glfwGetTime();
    f64 dt = t - t_prev;
    t_prev = t;

    process_inputs(&state, dt);
    render(&state);
    update_game_state(&state, dt);
  }

  destroy_state(&state);
  return 0;
}
