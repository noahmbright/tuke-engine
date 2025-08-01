#include <GLFW/glfw3.h> // TODO Remove

#include "camera.h"
#include "tuke_engine.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/trigonometric.hpp"

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
  if (camera->pitch < pi_over_2) {
    camera->pitch = pi_over_2;
  }

  camera->direction.x = glm::cos(camera->pitch) * glm::cos(camera->yaw);
  camera->direction.y = glm::sin(camera->pitch);
  camera->direction.z = glm::cos(camera->pitch) * glm::sin(camera->yaw);
  camera->right = glm::cross(camera->direction, camera->up);
}

Camera new_camera(CameraType type, const glm::vec3 &pos,
                  const glm::vec3 &direction, const glm::vec3 &up,
                  const glm::vec3 &right) {
  Camera camera;
  camera.type = type;
  camera.position = pos;
  camera.direction = direction;
  camera.up = up;
  camera.right = right;

  switch (camera.type) {

  case CameraType::Camera2D:
    camera.move_camera_function = move_camera_2d;
    break;

  case CameraType::Camera3D:
  case CameraType::CameraFPS:
    // TODO FPS

    f32 xy_magnitude =
        glm::length(glm::vec2{camera.direction.x, camera.direction.y});
    f32 xz_magnitude =
        glm::length(glm::vec2{camera.direction.x, camera.direction.z});

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
    camera.move_camera_function = move_camera_3d;
    break;
  }

  // TODO where does this stuff go?
  // unsigned ubo;
  // glGenBuffers(1, &ubo);
  // glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  // glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraMatrices), nullptr,
  //             GL_DYNAMIC_DRAW);
  // glBindBuffer(GL_UNIFORM_BUFFER, 0);
  // glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
  // camera.ubo = ubo;

  return camera;
}

// expect movement direction contains the WASD/arrow key inputs in x and y,
// and if the shift key is pressed, the w component will be 2, if not, 1
void move_camera_2d(Camera *camera, f32 delta_t,
                    const glm::vec4 &movement_direction) {

  const glm::vec3 dir3{movement_direction.x, movement_direction.y,
                       movement_direction.z};

  camera->position += camera->speed * delta_t * (movement_direction.w) * dir3;
}

// movement direction will have x and y components
// y means move in camera->direction
// x means move along camera->right
void move_camera_3d(Camera *camera, f32 delta_t,
                    const glm::vec4 &movement_direction) {

  f32 distance = camera->speed * delta_t * movement_direction.w;

  camera->position += distance * movement_direction.y * camera->direction;
  camera->position += distance * movement_direction.x * camera->right;
}

void move_camera(Camera *camera, f32 delta_t,
                 const glm::vec4 &movement_direction) {
  camera->move_camera_function(camera, delta_t, movement_direction);
}

glm::mat4 look_at_from_camera(const Camera *camera) {
  return glm::lookAt(camera->position, camera->position + camera->direction,
                     camera->up);
}

glm::mat4 perspective_projection_from_camera(const Camera *camera,
                                             u32 window_width,
                                             u32 window_height) {
  glm::mat4 proj =
      glm::perspective(glm::radians(camera->fovy),
                       f32(window_width) / f32(window_height), 0.1f, 100.0f);
  proj[1][1] *= -1.0f * camera->y_needs_inverted;
  return proj;
}

CameraMatrices new_camera_matrices(const Camera *camera, u32 window_width,
                                   u32 window_height) {
  CameraMatrices camera_matrices;
  camera_matrices.view = look_at_from_camera(camera);
  camera_matrices.projection =
      perspective_projection_from_camera(camera, window_width, window_height);
  return camera_matrices;
}

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
