#ifndef COL_DEBUG_H
#define COL_DEBUG_H
//............................

// Engine dependencies
#include "../tools/core.h"
#include "../mem/core.h"
// Module dependencies
#include "./types.h"
#include "./math.h"
#include "./cfg.h"

//............................
// This module is only used for visualization tools in cm_ debug functions (clipMap)
// og.name = polylib
//............................

//..............................
extern ColCfg  col;
extern LoadCfg load;
//..............................
// Patch BSP state (PatchCol)
extern i32        numPlanes;
extern PatchPlane planes[MAX_PATCH_PLANES];
extern i32        numFacets;
extern Facet      facets[MAX_FACETS];
// Debug counters: state.c
extern i32 c_active_windings;
extern i32 c_peak_windings;
extern i32 c_winding_allocs;
extern i32 c_winding_points;
//..............................
// Patch Debugging
extern const PatchCol* debugPatchCollide;
extern const Facet*    debugFacet;
extern bool            debugBlock;
extern vec3            debugBlockPoints[4];
//............................
#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS 3

//............................
typedef struct {
  int  numpoints;
  vec3 p[4];  // variable sized
} Winding;

//............................
bool CM_ValidateFacet(const Facet* facet);
void CM_AddFacetBevels(Facet* facet);

//............................
#endif  // COL_DEBUG_H
