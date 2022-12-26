#ifndef COL_TYPES_H
#define COL_TYPES_H
//....................................

// Depend on linmath and rad types, instead of the og
#include "../types.h"
// Local configuration defines
#include "./cfg.h"

//....................................
// Environment variables for the collision module
// Replacement for CVars
//....................................
typedef struct dbg_s {
  int surfUpdate;  // was: r_debugSurfaceUpdate "1"
  int surf;        // Value to pass to BotDrawDebugPolygons. was: r_debugSurface "0".
  int size;        // clipMap debug size. was: cm_debugSize "2".
} ColDbg;
//....................................
typedef struct colCfg_s {
  int    doVIS;             // All vis clipArea portals will be connected when disabled
  int    doPatchCol;        // Patches will be ignored for collision traces when disabled
  int    doPlayerCurveCol;  // PlayerToCurve collsion will be ignored when disabled. was: cm_playerCurveClip
  ColDbg dbg;
} ColCfg;
typedef struct loadCfg_s {
  int noCurves;   // Won't load any patches when active
  int developer;  // was: Com_DPrintf, instead of a conditional call to echo
} LoadCfg;
//....................................


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

typedef struct {
  cPlane* plane;
  i32     children[2];  // negative numbers are leafs
} cNode;

typedef struct {
  char shader[MAX_PATHLEN];
  i32  surfaceFlags;
  i32  contentFlags;
} dShader;

typedef struct {
  cPlane* plane;
  i32     surfaceFlags;
  i32     shaderNum;
} cBSide;

typedef struct {
  i32 cluster;
  i32 area;
  i32 firstLeafBrush;
  i32 numLeafBrushes;
  i32 firstLeafSurface;
  i32 numLeafSurfaces;
} cLeaf;

typedef struct cmodel_s {
  vec3  mins, maxs;
  cLeaf leaf;  // submodels don't reference the main tree
} cModel;

typedef struct {
  i32     shaderNum;  // the shader that determined the contents
  i32     contents;
  vec3    bounds[2];
  i32     numsides;
  cBSide* sides;
  i32     checkcount;  // to avoid repeated testings
} cBrush;

typedef struct {
  i32 floodNum;
  i32 floodValid;
} cArea;

typedef struct {
  i32                    checkcount;  // to avoid repeated testings
  i32                    surfaceFlags;
  i32                    contents;
  struct patchCollide_s* pc;
} cPatch;

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


//....................................
// Collision solver types
typedef i32 cHandle;
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
// Used for oriented capsule collision detection
typedef struct {
  bool use;
  f32  radius;
  f32  halfHeight;
  vec3 offset;
} Sphere;
//....................................
typedef struct {
  vec3   start;
  vec3   end;
  vec3   size[2];      // size of the box being swept through the model
  vec3   offsets[8];   // [signbits][x] = either size[0][x] or size[1][x]
  f32    maxOffset;    // longest corner length from origin
  vec3   extents;      // greatest of abs(size[0]) and abs(size[1])
  vec3   bounds[2];    // enclosing box of start and end surrounding by size
  vec3   modelOrigin;  // origin of the model tracing through
  i32    contents;     // ORed contents of the model tracing through
  bool   isPoint;      // optimized case
  Trace  trace;        // returned from trace call
  Sphere sphere;       // sphere for oriented capsule collision
} TraceWork;

//....................................
// Patch Types
//....................................
typedef struct {
  f32 plane[4];
  i32 signbits;  // signx + (signy<<1) + (signz<<2), used as lookup during collision
} PatchPlane;
//....................................
typedef struct {
  i32  surfacePlane;
  i32  numBorders;  // 3 or four + 6 axial bevels + 4 or 3 * 4 edge bevels
  i32  borderPlanes[4 + 6 + 16];
  i32  borderInward[4 + 6 + 16];
  bool borderNoAdjust[4 + 6 + 16];
} Facet;
//....................................
// BSP structure that will be used to collide with a patch
typedef struct patchCollide_s {
  vec3        bounds[2];
  i32         numPlanes;  // surface planes plus edge planes
  PatchPlane* planes;
  i32         numFacets;
  Facet*      facets;
} PatchCol;
//....................................
typedef struct {
  i32  width;
  i32  height;
  bool wrapWidth;
  bool wrapHeight;
  vec3 points[MAX_GRID_SIZE][MAX_GRID_SIZE];  // [width][height]
} cGrid;
//....................................
typedef enum { EN_TOP, EN_RIGHT, EN_BOTTOM, EN_LEFT } edgeName_t;
//....................................


//....................................
#endif  // COL_TYPES_H
