#include "pong.h"

int main() {

  State state = setup_state("Tuke Pong");

  f64 t_prev = glfwGetTime();
  f64 second_accumulator = 0.0f;
  f64 total_time = 0.0f;
  while (!glfwWindowShouldClose(state.window)) {
    glfwPollEvents();
    f64 t = glfwGetTime();
    f64 dt = t - t_prev;
    second_accumulator += dt;
    total_time += dt;
    t_prev = t;

    process_inputs(&state, dt);
    update_game_state(&state, dt);
    render(&state);

    if (second_accumulator > 0.0f) {
      f64 fps = state.current_frame / total_time;
      char title[64];
      snprintf(title, sizeof(title), "My Vulkan App - %.1f FPS", fps);
      glfwSetWindowTitle(state.window, title);
    }
  }

  destroy_state(&state);
  return 0;
}
