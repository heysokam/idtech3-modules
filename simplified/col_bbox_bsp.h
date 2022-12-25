// stdlib dependencies
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>

//......................................................................................................................
// Interface
// .........................
// In -one- source file, add:
//   #define IMP_BoxCollision
//   #include "col_bbox_bsp.h"
//
// Include this header wherever a box-to-bsp trace needs to be requested.
// The only external function is:
//   void CM_BoxTrace(cMap* cm, Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, i32 brushmask, bool doPatchCol);
//
// You are responsible for creating and passing a correct cMap pointer to the function.
// This pointer must contain valid collision data.
// NOTE: Patch collisions are currently disabled, and not included in this file. Passing true to doPathCol currently has no effect.
//......................................................................................................................


//......................................................................................................................
// cfg.h
#define MAX_PATHLEN 64  // the maximum size of game relative pathnames
//..................
// General
#define SURFACE_CLIP_EPSILON (0.125)  // keep 1/8 unit away to keep the position valid before network snapping and avoid various numeric issues
#define MAX_POSITION_LEAFS 1024
//..................
// AABB
#define BOUNDS_CLIP_EPSILON 0.25f  // assume single precision and slightly increase to compensate potential SIMD precison loss in 64-bit environment
//......................................................................................................................


//......................................................................................................................
// types/base.h
typedef float         f32;
typedef double        f64;
typedef int32_t       i32;
typedef uint32_t      u32;
typedef unsigned char byte;
typedef f32           vec3[3];
//..................
const vec3 vec3_origin = { 0, 0, 0 };
//......................................................................................................................


//......................................................................................................................
// types/solve.h
//....................................
// BSP data types  (clipType = cType : drawType = dType)
//....................................
typedef struct cplane_s {
  vec3 normal;
  f32  dist;
  byte type;      // for fast side tests: 0,1,2 = axial, 3 = nonaxial
  byte signbits;  // signx + (signy<<1) + (signz<<2), used as lookup during collision
  byte pad[2];
} cPlane;
//..................
typedef struct {
  cPlane* plane;
  i32     children[2];  // negative numbers are leafs
} cNode;
//..................
typedef struct {
  char shader[MAX_PATHLEN];
  i32  surfaceFlags;
  i32  contentFlags;
} dShader;
//..................
typedef struct {
  cPlane* plane;
  i32     surfaceFlags;
  i32     shaderNum;
} cBSide;
//..................
typedef struct {
  i32 cluster;
  i32 area;
  i32 firstLeafBrush;
  i32 numLeafBrushes;
  i32 firstLeafSurface;
  i32 numLeafSurfaces;
} cLeaf;
//..................
typedef struct cmodel_s {
  vec3  mins, maxs;
  cLeaf leaf;  // submodels don't reference the main tree
} cModel;
//..................
typedef struct {
  i32     shaderNum;  // the shader that determined the contents
  i32     contents;
  vec3    bounds[2];
  i32     numsides;
  cBSide* sides;
  i32     checkcount;  // to avoid repeated testings
} cBrush;
//..................
typedef struct {
  i32 floodNum;
  i32 floodValid;
} cArea;
//..................
typedef struct {
  i32                    checkcount;  // to avoid repeated testings
  i32                    surfaceFlags;
  i32                    contents;
  struct patchCollide_s* pc;
} cPatch;
//..................
typedef struct {
  char     name[MAX_PATHLEN];
  i32      numShaders;
  dShader* shaders;
  i32      numBSides;
  cBSide*  BSides;
  i32      numPlanes;
  cPlane*  planes;
  i32      numNodes;
  cNode*   nodes;
  i32      numLeafs;
  cLeaf*   leafs;
  i32      numLeafBrushes;
  i32*     leafbrushes;
  i32      numLeafSurfaces;
  i32*     leafsurfaces;
  i32      numSubModels;
  cModel*  cmodels;
  i32      numBrushes;
  cBrush*  brushes;
  i32      numClusters;
  i32      clusterBytes;
  byte*    visibility;
  bool     vised;  // if false, visibility is just a single cluster of ffs
  i32      numEntityChars;
  char*    entityString;
  i32      numAreas;
  cArea*   areas;
  i32*     areaPortals;  // [ numAreas*numAreas ] reference counts
  i32      numSurfaces;
  cPatch** surfaces;  // non-patches will be NULL
  i32      floodValid;
  i32      checkcount;  // incremented on each trace
  u32      checksum;
} cMap;
//....................................
// a trace is returned when a box is swept through the world
typedef struct {
  bool   allsolid;      // if true, plane is not valid
  bool   startsolid;    // if true, the initial point was in a solid area
  float  fraction;      // time completed, 1.0 = didn't hit anything
  vec3   endpos;        // final position
  cPlane plane;         // surface normal at impact, transformed to world space
  i32    surfaceFlags;  // surface hit
  i32    contents;      // contents on other side of surface hit
  i32    entityNum;     // entity the contacted surface is a part of
} Trace;
// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD
//....................................
typedef struct {
  vec3  start;
  vec3  end;
  vec3  size[2];      // size of the box being swept through the model
  vec3  offsets[8];   // [signbits][x] = either size[0][x] or size[1][x]
  f32   maxOffset;    // longest corner length from origin
  vec3  extents;      // greatest of abs(size[0]) and abs(size[1])
  vec3  bounds[2];    // enclosing box of start and end surrounding by size
  vec3  modelOrigin;  // origin of the model tracing through
  i32   contents;     // ORed contents of the model tracing through
  bool  isPoint;      // optimized case
  Trace trace;        // returned from trace call
  // Sphere sphere;       // sphere for oriented capsule collision
} TraceWork;
//....................................
typedef struct leafList_s {
  i32  count;
  i32  maxcount;
  bool overflowed;
  i32* list;
  vec3 bounds[2];
  i32  lastLeaf;  // for overflows where each leaf can't be stored individually
  void (*storeLeafs)(cMap* cm, struct leafList_s* ll, i32 nodeNum);
} LeafList;
//......................................................................................................................


//......................................................................................................................
// Interface
void CM_BoxTrace(cMap* cm, Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, i32 brushmask, bool doPatchCol);
// .........................
#if defined IMP_BoxCollision
//......................................................................................................................


//......................................................................................................................
// math.h
//..............................
// Generic Vec3 functions. Can be applied to f32 and f64. Port from id-Tech3
#  define GVec3Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#  define GVec3Clear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#  define GVec3Add(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#  define GVec3Dot(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
// Vec3 functions
f32 Vec3LenSq(const vec3 v);
// forced double-precison functions
#  define DVec3Dot(x, y) ((f64)(x)[0] * (y)[0] + (f64)(x)[1] * (y)[1] + (f64)(x)[2] * (y)[2])
//......................................................................................................................


//......................................................................................................................
// math.c
inline f32 Vec3LenSq(const vec3 v) { return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
//......................................................................................................................


//......................................................................................................................
// position.c
//..................
// CM_TestBoxInBrush
//..................
static void CM_TestBoxInBrush(TraceWork* tw, const cBrush* brush) {
  if (!brush->numsides) { return; }
  // special test for axial
  if (tw->bounds[0][0] > brush->bounds[1][0] || tw->bounds[0][1] > brush->bounds[1][1] || tw->bounds[0][2] > brush->bounds[1][2]
      || tw->bounds[1][0] < brush->bounds[0][0] || tw->bounds[1][1] < brush->bounds[0][1] || tw->bounds[1][2] < brush->bounds[0][2]) {
    return;
  }
  cPlane* plane;
  f64     dist;
  f64     d1;
  cBSide* side;
  // sk.rmv -> sphere code
  //
  // the first six planes are the axial planes,
  // so we only need to test the remainder
  for (i32 i = 6; i < brush->numsides; i++) {
    side  = brush->sides + i;
    plane = side->plane;
    // adjust the plane distance appropriately for mins/maxs
    dist  = plane->dist - GVec3Dot(tw->offsets[plane->signbits], plane->normal);
    d1    = DVec3Dot(tw->start, plane->normal) - dist;
    // if completely in front of face, no intersection
    if (d1 > 0) { return; }
  }

  // inside this brush
  tw->trace.startsolid = true;
  tw->trace.allsolid   = true;
  tw->trace.fraction   = 0;
  tw->trace.contents   = brush->contents;
}


//..................
// CM_TestInLeaf
//..................
static void CM_TestInLeaf(cMap* cm, TraceWork* tw, const cLeaf* leaf, bool doPatchCol) {
  // test box position against all brushes in the leaf
  cBrush* b;
  for (i32 k = 0; k < leaf->numLeafBrushes; k++) {
    i32 brushnum = cm->leafbrushes[leaf->firstLeafBrush + k];
    b            = &cm->brushes[brushnum];
    if (b->checkcount == cm->checkcount) { continue; }  // already checked this brush in another leaf
    b->checkcount = cm->checkcount;
    if (!(b->contents & tw->contents)) { continue; }
    CM_TestBoxInBrush(tw, b);
    if (tw->trace.allsolid) { return; }
  }

  // test against all patches
  cPatch* patch;
  if (doPatchCol) {
#  if 0  // TODO: Patch collision
    for (i32 k = 0; k < leaf->numLeafSurfaces; k++) {
      patch = cm->surfaces[cm->leafsurfaces[leaf->firstLeafSurface + k]];
      if (!patch) { continue; }
      if (patch->checkcount == cm->checkcount) {
        continue;  // already checked this brush in another leaf
      }
      patch->checkcount = cm->checkcount;

      if (!(patch->contents & tw->contents)) { continue; }

      if (CM_PositionTestInPatchCollide(tw, patch->pc)) {
        tw->trace.startsolid = tw->trace.allsolid = true;
        tw->trace.fraction                        = 0;
        tw->trace.contents                        = patch->contents;
        return;
      }
    }
#  endif
  }
}

//..................
// CM_StoreLeafs
//   Stores the leaves of the given LeafList into the given clipMap data
//..................
static void CM_StoreLeafs(cMap* cm, LeafList* ll, i32 nodeNum) {
  i32 leafNum = -1 - nodeNum;

  // store the lastLeaf even if the list is overflowed
  if (cm->leafs[leafNum].cluster != -1) { ll->lastLeaf = leafNum; }

  if (ll->count >= ll->maxcount) {
    ll->overflowed = true;
    return;
  }
  ll->list[ll->count++] = leafNum;
}

//..............................
// BoxOnPlaneSide
//   Checks on which plane side the AABB is standing
//   Returns:  1, 2, or 1 + 2
//   Guessing this means:
//     1 = 01b  : In front
//     2 = 10b  : Behind
//     3 = 11b  : Straddling
//     dist[0] appears to be the max corner point
//     dist[1] is the min corner point.
//     Selects which one is 0 and which one is 1
//       assuming the normal is stored as all positive numbers,
//       with single bits to represent the sign
//..............................
static i32 BoxOnPlaneSide(const vec3 emins, const vec3 emaxs, const cPlane* p) {
  // fast axial cases
  if (p->type < 3) {
    if (p->dist <= emins[p->type]) return 1;
    if (p->dist >= emaxs[p->type]) return 2;
    return 3;
  }
  // general case
  f32 dist[2];
  dist[0] = dist[1] = 0;
  if (p->signbits < 8) {  // >= 8: default case is original code (dist[0]=dist[1]=0)
    i32 b;
    for (i32 i = 0; i < 3; i++) {
      b = (p->signbits >> i) & 1;
      dist[b] += p->normal[i] * emaxs[i];
      dist[!b] += p->normal[i] * emins[i];
    }
  }

  i32 sides = 0;
  if (dist[0] >= p->dist) sides = 1;
  if (dist[1] < p->dist) sides |= 2;

  return sides;
}


//..................
// CM_BoxLeafnums_r
//   Stores all the leafs touched by the given LeafList, recursively
//..................
static void CM_BoxLeafnums_r(cMap* cm, LeafList* ll, i32 nodeNum) {
  cPlane* plane;
  cNode*  node;
  i32     s;
  while (1) {
    if (nodeNum < 0) {                  // Negative numbers are leaves
      ll->storeLeafs(cm, ll, nodeNum);  // Store the current leaf
      return;
    }
    node  = &cm->nodes[nodeNum];
    plane = node->plane;
    s     = BoxOnPlaneSide(ll->bounds[0], ll->bounds[1], plane);
    if (s == 1) {
      nodeNum = node->children[0];
    } else if (s == 2) {
      nodeNum = node->children[1];
    } else {
      // go down both
      CM_BoxLeafnums_r(cm, ll, node->children[0]);
      nodeNum = node->children[1];
    }
  }
}

//..................
// CM_PositionTest
//..................
static void CM_PositionTest(cMap* cm, TraceWork* tw, bool doPatchCol) {
  LeafList ll;
  // identify the leafs we are touching
  GVec3Add(tw->start, tw->size[0], ll.bounds[0]);
  GVec3Add(tw->start, tw->size[1], ll.bounds[1]);

  for (i32 i = 0; i < 3; i++) {
    ll.bounds[0][i] -= 1;
    ll.bounds[1][i] += 1;
  }

  i32 leafs[MAX_POSITION_LEAFS];
  ll.list       = leafs;
  ll.count      = 0;
  ll.maxcount   = MAX_POSITION_LEAFS;
  ll.storeLeafs = CM_StoreLeafs;
  ll.lastLeaf   = 0;
  ll.overflowed = false;

  cm->checkcount++;

  CM_BoxLeafnums_r(cm, &ll, 0);

  cm->checkcount++;

  // test the contents of the leafs
  for (i32 i = 0; i < ll.count; i++) {
    CM_TestInLeaf(cm, tw, &cm->leafs[leafs[i]], doPatchCol);
    if (tw->trace.allsolid) { break; }
  }
}
//......................................................................................................................


//......................................................................................................................
// trace.c
//..................
// CM_BoundsIntersect
//   Checks if the given AABBs intersect with each other
//   aka AABBtoAABB
//..................
static bool CM_BoundsIntersect(const vec3 mins, const vec3 maxs, const vec3 mins2, const vec3 maxs2) {  // clang-format off
  if (   maxs[0] < mins2[0] - BOUNDS_CLIP_EPSILON 
      || maxs[1] < mins2[1] - BOUNDS_CLIP_EPSILON 
      || maxs[2] < mins2[2] - BOUNDS_CLIP_EPSILON
      || mins[0] > maxs2[0] + BOUNDS_CLIP_EPSILON 
      || mins[1] > maxs2[1] + BOUNDS_CLIP_EPSILON 
      || mins[2] > maxs2[2] + BOUNDS_CLIP_EPSILON) {  // clang-format on
    return false;
  }
  return true;
}

//..................
// CM_TraceThroughBrush
//   Checks if the given trace data (TraceWork) passes through any of the given clipBrush planes
//   Increases the c_brush_traces counter
//..................
static void CM_TraceThroughBrush(TraceWork* tw, const cBrush* brush) {
  if (!brush->numsides) { return; }
  // c_brush_traces++;

  bool getout       = false;
  bool startout     = false;

  f32     enterFrac = -1.0;
  f32     leaveFrac = 1.0;
  cPlane* clipplane = NULL;
  cBSide* leadside  = NULL;
  cBSide* side;
  cPlane* plane;
  // sk.rmv -> sphere code
  //
  // compare the trace against all planes of the brush
  // find the latest time the trace crosses a plane towards the interior
  // and the earliest time the trace crosses a plane towards the exterior
  for (i32 sideId = 0; sideId < brush->numsides; sideId++) {
    side     = brush->sides + sideId;
    plane    = side->plane;

    // adjust the plane distance appropriately for mins/maxs
    f32 dist = plane->dist - GVec3Dot(tw->offsets[plane->signbits], plane->normal);
    f32 d1   = GVec3Dot(tw->start, plane->normal) - dist;
    f32 d2   = GVec3Dot(tw->end, plane->normal) - dist;
    if (d2 > 0) { getout = true; }  // endpoint is not in solid
    if (d1 > 0) { startout = true; }
    // if completely in front of face, no intersection with the entire brush
    if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) { return; }
    // if it doesn't cross the plane, the plane isn't relevant
    if (d1 <= 0 && d2 <= 0) { continue; }
    // crosses face
    if (d1 > d2) {  // enter
      f32 f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
      if (f < 0) { f = 0; }
      if (f > enterFrac) {
        enterFrac = f;
        clipplane = plane;
        leadside  = side;
      }
    } else {  // leave
      f32 f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
      if (f > 1) { f = 1; }
      if (f < leaveFrac) { leaveFrac = f; }
    }
  }
  // all planes have been checked, and the trace was not completely outside the brush
  if (!startout) {  // original point was inside brush
    tw->trace.startsolid = true;
    if (!getout) {
      tw->trace.allsolid = true;
      tw->trace.fraction = 0;
      tw->trace.contents = brush->contents;
    }
    return;
  }
  if (enterFrac < leaveFrac) {
    if (enterFrac > -1 && enterFrac < tw->trace.fraction) {
      if (enterFrac < 0) { enterFrac = 0; }
      tw->trace.fraction = enterFrac;
      if (clipplane != NULL) { tw->trace.plane = *clipplane; }
      if (leadside != NULL) { tw->trace.surfaceFlags = leadside->surfaceFlags; }
      tw->trace.contents = brush->contents;
    }
  }
}


//..................
// CM_TraceThroughLeaf
//   Checks if the given trace data (TraceWork)
//   passes through any of the given clipLeaf brushes or patches
//..................
static void CM_TraceThroughLeaf(cMap* cm, TraceWork* tw, const cLeaf* leaf, bool doPatchCol) {
  // trace line against all brushes in the leaf
  for (i32 leafBrushId = 0; leafBrushId < leaf->numLeafBrushes; leafBrushId++) {
    i32     brushnum = cm->leafbrushes[leaf->firstLeafBrush + leafBrushId];
    cBrush* b        = &cm->brushes[brushnum];
    if (b->checkcount == cm->checkcount) { continue; }  // already checked this brush in another leaf
    b->checkcount = cm->checkcount;
    if (!(b->contents & tw->contents)) { continue; }
    if (!CM_BoundsIntersect(tw->bounds[0], tw->bounds[1], b->bounds[0], b->bounds[1])) { continue; }
    CM_TraceThroughBrush(tw, b);
    if (!tw->trace.fraction) { return; }
  }

  // trace line against all patches in the leaf
  if (doPatchCol) {
#  if 0  // TODO: Patch collision
    for (i32 leafSurfId = 0; leafSurfId < leaf->numLeafSurfaces; leafSurfId++) {
      cPatch* patch = cm->surfaces[cm->leafsurfaces[leaf->firstLeafSurface + leafSurfId]];
      if (!patch) { continue; }
      if (patch->checkcount == cm->checkcount) {
        continue;  // already checked this patch in another leaf
      }
      patch->checkcount = cm->checkcount;
      if (!(patch->contents & tw->contents)) { continue; }
      CM_TraceThroughPatch(tw, patch);
      if (!tw->trace.fraction) { return; }
    }
#  endif
  }
}

//..................
// CM_TraceThroughTree :
//   Traverse all the contacted leafs from the start to the end position.
//   If the trace is a point, they will be exactly in order,
//   but for larger trace volumes it is possible to hit something in a later leaf
//   with a smaller intercept fraction.
//..................
static void CM_TraceThroughTree(cMap* cm, TraceWork* tw, i32 num, f32 p1f, f32 p2f, const vec3 p1, const vec3 p2, bool doPatchCol) {
  if (tw->trace.fraction <= p1f) { return; }  // already hit something nearer
  // if < 0, we are in a leaf node
  if (num < 0) {
    CM_TraceThroughLeaf(cm, tw, &cm->leafs[-1 - num], doPatchCol);
    return;
  }
  // find the point distances to the separating plane
  // and the offset for the size of the box
  cNode*  node  = cm->nodes + num;
  cPlane* plane = node->plane;
  // adjust the plane distance appropriately for mins/maxs
  f64 t1, t2, offset;
  if (plane->type < 3) {
    t1     = p1[plane->type] - plane->dist;
    t2     = p2[plane->type] - plane->dist;
    offset = tw->extents[plane->type];
  } else {
    t1 = GVec3Dot(plane->normal, p1) - plane->dist;
    t2 = GVec3Dot(plane->normal, p2) - plane->dist;
    if (tw->isPoint) {
      offset = 0;
    } else {
      // this is silly
      offset = 2048;
    }
  }
  // see which sides we need to consider
  if (t1 >= offset + 1 && t2 >= offset + 1) {
    CM_TraceThroughTree(cm, tw, node->children[0], p1f, p2f, p1, p2, doPatchCol);
    return;
  }
  if (t1 < -offset - 1 && t2 < -offset - 1) {
    CM_TraceThroughTree(cm, tw, node->children[1], p1f, p2f, p1, p2, doPatchCol);
    return;
  }
  // put the crosspoint SURFACE_CLIP_EPSILON pixels on the near side
  f32 idist;
  i32 side;
  f32 frac, frac2;
  if (t1 < t2) {
    idist = 1.0 / (t1 - t2);
    side  = 1;
    frac2 = (t1 + offset + SURFACE_CLIP_EPSILON) * idist;
    frac  = (t1 - offset + SURFACE_CLIP_EPSILON) * idist;
  } else if (t1 > t2) {
    idist = 1.0 / (t1 - t2);
    side  = 0;
    frac2 = (t1 - offset - SURFACE_CLIP_EPSILON) * idist;
    frac  = (t1 + offset + SURFACE_CLIP_EPSILON) * idist;
  } else {
    side  = 0;
    frac  = 1;
    frac2 = 0;
  }
  // move up to the node
  if (frac < 0) {
    frac = 0;
  } else if (frac > 1) {
    frac = 1;
  }
  f32 midf = p1f + (p2f - p1f) * frac;

  vec3 mid;
  mid[0] = p1[0] + frac * (p2[0] - p1[0]);
  mid[1] = p1[1] + frac * (p2[1] - p1[1]);
  mid[2] = p1[2] + frac * (p2[2] - p1[2]);

  CM_TraceThroughTree(cm, tw, node->children[side], p1f, midf, p1, mid, doPatchCol);

  // go past the node
  if (frac2 < 0) {
    frac2 = 0;
  } else if (frac2 > 1) {
    frac2 = 1;
  }

  midf   = p1f + (p2f - p1f) * frac2;

  mid[0] = p1[0] + frac2 * (p2[0] - p1[0]);
  mid[1] = p1[1] + frac2 * (p2[1] - p1[1]);
  mid[2] = p1[2] + frac2 * (p2[2] - p1[2]);

  CM_TraceThroughTree(cm, tw, node->children[side ^ 1], midf, p2f, mid, p2, doPatchCol);
}


//......................................................................................................................
// solve.c
//..................
// CM_Trace
//..................
static void CM_Trace(cMap* cm, Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, const vec3 origin, i32 brushmask,
                     bool doPatchCol) {
  // sk.rmv -> cmod, for when passing a clipModel handle. Will always use the trace aabb instead
  cm->checkcount++;  // for multi-check avoidance
  // c_traces++;        // for statistics, may be zeroed

  // fill in a default trace
  TraceWork tw;
  memset(&tw, 0, sizeof(tw));
  tw.trace.fraction = 1;  // assume it goes the entire distance until shown otherwise
  GVec3Copy(origin, tw.modelOrigin);

  if (!cm->numNodes) {  // map not loaded, shouldn't happen
    *results = tw.trace;
    return;
  }

  // allow NULL to be passed in for 0,0,0
  if (!mins) { mins = vec3_origin; }
  if (!maxs) { maxs = vec3_origin; }

  // set basic parms
  tw.contents = brushmask;

  // adjust so that mins and maxs are always symetric
  // avoids some complications with plane expanding of rotated bmodels
  vec3 offset;
  for (i32 i = 0; i < 3; i++) {
    offset[i]     = (mins[i] + maxs[i]) * 0.5;
    tw.size[0][i] = mins[i] - offset[i];
    tw.size[1][i] = maxs[i] - offset[i];
    tw.start[i]   = start[i] + offset[i];
    tw.end[i]     = end[i] + offset[i];
  }

  // sk.rmv -> sphere code
  //

  tw.maxOffset     = tw.size[1][0] + tw.size[1][1] + tw.size[1][2];
  // tw.offsets[signbits] = vector to appropriate corner from origin
  tw.offsets[0][0] = tw.size[0][0];
  tw.offsets[0][1] = tw.size[0][1];
  tw.offsets[0][2] = tw.size[0][2];

  tw.offsets[1][0] = tw.size[1][0];
  tw.offsets[1][1] = tw.size[0][1];
  tw.offsets[1][2] = tw.size[0][2];

  tw.offsets[2][0] = tw.size[0][0];
  tw.offsets[2][1] = tw.size[1][1];
  tw.offsets[2][2] = tw.size[0][2];

  tw.offsets[3][0] = tw.size[1][0];
  tw.offsets[3][1] = tw.size[1][1];
  tw.offsets[3][2] = tw.size[0][2];

  tw.offsets[4][0] = tw.size[0][0];
  tw.offsets[4][1] = tw.size[0][1];
  tw.offsets[4][2] = tw.size[1][2];

  tw.offsets[5][0] = tw.size[1][0];
  tw.offsets[5][1] = tw.size[0][1];
  tw.offsets[5][2] = tw.size[1][2];

  tw.offsets[6][0] = tw.size[0][0];
  tw.offsets[6][1] = tw.size[1][1];
  tw.offsets[6][2] = tw.size[1][2];

  tw.offsets[7][0] = tw.size[1][0];
  tw.offsets[7][1] = tw.size[1][1];
  tw.offsets[7][2] = tw.size[1][2];

  //
  // calculate bounds
  //

  // sk.rmv -> sphere code
  //
  for (i32 i = 0; i < 3; i++) {
    if (tw.start[i] < tw.end[i]) {
      tw.bounds[0][i] = tw.start[i] + tw.size[0][i];
      tw.bounds[1][i] = tw.end[i] + tw.size[1][i];
    } else {
      tw.bounds[0][i] = tw.end[i] + tw.size[0][i];
      tw.bounds[1][i] = tw.start[i] + tw.size[1][i];
    }
  }

  // check for position test special case
  if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
    // sk.rmv -> sphere and capsule code
    // sk.rmv -> cmod code path
    //
    CM_PositionTest(cm, &tw, doPatchCol);
  } else {
    // check for point special case
    if (tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0) {
      tw.isPoint = true;
      GVec3Clear(tw.extents);
    } else {
      tw.isPoint    = false;
      tw.extents[0] = tw.size[1][0];
      tw.extents[1] = tw.size[1][1];
      tw.extents[2] = tw.size[1][2];
    }
    // general sweeping through world
    // sk.rmv -> sphere and capsule code
    // sk.rmv -> cmod code path
    //
    CM_TraceThroughTree(cm, &tw, 0, 0, 1, tw.start, tw.end, doPatchCol);
  }

  // generate endpos from the original, unmodified start/end
  if (tw.trace.fraction == 1) {
    GVec3Copy(end, tw.trace.endpos);
  } else {
    for (i32 i = 0; i < 3; i++) { tw.trace.endpos[i] = start[i] + tw.trace.fraction * (end[i] - start[i]); }
  }

  // If allsolid is set (was entirely inside something solid), the plane is not valid.
  // If fraction == 1.0, we never hit anything, and thus the plane is not valid.
  // Otherwise, the normal on the plane should have unit length
  assert(tw.trace.allsolid || tw.trace.fraction == 1.0 || Vec3LenSq(tw.trace.plane.normal) > 0.9999);
  *results = tw.trace;
}


//..................
// CM_BoxTrace
//..................
void CM_BoxTrace(cMap* cm, Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, i32 brushmask, bool doPatchCol) {
  CM_Trace(cm, results, start, end, mins, maxs, vec3_origin, brushmask, doPatchCol);
}


//......................................................................................................................
#endif  // IMP_BoxCollision
