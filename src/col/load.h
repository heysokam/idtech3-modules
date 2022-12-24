#ifndef COL_LOAD_H
#define COL_LOAD_H
//..............................

// stdlib dependencies
#include <stdio.h>
#include <string.h>
// Engine dependencies
#include "../mem/core.h"
#include "../tools/core.h"
#include "../files/md4.h"
#include "../files/io.h"
// Collision module dependencies
#include "./types.h"
#include "./state.h"
#include "./vis.h"

//..............................
#define BSP_VERSION 46

//..............................
// to allow boxes to be treated as brush models,
// some extra indexes are allocated along with those needed by the map
#define BOX_BRUSHES 1
#define BOX_SIDES 6
#define BOX_LEAFS 2
#define BOX_PLANES 12

//..............................
// plane types are used to speed some tests
// 0-2 are axial planes
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2
#define PLANE_NON_AXIAL 3
//..............................
#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)))

//..............................
typedef struct {
  i32 fileofs;
  i32 filelen;
} Lump;
//..............................
#define LUMP_ENTITIES 0
#define LUMP_SHADERS 1
#define LUMP_PLANES 2
#define LUMP_NODES 3
#define LUMP_LEAFS 4
#define LUMP_LEAFSURFACES 5
#define LUMP_LEAFBRUSHES 6
#define LUMP_MODELS 7
#define LUMP_BRUSHES 8
#define LUMP_BSideS 9
#define LUMP_DRAWVERTS 10
#define LUMP_DRAWINDEXES 11
#define LUMP_FOGS 12
#define LUMP_SURFACES 13
#define LUMP_LIGHTMAPS 14
#define LUMP_LIGHTGRID 15
#define LUMP_VISIBILITY 16
#define LUMP_COUNT 17
//..............................

//..............................
// Drawing Types: General
//..............................
typedef union {
  byte rgba[4];
  u32  u32;
} color4ub_t;

//..............................
// Drawing Types: BSP
//..............................
typedef struct {
  i32  ident;
  i32  version;
  Lump lumps[LUMP_COUNT];
} dHeader;
//..............................
typedef struct {
  f32 mins[3], maxs[3];
  i32 firstSurface;
  i32 numSurfaces;
  i32 firstBrush;
  i32 numBrushes;
} dModel;
//..............................
// planes x^1 is allways the opposite of plane x
typedef struct {
  f32 normal[3];
  f32 dist;
} dPlane;
//..............................
typedef struct {
  i32 planeNum;
  i32 children[2];  // negative numbers are -(leafs+1), not nodes
  i32 mins[3];      // for frustom culling
  i32 maxs[3];
} dNode;
//..............................
typedef struct {
  i32 cluster;  // -1 = opaque cluster (do I still store these?)
  i32 area;
  i32 mins[3];  // for frustum culling
  i32 maxs[3];
  i32 firstLeafSurface;
  i32 numLeafSurfaces;
  i32 firstLeafBrush;
  i32 numLeafBrushes;
} dLeaf;
//..............................
typedef struct {
  i32 planeNum;  // positive plane side faces out of the leaf
  i32 shaderNum;
} dBSide_t;
//..............................
typedef struct {
  i32 firstSide;
  i32 numSides;
  i32 shaderNum;  // the shader that determines the contents flags
} dBrush;
//..............................
typedef struct {
  char shader[MAX_PATHLEN];
  i32  brushNum;
  i32  visibleSide;  // the brush side that ray tests need to clip against (-1 == none)
} dFog;
//..............................
typedef struct {
  vec3       xyz;
  f32        st[2];
  f32        lightmap[2];
  vec3       normal;
  color4ub_t color;
} dVert;
//..............................
typedef enum { MST_BAD, MST_PLANAR, MST_PATCH, MST_TRIANGLE_SOUP, MST_FLARE } mapSurfaceType_t;
//....................................
typedef struct {
  i32  shaderNum;
  i32  fogNum;
  i32  surfaceType;
  i32  firstVert;
  i32  numVerts;
  i32  firstIndex;
  i32  numIndexes;
  i32  lightmapNum;
  i32  lightmapX;
  i32  lightmapY;
  i32  lightmapWidth;
  i32  lightmapHeight;
  vec3 lightmapOrigin;
  vec3 lightmapVecs[3];  // for patches, [0] and [1] are lodbounds
  i32  patchWidth;
  i32  patchHeight;
} dSurf;
//....................................

//..............................
// BSP Loading utility
// Will alloc and store its data in state.c static variables
void CM_LoadMap(const char* name, bool clientload, int* checksum);
void CM_ClearMap(void);

//..............................
#endif  // COL_LOAD_H
