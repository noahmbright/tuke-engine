#include "camera.h"
#include "pong.h"
#include "vulkan_base.h"
#include <vulkan/vulkan_core.h>

int main() {

  State state = setup_state("Tuke Pong");
  VulkanContext *ctx = &state.context;

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
