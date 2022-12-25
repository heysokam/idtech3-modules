#ifndef COL_STATE_H
#define COL_STATE_H
//..............................

#include "../mem/core.h"
#include "./types.h"
#include "./math.h"
#include "./flags.h"
#include "./solve.h"
#include "./patch.h"

//..............................
// Configurable runtime behavior (cvar replacement)
extern ColCfg  col;
extern LoadCfg load;
//..............................
void CM_InitCfg(void);

//..............................
// Currently loaded clipMap data
extern byte* cmod_base;  // Base clipModel raw data
extern cMap  cm;         // Currently loaded clipMap data

//..............................
// Successful checks counters
extern i32 c_pointcontents;
extern i32 c_traces;
extern i32 c_brush_traces;
extern i32 c_patch_traces;
// Debug counters
extern i32 c_active_windings;
extern i32 c_peak_windings;
extern i32 c_winding_allocs;
extern i32 c_winding_points;

//..............................
// Temporary bsp for the player AABB
extern cModel  box_model;
extern cPlane* box_planes;
extern cBrush* box_brush;
//..............................
void CM_InitBoxHull(void);

//..............................
// Patch Debugging
extern const PatchCol* debugPatchCollide;
extern const Facet*    debugFacet;
extern bool            debugBlock;
extern vec3            debugBlockPoints[4];
//..............................
void CM_ClearLevelPatches(void);
//..............................
// Patch BSP generation (PatchCol)
extern i32        numPlanes;
extern PatchPlane planes[MAX_PATCH_PLANES];
extern i32        numFacets;
extern Facet      facets[MAX_FACETS];

//..............................
// State Setters
//..............................
void      CM_StoreLeafs(LeafList* ll, i32 nodeNum);
void      CM_StoreBrushes(LeafList* ll, i32 nodeNum);
cHandle   CM_TempBoxModel(const vec3 mins, const vec3 maxs, int capsule);
PatchCol* CM_GeneratePatchCollide(i32 width, i32 height, vec3* points);

//..............................
// State Getters
//..............................
// New
void    CM_EchoTest(void);
void    CM_GetName(char* name, i32 maxchars);
// Original
void    CM_BoxLeafnums_r(LeafList* ll, i32 nodeNum);
cModel* CM_ClipHandleToModel(cHandle handle);
cHandle CM_InlineModel(i32 index);
i32     CM_NumClusters(void);
i32     CM_NumInlineModels(void);
char*   CM_EntityString(void);
i32     CM_LeafCluster(i32 leafnum);
i32     CM_LeafArea(i32 leafnum);
i32     CM_PointContents(const vec3 p, cHandle model);
i32     CM_TransformedPointContents(const vec3 p, cHandle model, const vec3 origin, const vec3 angles);

//..............................
#endif  // COL_STATE_H
