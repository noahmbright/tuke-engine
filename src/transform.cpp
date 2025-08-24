#include "transform.h"
#include "glm/ext/matrix_transform.hpp"

void generate_transform(Transform *transform, glm::mat4 *model) {

  const glm::mat4 translated =
      glm::translate(glm::mat4(1.0f), *transform->translation);

  const glm::mat4 rotated =
      glm::rotate(translated, *transform->theta, *transform->rotation_axis);

  *model = glm::scale(rotated, *transform->scale);
  transform->dirty = false;
}

Transform create_transform(glm::vec3 *translation) {
  Transform transform;

  transform.translation = translation;
  transform.theta = &default_rotation_angle;
  transform.rotation_axis = &default_rotation_axis;
  transform.scale = &default_scale;
  transform.dirty = false;

  return transform;
}
