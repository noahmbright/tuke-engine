#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "pong.h"
#include "vulkan_base.h"

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;
  glm::vec3 camera_pos{0.0f, 0.0f, 100.0f};
  Camera camera = new_camera(CameraType::Camera2D, camera_pos);

  glm::mat4 identity = glm::mat4(1.0f);
  MVPUniform mvp;
  mvp.model = glm::scale(glm::mat4(1.0f), arena_dimensions0);
  // TODO make camera matrices only on camera movement
  // TODO buffer only on resize
  // will need a callback? How to integrate with polling?
  int height, width;
  glfwGetFramebufferSize(ctx->window, &width, &height);
  const CameraMatrices camera_matrices =
      new_camera_matrices(&camera, width, height);
  mvp.view = camera_matrices.view;
  mvp.projection = camera_matrices.projection;
  mvp.model = identity;
  mvp.projection = identity;
  mvp.view = identity;
  write_to_uniform_buffer(&state.uniform_buffer, &mvp, 0, sizeof(mvp));

  f64 t_prev = glfwGetTime();
  while (!glfwWindowShouldClose(ctx->window)) {
    glfwPollEvents();
    f64 t = glfwGetTime();
    f64 dt = t - t_prev;
    t_prev = t;
    (void)dt;

    render(&state);
  }

  destroy_state(&state);
  return 0;
}
