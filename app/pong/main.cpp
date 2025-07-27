#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "pong.h"
#include "vulkan_base.h"

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;
  glm::vec3 camera_pos{0.0f, 0.0f, 30.0f};
  Camera camera = new_camera(CameraType::Camera2D, camera_pos);

  // TODO how to get model onto GPU? uniform? something else?
  glm::mat4 arena_model = glm::scale(glm::mat4(1.0f), arena_dimensions0);

  // TODO make camera matrices only on camera movement
  // TODO buffer only on resize
  // TODO when can I most efficiently multiply matrices together, view/proj?
  // When would that not be possible?
  int height, width;
  glfwGetFramebufferSize(ctx->window, &width, &height);
  const CameraMatrices camera_matrices =
      new_camera_matrices(&camera, width, height);
  glm::mat4 camera_vp = camera_matrices.projection * camera_matrices.view;

  glm::mat4 player_paddle_model = glm::mat4(1.0f);

  // TODO want a system for updating parts of a buffer by name
  // uniform buffer structure: camera vp, background model, paddle model
  write_to_uniform_buffer(&state.uniform_buffer, &camera_vp, 0 * MAT4_SIZE,
                          MAT4_SIZE);
  write_to_uniform_buffer(&state.uniform_buffer, &arena_model, 1 * MAT4_SIZE,
                          MAT4_SIZE);
  write_to_uniform_buffer(&state.uniform_buffer, &player_paddle_model,
                          2 * MAT4_SIZE, MAT4_SIZE);

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
