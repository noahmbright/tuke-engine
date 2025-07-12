#include "camera.h"
#include "pong.h"
#include "vulkan_base.h"

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;
  // Camera camera = new_camera(CameraType::Camera2D);

  while (!glfwWindowShouldClose(ctx->window)) {
    glfwPollEvents();
    // float t = glfwGetTime();
    render(&state);
  }

  destroy_state(&state);
  return 0;
}
