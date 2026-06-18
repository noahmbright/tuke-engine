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

  if (camera->pitch > PI_OVER_2) {
    camera->pitch = PI_OVER_2;
  }
  if (camera->pitch < -PI_OVER_2) {
    camera->pitch = -PI_OVER_2;
  }

  f32 cos_pitch = cosf(camera->pitch);
  camera->direction.x = cos_pitch * cosf(camera->yaw);
  camera->direction.y = sinf(camera->pitch);
  camera->direction.z = cos_pitch * sinf(camera->yaw);
  camera->right = glm::cross(camera->direction, camera->up);
}

Camera create_camera(CameraType type, glm::vec3 pos, glm::vec3 direction, glm::vec3 up, glm::vec3 right) {
  Camera camera;
  camera.type = type;
  camera.position = pos;
  camera.direction = direction;
  camera.up = up;
  camera.right = right;
  camera.has_moused_yet = false;

  f32 xy_magnitude = glm::length(glm::vec2{camera.direction.x, camera.direction.y});
  f32 xz_magnitude = glm::length(glm::vec2{camera.direction.x, camera.direction.z});

  if (xy_magnitude < EPSILON) {
    camera.yaw = -1.57;
  } else {
    camera.yaw = glm::atan(camera.direction.z / xy_magnitude);
  }

  if (xz_magnitude <= EPSILON) {
    camera.pitch = -3.14;
  } else {
    camera.pitch = glm::atan(camera.direction.y / xz_magnitude);
  }

  camera.y_needs_inverted = false;
  return camera;
}
