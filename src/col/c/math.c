#include "../math.h"

// vec3	vec3_origin = {0,0,0};  // Currently taken from linmath.h. Same value

//..............................
// SquareRootFloat
f32 f32Sqr(f32 number) {
  f32  x = number * 0.5F;
  f32i t;
  t.f    = number;
  t.i    = 0x5f3759df - (t.i >> 1);
  f32k f = 1.5F;
  f32  y = t.f;
  y      = y * (f - (x * y * y));
  y      = y * (f - (x * y * y));
  return number * y;
}

//..............................
// Vector math
//..............................
static inline void Vec3Inv(vec3 v) {
  v[0] = -v[0];
  v[1] = -v[1];
  v[2] = -v[2];
}
//..............................
static inline void Vec3MA(const vec3 v1, const f32 scalar, const vec3 v2, vec3 out) {
  out[0] = v1[0] + v2[0] * scalar;
  out[1] = v1[1] + v2[1] * scalar;
  out[2] = v1[2] + v2[2] * scalar;
}

//..............................
inline f32 Vec3Len(const vec3 v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
//..............................
inline f32 Vec3LenSq(const vec3 v) { return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
//..............................
inline f32 Vec3Norm(vec3 v) {
  f32 length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  if (length) {
    /* writing it this way allows gcc to recognize that rsqrt can be used */
    f32 ilength = 1 / (f32)sqrt(length);
    /* sqrt(length) = length * (1 / sqrt(length)) */
    length *= ilength;
    v[0] *= ilength;
    v[1] *= ilength;
    v[2] *= ilength;
  }
  return length;
}

//..............................
// Double Precision Vectors
//..............................
inline f64 DVec3Dotf(const f32* v1, const f32* v2) {
  f64 x[3], y[3];
  GVec3Copy(v1, x);
  GVec3Copy(v2, y);
  return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
}
//..............................
inline f64 DVec3Norm(vec3 v) {
  double d[3];
  GVec3Copy(v, d);
  double length = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
  if (length) {
    /* writing it this way allows gcc to recognize that rsqrt can be used */
    double ilength = 1.0 / (double)sqrt(length);
    /* sqrt(length) = length * (1 / sqrt(length)) */
    length *= ilength;
    v[0] = d[0] * ilength;
    v[1] = d[1] * ilength;
    v[2] = d[2] * ilength;
  }
  return length;
}
//..............................
inline void DVec3Cross(const vec3 v1, const vec3 v2, vec3 cross) {
  double d1[3], d2[3];
  GVec3Copy(v1, d1);
  GVec3Copy(v2, d2);
  cross[0] = d1[1] * d2[2] - d1[2] * d2[1];
  cross[1] = d1[2] * d2[0] - d1[0] * d2[2];
  cross[2] = d1[0] * d2[1] - d1[1] * d2[0];
}

//..............................
// Matrices and Rotation math
//..............................
void AngleVectors(const vec3 angles, vec3 forward, vec3 right, vec3 up) {
  f32        angle;
  static f32 sr, sp, sy, cr, cp, cy;
  // static to help MS compiler fp bugs

  angle = angles[YAW] * (M_PI * 2 / 360);
  sy    = sin(angle);
  cy    = cos(angle);
  angle = angles[PITCH] * (M_PI * 2 / 360);
  sp    = sin(angle);
  cp    = cos(angle);
  angle = angles[ROLL] * (M_PI * 2 / 360);
  sr    = sin(angle);
  cr    = cos(angle);

  if (forward) {
    forward[0] = cp * cy;
    forward[1] = cp * sy;
    forward[2] = -sp;
  }
  if (right) {
    right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
    right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
    right[2] = -1 * sr * cp;
  }
  if (up) {
    up[0] = (cr * sp * cy + -sr * -sy);
    up[1] = (cr * sp * sy + -sr * cy);
    up[2] = cr * cp;
  }
}

//..............................
// RotatePoint
//   Rotates the given point, based on the input rotation matrix
//..............................
void RotatePoint(vec3 point, /*const*/ vec3 matrix[3]) {
  vec3 tvec;
  GVec3Copy(point, tvec);
  point[0] = GVec3Dot(matrix[0], tvec);
  point[1] = GVec3Dot(matrix[1], tvec);
  point[2] = GVec3Dot(matrix[2], tvec);
}
//..............................
// CreateRotationMatrix
//..............................
void CreateRotationMatrix(const vec3 angles, vec3 matrix[3]) {
  AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
  Vec3Inv(matrix[1]);
}


//..............................
// TransposeMatrix
//..............................
void TransposeMatrix(/*const*/ vec3 matrix[3], vec3 transpose[3]) {
  for (i32 i = 0; i < 3; i++) {
    for (i32 j = 0; j < 3; j++) { transpose[i][j] = matrix[j][i]; }
  }
}

//..............................
// CM_ProjectPointOntoVector
//..............................
static void CM_ProjectPointOntoVector(const vec3 point, const vec3 vStart, const vec3 vDir, vec3 vProj) {
  vec3 pVec;
  GVec3Sub(point, vStart, pVec);
  // project onto the directional vector for this segment
  Vec3MA(vStart, GVec3Dot(pVec, vDir), vDir, vProj);
}

//..............................
// CM_DistanceFromLineSquared
//..............................
f32 CM_DistanceFromLineSquared(const vec3 p, const vec3 lp1, const vec3 lp2, const vec3 dir) {
  vec3 proj, t;
  CM_ProjectPointOntoVector(p, lp1, dir, proj);
  i32 j;
  for (j = 0; j < 3; j++)
    if ((proj[j] > lp1[j] && proj[j] > lp2[j]) || (proj[j] < lp1[j] && proj[j] < lp2[j])) break;
  if (j < 3) {
    if (fabs(proj[j] - lp1[j]) < fabs(proj[j] - lp2[j])) GVec3Sub(p, lp1, t);
    else GVec3Sub(p, lp2, t);
    return Vec3LenSq(t);
  }
  GVec3Sub(p, proj, t);
  return Vec3LenSq(t);
}

//..............................
// SetPlaneSignbits
//..............................
void SetPlaneSignbits(cPlane* out) {
  // for fast box on planeside test
  i32 bits = 0;
  for (i32 j = 0; j < 3; j++) {
    if (out->normal[j] < 0) { bits |= 1 << j; }
  }
  out->signbits = bits;
}

//..............................
// RadiusFromBounds
//..............................
f32 RadiusFromBounds(const vec3 mins, const vec3 maxs) {
  vec3 corner;
  for (i32 i = 0; i < 3; i++) {
    f32 a     = fabs(mins[i]);
    f32 b     = fabs(maxs[i]);
    corner[i] = a > b ? a : b;
  }
  return Vec3Len(corner);
}

//..............................
// ClearBounds
//..............................
void ClearBounds(vec3 mins, vec3 maxs) {
  mins[0] = mins[1] = mins[2] = 99999;
  maxs[0] = maxs[1] = maxs[2] = -99999;
}

//..............................
// AddPointToBounds
//..............................
void AddPointToBounds(const vec3 v, vec3 mins, vec3 maxs) {
  if (v[0] < mins[0]) { mins[0] = v[0]; }
  if (v[0] > maxs[0]) { maxs[0] = v[0]; }
  if (v[1] < mins[1]) { mins[1] = v[1]; }
  if (v[1] > maxs[1]) { maxs[1] = v[1]; }
  if (v[2] < mins[2]) { mins[2] = v[2]; }
  if (v[2] > maxs[2]) { maxs[2] = v[2]; }
}
