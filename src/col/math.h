#ifndef COL_MATH_H
#define COL_MATH_H
//..............................

#include <math.h>
#include "./types.h"

//..............................
// General math
#define Sqr(x) ((x) * (x))
f32 f32Sqr(f32 number);

//..............................
// Generic Vec3 functions. Can be applied to f32 and f64. Port from id-Tech3
#define GVec3Dot(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define GVec3Sub(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define GVec3Add(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define GVec3Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define GVec3Scale(v, s, o) ((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
#define GVec3MA(v, s, b, o) ((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))
#define GVec3Clear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define GVec3Neg(a, b) ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define GVec3Set(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define GVec4Set(v, x, y, z, w) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z), v[3] = (w))
#define GVec4Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])
//..............................
// forced double-precison functions
#define DVec3Dot(x, y) ((f64)(x)[0] * (y)[0] + (f64)(x)[1] * (y)[1] + (f64)(x)[2] * (y)[2])
#define DVec3Sub(a, b, c) ((c)[0] = (f64)((a)[0] - (b)[0]), (c)[1] = (f64)((a)[1] - (b)[1]), (c)[2] = (f64)((a)[2] - (b)[2]))
#define DVec3Add(a, b, c) ((c)[0] = (f64)((a)[0] + (b)[0]), (c)[1] = (f64)((a)[1] + (b)[1]), (c)[2] = (f64)((a)[2] + (b)[2]))
//..............................
f64  DVec3Dotf(const f32* v1, const f32* v2);
f64  DVec3Norm(vec3 v);
void DVec3Cross(const vec3 v1, const vec3 v2, vec3 cross);

//..............................
// Vec3
f32 Vec3Len(const vec3 v);
f32 Vec3LenSq(const vec3 v);
f32 Vec3Norm(vec3 v);
//..............................
// Matrix and rotations
void RotatePoint(vec3 point, /*const*/ vec3 matrix[3]);
void CreateRotationMatrix(const vec3 angles, vec3 matrix[3]);
void TransposeMatrix(/*const*/ vec3 matrix[3], vec3 transpose[3]);
void AngleVectors(const vec3 angles, vec3 forward, vec3 right, vec3 up);
//..............................
// Geometry
f32  CM_DistanceFromLineSquared(const vec3 p, const vec3 lp1, const vec3 lp2, const vec3 dir);
void SetPlaneSignbits(cPlane* out);
f32  RadiusFromBounds(const vec3 mins, const vec3 maxs);
void ClearBounds(vec3 mins, vec3 maxs);
void AddPointToBounds(const vec3 v, vec3 mins, vec3 maxs);

//..............................
#endif  // COL_MATH_H
