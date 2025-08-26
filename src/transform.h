#pragma once

#include "tuke_engine.h"

const glm::vec3 default_translation = glm::vec3(0.0f);
const glm::vec3 default_scale = glm::vec3(1.0f);
const glm::vec3 default_rotation_axis = glm::vec3(0.0f, 0.0f, 1.0f);
const f32 default_rotation_angle = 0.0f;

// TODO engine: rewrite to SoA - make this *the* SoA
//  Do I need a general one of these? I can just allocate this stuff
//  per app with the right static sizes
struct Transform {
  const glm::vec3 *translation;
  // TODO engine: move to quaternions
  const f32 *theta;
  const glm::vec3 *rotation_axis;
  const glm::vec3 *scale;

  bool dirty;
};

void generate_transform(Transform *transform, glm::mat4 *model);
Transform create_transform(glm::vec3 *translation);
