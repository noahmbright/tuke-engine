#pragma once

#include <stdint.h>

#define MEMBER_SIZE(type, member) (sizeof(((type *)0)->member))
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof(arr[0]))

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

const f32 PI = 3.14159265358979323846264338327950288;
const f32 PI_OVER_2 = PI / 2.0f;
const f32 EPSILON = 1e-9;
const f32 INFINITY_F32 = 1.0f / 0.0f;
const f64 INFINITY_F64 = 1.0 / 0.0;
