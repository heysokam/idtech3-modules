#ifndef COL_VIS_H
#define COL_VIS_H
//..............................

#include "../tools/core.h"
#include "./types.h"
#include "../core/state.h"
#include "./state.h"

//..............................
// PVS
byte* CM_ClusterPVS(i32 cluster);

//..............................
// Area Portals
void CM_FloodAreaConnections(void);

//..............................
#endif  // COL_VIS_H
