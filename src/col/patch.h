#ifndef COL_PATCH_H
#define COL_PATCH_H
//..............................

#include "../mem/core.h"
#include "../tools/core.h"
#include "./types.h"
#include "./cfg.h"
#include "./math.h"
#include "./debug.h"

//..............................
// Patch Debugging: polylib
//..............................
// Patch Debugging: state.c
extern const PatchCol* debugPatchCollide;
extern const Facet*    debugFacet;
extern bool            debugBlock;
extern vec3            debugBlockPoints[4];
//..............................
// state.c: Patch BSP generation (PatchCol)
extern i32        numPlanes;
extern PatchPlane planes[MAX_PATCH_PLANES];
extern i32        numFacets;
extern Facet      facets[MAX_FACETS];

//..............................

//..............................
void CM_SetGridWrapWidth(cGrid* grid);
bool CM_NeedsSubdivision(const vec3 a, const vec3 b, const vec3 c);
void CM_TransposeGrid(cGrid* grid);
void CM_Subdivide(const vec3 a, const vec3 b, const vec3 c, vec3 out1, vec3 out2, vec3 out3);
void CM_SubdivideGridColumns(cGrid* grid);
bool CM_ComparePoints(const f32* a, const f32* b);
void CM_RemoveDegenerateColumns(cGrid* grid);
void CM_PatchCollideFromGrid(const cGrid* grid, PatchCol* pf);

//..............................
#endif  // COL_PATCH_H
