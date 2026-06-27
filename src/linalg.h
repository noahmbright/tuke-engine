#pragma once

#include "tuke_engine.h"

// Vectors
typedef struct {
  f32 x;
  f32 y;
  f32 z;
  f32 w;
} Vec4;

typedef struct {
  f32 x;
  f32 y;
  f32 z;
} Vec3;

typedef struct {
  f32 x;
  f32 y;
} Vec2;

// Matrices
// Storage is column major for use with the graphics APIs
//
// Domino naming convention:
//  turn model_position into model_projection using:
//  model_projection = projection_from_view * view_from_world * world_from_model * model_position
typedef struct {
  f32 arr[4][4];
} Mat4;

typedef struct {
  f32 arr[3][3];
} Mat3;

// i^2 = j^2 = k^2 = ijk = -1
// ij = -ji = k
// jk = -kj = i
// ki = -ik = j
typedef struct {
  f32 real;
  f32 i;
  f32 j;
  f32 k;
} Quaternion;

Vec2 vec2(f32 x, f32 y);
Vec3 vec3(f32 x, f32 y, f32 z);
Vec4 vec4(f32 x, f32 y, f32 z, f32 w);
Mat4 mat4();

f32 dot_v2(Vec2 v, Vec2 u);
f32 dot_v3(Vec3 v, Vec3 u);
f32 dot_v4(Vec4 v, Vec4 u);

Vec2 add_v2(Vec2 v, Vec2 u);
Vec3 add_v3(Vec3 v, Vec3 u);
Vec4 add_v4(Vec4 v, Vec4 u);

void inc_v2(Vec2 *v, Vec2 u);
void inc_v3(Vec3 *v, Vec3 u);
void inc_v4(Vec4 *v, Vec4 u);

Vec2 sub_v2(Vec2 v, Vec2 u);
Vec3 sub_v3(Vec3 v, Vec3 u);
Vec4 sub_v4(Vec4 v, Vec4 u);

Vec2 scale_v2(Vec2 x, f32 s);
Vec3 scale_v3(Vec3 x, f32 s);

Vec3 cross_v3(Vec3 v, Vec3 u);

Vec2 abs_v2(Vec2 v);
f32 len2_v2(Vec2 v);
f32 len_v2(Vec2 v);

f32 len2_v3(Vec3 v);
f32 len_v3(Vec3 v);
Vec3 normalize_v3(Vec3 v);

// TODO Rodrigues rotation formula
Mat4 translation_m4(Vec3 v);
void translate_m4(Vec3 v, Mat4 *m);
void scale_m4(Vec3 v, Mat4 *m);
void mult_m4(const Mat4 *left, const Mat4 *right, Mat4 *out);
Vec4 mvm4(const Mat4 *mat, Vec4 v);
Mat4 make_camera_from_world(Vec3 pos, Vec3 forward, Vec3 up);
Mat4 perspective_proj(f32 aspect, f32 hfov, f32 z_near, f32 z_far);

void log_v3(Vec3 v);
void log_v4(Vec4 v);
bool isfinite_v3(Vec3 v);

void log_m4(const Mat4 *m);
bool mat4_has_nan(const Mat4 *m);
