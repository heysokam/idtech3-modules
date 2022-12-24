#ifndef COL_SOLVE_H
#define COL_SOLVE_H
//....................................

// stdlib dependencies
#include <assert.h>
// Engine dependencies
#include "../core/state.h"  // For env variables access
// Collision dependencies
#include "./types.h"
#include "./math.h"
#include "./flags.h"

//..............................
// State Variables : from state.h
//..............................
// Configurable runtime behavior (cvar replacement)
extern ColCfg  col;
extern LoadCfg load;
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
//..............................
// Patch Debugging
extern const PatchCol* debugPatchCollide;
extern const Facet*    debugFacet;
extern bool            debugBlock;
extern vec3            debugBlockPoints[4];
//..............................
// State Setters/Getters : from state.h
//..............................
cHandle CM_TempBoxModel(const vec3 mins, const vec3 maxs, int capsule);
cModel* CM_ClipHandleToModel(cHandle handle);
void    CM_StoreLeafs(LeafList* ll, i32 nodeNum);
void    CM_BoxLeafnums_r(LeafList* ll, i32 nodeNum);


//....................................
// tools.c
void CM_ModelBounds(cHandle model, vec3 mins, vec3 maxs);

//....................................
// position.c
i32  BoxOnPlaneSide(const vec3 emins, const vec3 emaxs, const struct cplane_s* p);
bool CM_BoundsIntersect(const vec3 mins, const vec3 maxs, const vec3 mins2, const vec3 maxs2);
bool CM_BoundsIntersectPoint(const vec3 mins, const vec3 maxs, const vec3 point);
i32  CM_PointLeafnum_r(const vec3 p, i32 num);
i32  CM_PointLeafnum(const vec3 p);
void CM_TestCapsuleInCapsule(TraceWork* tw, cHandle model);
void CM_TestBoundingBoxInCapsule(TraceWork* tw, cHandle model);
void CM_TestInLeaf(TraceWork* tw, const cLeaf* leaf);
void CM_PositionTest(TraceWork* tw);
i32  CM_PointContents(const vec3 p, cHandle model);
//....................................
// trace.c
void CM_BoxTrace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, i32 brushmask, bool capsule);

//....................................
#endif  // COL_SOLVE_H
