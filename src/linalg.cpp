#include "linalg.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

Vec2 vec2(f32 x, f32 y) {
  Vec2 v;
  v.x = x;
  v.y = y;
  return v;
}

Vec3 vec3(f32 x, f32 y, f32 z) {
  Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

Vec4 vec4(f32 x, f32 y, f32 z, f32 w) {
  Vec4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

Vec2 scale_v2(Vec2 v, f32 s) {
  Vec2 u;
  u.x = v.x * s;
  u.y = v.y * s;
  return u;
}

Vec3 scale_v3(Vec3 v, f32 s) {
  Vec3 u;
  u.x = v.x * s;
  u.y = v.y * s;
  u.z = v.z * s;
  return u;
}

f32 dot_v3(Vec3 v, Vec3 u) { return v.x * u.x + v.y * u.y + v.z * u.z; }

f32 dot_v4(Vec4 v, Vec4 u) { return v.x * u.x + v.y * u.y + v.z * u.z + v.w * u.w; }

Vec2 add_v2(Vec2 v, Vec2 u) {
  Vec2 res;
  res.x = v.x + u.x;
  res.y = v.y + u.y;
  return res;
}

Vec3 add_v3(Vec3 v, Vec3 u) {
  Vec3 res;
  res.x = v.x + u.x;
  res.y = v.y + u.y;
  res.z = v.z + u.z;
  return res;
}

Vec4 add_v4(Vec4 v, Vec4 u) {
  Vec4 res;
  res.x = v.x + u.x;
  res.y = v.y + u.y;
  res.z = v.z + u.z;
  res.w = v.w + u.w;
  return res;
}

void inc_v2(Vec2 *v, Vec2 u) {
  v->x += u.x;
  v->y += u.y;
}

void inc_v3(Vec3 *v, Vec3 u) {
  v->x += u.x;
  v->y += u.y;
  v->z += u.z;
}

void inc_v4(Vec4 *v, Vec4 u) {
  v->x += u.x;
  v->y += u.y;
  v->z += u.z;
  v->w += u.w;
}

Vec2 sub_v2(Vec2 v, Vec2 u) {
  Vec2 res;
  res.x = v.x - u.x;
  res.y = v.y - u.y;
  return res;
}

Vec3 sub_v3(Vec3 v, Vec3 u) {
  Vec3 res;
  res.x = v.x - u.x;
  res.y = v.y - u.y;
  res.z = v.z - u.z;
  return res;
}

Vec4 sub_v4(Vec4 v, Vec4 u) {
  Vec4 res;
  res.x = v.x - u.x;
  res.y = v.y - u.y;
  res.z = v.z - u.z;
  res.w = v.w - u.w;
  return res;
}

Vec3 cross_v3(Vec3 v, Vec3 u) {
  Vec3 w = {
      .x = v.y * u.z - v.z * u.y,
      .y = v.z * u.x - v.x * u.z,
      .z = v.x * u.y - v.y * u.x,
  };
  return w;
}

Vec2 abs_v2(Vec2 v) {
  Vec2 u;
  u.x = fabs(v.x);
  u.y = fabs(v.y);
  return u;
}

f32 len2_v2(Vec2 v) { return v.x * v.x + v.y * v.y; }

f32 len_v2(Vec2 v) {
  f32 len2 = v.x * v.x + v.y * v.y;
  return sqrtf(len2);
}

f32 len2_v3(Vec3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

f32 len_v3(Vec3 v) {
  f32 len2 = v.x * v.x + v.y * v.y + v.z * v.z;
  return sqrtf(len2);
}

Vec3 normalize_v3(Vec3 v) {
  Vec3 u;
  f32 len = len_v3(v);
  if (len < EPSILON) {
    u.x = 0.0f;
    u.y = 0.0f;
    u.z = 0.0f;
  } else {
    u.x = v.x / len;
    u.y = v.y / len;
    u.z = v.z / len;
  }
  return u;
}

////////////////////////////////////////////////////////////////
// Matrices
////////////////////////////////////////////////////////////////

// Identity
Mat4 mat4() {
  Mat4 m;
  memset(m.arr, 0, sizeof(m.arr));
  m.arr[0][0] = 1.0f;
  m.arr[1][1] = 1.0f;
  m.arr[2][2] = 1.0f;
  m.arr[3][3] = 1.0f;
  return m;
}

// | 1 0 0 x |    | a |     | a + x |
// | 0 1 0 y | x  | b |  =  | b + y |
// | 0 0 1 z |    | c |     | c + z |
// | 0 0 0 1 |    | 1 |     |   1   |
Mat4 translation_m4(Vec3 v) {
  Mat4 m;
  memset(m.arr, 0, sizeof(m.arr));
  m.arr[3][0] = v.x;
  m.arr[3][1] = v.y;
  m.arr[3][2] = v.z;
  m.arr[0][0] = 1.0f;
  m.arr[1][1] = 1.0f;
  m.arr[2][2] = 1.0f;
  return m;
}

void scale_m4(Vec3 v, Mat4 *m) {
  m->arr[0][0] *= v.x;
  m->arr[1][0] *= v.x;
  m->arr[2][0] *= v.x;
  m->arr[3][0] *= v.x;

  m->arr[0][1] *= v.y;
  m->arr[1][1] *= v.y;
  m->arr[2][1] *= v.y;
  m->arr[3][1] *= v.y;

  m->arr[0][2] *= v.z;
  m->arr[1][2] *= v.z;
  m->arr[2][2] *= v.z;
  m->arr[3][2] *= v.z;
}

void translate_m4(Vec3 v, Mat4 *m) {
  m->arr[0][0] += m->arr[0][3] * v.x;
  m->arr[1][0] += m->arr[1][3] * v.x;
  m->arr[2][0] += m->arr[2][3] * v.x;
  m->arr[3][0] += m->arr[3][3] * v.x;

  m->arr[0][1] += m->arr[0][3] * v.y;
  m->arr[1][1] += m->arr[1][3] * v.y;
  m->arr[2][1] += m->arr[2][3] * v.y;
  m->arr[3][1] += m->arr[3][3] * v.y;

  m->arr[0][2] += m->arr[0][3] * v.z;
  m->arr[1][2] += m->arr[1][3] * v.z;
  m->arr[2][2] += m->arr[2][3] * v.z;
  m->arr[3][2] += m->arr[3][3] * v.z;
}

Vec4 mvm4(const Mat4 *mat, Vec4 v) {
  Vec4 u;
  const float (*m)[4] = mat->arr;
  u.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w;
  u.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w;
  u.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w;
  u.w = m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w;
  return u;
}

void mult_m4(const Mat4 *left, const Mat4 *right, Mat4 *out) {
  memset(out->arr, 0, sizeof(out->arr));

  const float (*l)[4] = left->arr;
  const float (*r)[4] = right->arr;

  // Storage is column major
  // First index is column, second is row
  // Left iterates over columns, right iterates over rows
  // Fix out col and out row, then multiply matching index left column (first idx) with right row (second idx)

  // First row of out, i.e. all columns of out at row 0
  // Use all columns of left per sum (first idx), all rows of r
  // Fix first row of left, advance column of right
  out->arr[0][0] = l[0][0] * r[0][0] + l[1][0] * r[0][1] + l[2][0] * r[0][2] + l[3][0] * r[0][3];
  out->arr[1][0] = l[0][0] * r[1][0] + l[1][0] * r[1][1] + l[2][0] * r[1][2] + l[3][0] * r[1][3];
  out->arr[2][0] = l[0][0] * r[2][0] + l[1][0] * r[2][1] + l[2][0] * r[2][2] + l[3][0] * r[2][3];
  out->arr[3][0] = l[0][0] * r[3][0] + l[1][0] * r[3][1] + l[2][0] * r[3][2] + l[3][0] * r[3][3];

  out->arr[0][1] = l[0][1] * r[0][0] + l[1][1] * r[0][1] + l[2][1] * r[0][2] + l[3][1] * r[0][3];
  out->arr[1][1] = l[0][1] * r[1][0] + l[1][1] * r[1][1] + l[2][1] * r[1][2] + l[3][1] * r[1][3];
  out->arr[2][1] = l[0][1] * r[2][0] + l[1][1] * r[2][1] + l[2][1] * r[2][2] + l[3][1] * r[2][3];
  out->arr[3][1] = l[0][1] * r[3][0] + l[1][1] * r[3][1] + l[2][1] * r[3][2] + l[3][1] * r[3][3];

  out->arr[0][2] = l[0][2] * r[0][0] + l[1][2] * r[0][1] + l[2][2] * r[0][2] + l[3][2] * r[0][3];
  out->arr[1][2] = l[0][2] * r[1][0] + l[1][2] * r[1][1] + l[2][2] * r[1][2] + l[3][2] * r[1][3];
  out->arr[2][2] = l[0][2] * r[2][0] + l[1][2] * r[2][1] + l[2][2] * r[2][2] + l[3][2] * r[2][3];
  out->arr[3][2] = l[0][2] * r[3][0] + l[1][2] * r[3][1] + l[2][2] * r[3][2] + l[3][2] * r[3][3];

  out->arr[0][3] = l[0][3] * r[0][0] + l[1][3] * r[0][1] + l[2][3] * r[0][2] + l[3][3] * r[0][3];
  out->arr[1][3] = l[0][3] * r[1][0] + l[1][3] * r[1][1] + l[2][3] * r[1][2] + l[3][3] * r[1][3];
  out->arr[2][3] = l[0][3] * r[2][0] + l[1][3] * r[2][1] + l[2][3] * r[2][2] + l[3][3] * r[2][3];
  out->arr[3][3] = l[0][3] * r[3][0] + l[1][3] * r[3][1] + l[2][3] * r[3][2] + l[3][3] * r[3][3];
}

// Clip space in vulkan goes from -1 to 1 in x and y, and 0  to 1 in z.
//            in opengl goes from -1 to 1 in x and y, and -1 to 1 in z.
//
// The rasterization HW pipeline has hardcoded division by z between vertex/fragment stages.
// This saves us because perspective projection requires a division by z, which is
// a nonlinear transformation.
// However, this also requires that we save z artifically in the matrix, because
// z/z = 1 and we lose info. This is why we copy it into w.
Mat4 perspective_proj(f32 aspect, f32 hfov, f32 z_near, f32 z_far) {
  Mat4 m = {};

  f32 tan_inv = 1.0f / tanf(hfov / 2.0f);

  m.arr[0][0] = tan_inv / aspect;
  m.arr[1][1] = tan_inv;
  m.arr[2][2] = -1.0f / (1.0f - z_near / z_far); // works for Vulkan
  m.arr[3][2] = 1.0f * m.arr[2][2];              // works for Vulkan
  m.arr[2][3] = -1.0f;

  return m;
}

void log_v3(Vec3 v) {
  printf("| %9.4f |\n", v.x);
  printf("| %9.4f |\n", v.y);
  printf("| %9.4f |\n", v.z);
}

void log_v4(Vec4 v) {
  printf("| %9.4f |\n", v.x);
  printf("| %9.4f |\n", v.y);
  printf("| %9.4f |\n", v.z);
  printf("| %9.4f |\n", v.w);
}

bool isfinite_v3(Vec3 v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }

void log_m4(const Mat4 *m) {
  for (int r = 0; r < 4; r++) {
    printf("| %9.4f %9.4f %9.4f %9.4f |\n", m->arr[0][r], m->arr[1][r], m->arr[2][r], m->arr[3][r]);
  }
}

bool mat4_has_nan(const Mat4 *m) {
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      if (isnan(m->arr[c][r])) {
        return true;
      }
    }
  }
  return false;
}

// "Camera matrix": classic v in MVP stack.
// Projection works nicely whenever the camera is at the origin -
// so move into the reference frame where the camera is at the origin.
//
// pos, dir, and up are the parameters of the camera in world space.
// z is screwed up because of OpenGL historical cruft.
Mat4 make_camera_from_world(Vec3 pos, Vec3 forward, Vec3 up) {
  Mat4 m = {};

  // Rotate the world such that camera right aligns with x, up with y, and forward with z
  Vec3 right = normalize_v3(cross_v3(forward, up));
  up = cross_v3(right, forward);

  m.arr[0][0] = right.x;
  m.arr[1][0] = right.y;
  m.arr[2][0] = right.z;

  m.arr[0][1] = up.x;
  m.arr[1][1] = up.y;
  m.arr[2][1] = up.z;

  m.arr[0][2] = -forward.x;
  m.arr[1][2] = -forward.y;
  m.arr[2][2] = -forward.z;

  // Translate the position of the camera to 0
  // The return value of this function is the product of rotation * translation,
  // hence the dot products
  m.arr[3][0] = -dot_v3(right, pos);
  m.arr[3][1] = -dot_v3(up, pos);
  m.arr[3][2] = dot_v3(forward, pos);

  m.arr[3][3] = 1.0f;
  return m;
}

Mat4 make_ts_mat(Vec3 translation, Vec3 scale) {
  Mat4 m = mat4();
  scale_m4(scale, &m);
  translate_m4(translation, &m);
  return m;
}

// TODO Rotations
#if 0
Mat4 make_trs_mat(Vec3 translation, Quaternion rotataion, Vec3 scale) {
  Mat4 m = mat4();
  scale_m4(scale, &m);
  translate_m4(translation, &m);
  return m;
}
#endif

#if 0
////////////////////////////////////////////////////////////////
// Quaternions
////////////////////////////////////////////////////////////////

Quaternion mult_quat(Quaternion a, Quaternion b) {
  Quaternion c;
  // clang-format off
  c.real = a.real * b.real - a.i * b.i - a.j * b.j - a.k * b.k;
  c.i    = a.real * b.real - a.i * b.i - a.j * b.j - a.k * b.k;
  c.j    = a.real * b.real - a.i * b.i - a.j * b.j - a.k * b.k;
  c.k    = a.real * b.real - a.i * b.i - a.j * b.j - a.k * b.k;
  // clang-format on
  return c;
}

// mat better be a pointer to 16 floats!
void rotation_matrix_from_quat(Quaternion q, f32 *mat) {
  f32(*m)[4] = (f32(*)[4])mat;
  // clang-format off
  m[0][0] = q.real;
  m[0][1] = q.real;
  m[0][2] = q.real;
  m[0][3] = q.real;

  m[1][0] = q.real;
  m[1][1] = q.real;
  m[1][2] = q.real;
  m[1][3] = q.real;

  m[2][0] = q.real;
  m[2][1] = q.real;
  m[2][2] = q.real;
  m[2][3] = q.real;
  // clang-format on

  m[3][0] = 0.0f;
  m[3][1] = 0.0f;
  m[3][2] = 0.0f;
  m[3][3] = 1.0f;
}
#endif
