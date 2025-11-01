#include "camera.h"

void process_mouse_input3d(Camera *camera, f64 xpos, f64 ypos) {
  if (!camera->has_moused_yet) {
    camera->has_moused_yet = true;
    camera->last_mouse_x = xpos;
    camera->last_mouse_y = ypos;
  }

  f64 dx = xpos - camera->last_mouse_x;
  f64 dy = ypos - camera->last_mouse_y;
  camera->last_mouse_x = xpos;
  camera->last_mouse_y = ypos;
  camera->yaw += camera->mouse_sensitivity * dx;
  camera->pitch -= camera->mouse_sensitivity * dy;

  const f32 pi_over_2 = PI / 2.0f;
  if (camera->pitch > pi_over_2) {
    camera->pitch = pi_over_2;
  }
  if (camera->pitch < -pi_over_2) {
    camera->pitch = -pi_over_2;
  }

  camera->direction.x = glm::cos(camera->pitch) * glm::cos(camera->yaw);
  camera->direction.y = glm::sin(camera->pitch);
  camera->direction.z = glm::cos(camera->pitch) * glm::sin(camera->yaw);
  camera->right = glm::cross(camera->direction, camera->up);
}

Camera new_camera(CameraType type, const glm::vec3 &pos, const glm::vec3 &direction, const glm::vec3 &up,
                  const glm::vec3 &right) {
  Camera camera;
  camera.type = type;
  camera.position = pos;
  camera.direction = direction;
  camera.up = up;
  camera.right = right;

  switch (camera.type) {

  case CAMERA_TYPE_2D:
    camera.move_camera_function = camera_move_2d;
    break;

  case CAMERA_TYPE_3D:
  case CAMERA_TYPE_FPS:
    // TODO FPS

    f32 xy_magnitude = glm::length(glm::vec2{camera.direction.x, camera.direction.y});
    f32 xz_magnitude = glm::length(glm::vec2{camera.direction.x, camera.direction.z});

    const f32 tolerance = 1e-5;
    if (xy_magnitude < tolerance) {
      camera.yaw = -1.57;
    } else {
      camera.yaw = glm::atan(camera.direction.z / xy_magnitude);
    }

    if (xz_magnitude <= tolerance) {
      camera.pitch = -3.14;
    } else {
      camera.pitch = glm::atan(camera.direction.y / xz_magnitude);
    }

    camera.move_camera_function = camera_move_3d;
    break;
  }

  return camera;
}

// TODO decouple from window
// static void scroll_callback(GLFWwindow *window, f64 xoffset, f64 yoffset) {
//  Camera *camera = (Camera *)glfwGetWindowUserPointer(window);
//
//  (void)xoffset;
//  (void)yoffset;
//
//  camera->fovy -= (f32)yoffset;
//  if (camera->fovy < 1.0f)
//    camera->fovy = 1.0f;
//
//  if (camera->fovy > 45.0f)
//    camera->fovy = 45.0f;
//}

// void buffer_camera_matrices_to_gl_uniform_buffer(
//     const CameraMatrices *camera_matrices) {
//
//   CameraMatrices *uniform_buffer_range_ptr = (CameraMatrices
//   *)glMapBufferRange(
//       GL_UNIFORM_BUFFER, 0, sizeof(CameraMatrices),
//       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
//
//   if (uniform_buffer_range_ptr) {
//     *uniform_buffer_range_ptr = *camera_matrices;
//     glUnmapBuffer(GL_UNIFORM_BUFFER);
//   }
// #if 1
//   else {
//     fprintf(stderr, "buffer_camera_matrices_to_gl_uniform_buffer: failed to "
//                     "get uniform_buffer_range_ptr\n");
//   }
// #endif
//}
