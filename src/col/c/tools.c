#include "../types.h"
#include "../math.h"
#include "../state.h"

//..............................
// Solve: General tools
//..............................

//..............................
// CM_ModelBounds
//   Returns the AABB of the input clipModel handleId
//..............................
void CM_ModelBounds(cHandle model, vec3 mins, vec3 maxs) {
  cModel* cmod = CM_ClipHandleToModel(model);
  GVec3Copy(cmod->mins, mins);
  GVec3Copy(cmod->maxs, maxs);
}
