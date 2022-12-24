#ifndef COL_CORE_H
#define COL_CORE_H
//....................................

#include "./types.h"

//....................................
// load.h : State Setter
void CM_LoadMap(const char* name, bool clientload, int* checksum);
void CM_ClearMap(void);

//....................................
// state.h : Getters
cHandle CM_InlineModel(i32 index);
i32     CM_NumClusters(void);
i32     CM_NumInlineModels(void);
char*   CM_EntityString(void);
i32     CM_LeafCluster(i32 leafnum);
i32     CM_LeafArea(i32 leafnum);
i32     CM_PointContents(const vec3 p, cHandle model);
i32     CM_TransformedPointContents(const vec3 p, cHandle model, const vec3 origin, const vec3 angles);
i32     CM_BoxLeafnums(const vec3 mins, const vec3 maxs, i32* list, i32 listsize, i32* lastLeaf);
// state.h : Setters
cHandle CM_TempBoxModel(const vec3 mins, const vec3 maxs, int capsule);

//....................................
// vis.h : Solvers
byte* CM_ClusterPVS(i32 cluster);
bool  CM_AreasConnected(i32 area1, i32 area2);
i32   CM_WriteAreaBits(byte* buffer, i32 area);
void  CM_AdjustAreaPortalState(i32 area1, i32 area2, bool open);

//....................................
// Solve: General   tools.c
void CM_ModelBounds(cHandle model, vec3 mins, vec3 maxs);
// Solve: Trace     trace.c
void CM_BoxTrace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, i32 brushmask, bool capsule);
void CM_TransformedBoxTrace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, i32 brushmask, const vec3 origin,
                            const vec3 angles, bool capsule);
// Solve: Position   position.c
i32 CM_PointLeafnum(const vec3 p);

//....................................
// Debug: Patches   patch.c
void CM_DrawDebugSurface(void (*drawPoly)(i32 color, i32 numPoints, f32* points));

//....................................
#endif  // COL_CORE_H
