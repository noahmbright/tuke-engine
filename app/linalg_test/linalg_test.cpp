#include "linalg.h"
#include <stdio.h>
#include <string.h>

int main() {
  Mat4 l, r, out;
  memset(out.arr, 0, sizeof(out.arr));

  f32 dl[4][4] = {{1, 2, 6, 4}, {0, 5, -3, 8}, {3, -7, 1, -3}, {5, 6, 0, 7}};
  f32 dr[4][4] = {{12, -1, 3, 1}, {6, -1, -8, 11}, {-5, 0, 9, -4}, {6, -5, -9, 6}};
  memcpy(l.arr, dl, sizeof(l.arr));
  memcpy(r.arr, dr, sizeof(r.arr));

  mult_m4(&l, &r, &out);
  log_m4(&out);

  f32 dl2[4][4] = {{1, 7, -2, 0}, {3, 4, 1, 0}, {0, 1, 9, 0}, {0, 0, 0, 1}};
  Vec4 v = vec4(-2, 1, 9, 8);
  memcpy(l.arr, dl2, sizeof(l.arr));

  Vec4 u = mvm4(&l, v);
  log_v4(u);

  return 0;
}
