#ifndef BASE_TYPES_H
#define BASE_TYPES_H
//..................

// stdlib dependencies
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>   // for time_t
#include <stdio.h>  // for off_t

//.........................
typedef uint8_t       u8;
typedef uint32_t      u32;
typedef uint64_t      u64;
typedef int32_t       i32;
typedef float         f32;
typedef double        f64;
typedef unsigned char byte;
//.........................
typedef const float f32k;
typedef const u32   u32k;
//.........................
typedef union floatint_u {
  i32   i;
  u32   u;
  float f;
  byte  b[4];
} f32i;

//..................
#endif  // BASE_TYPES_H
