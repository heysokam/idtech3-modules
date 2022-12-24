#include "../state.h"

//..............................
// Runtime configurable state (cvar replacement)
ColCfg  col;
LoadCfg load;
//..............................
void CM_InitCfg(void) {
  col.doVIS            = 1;
  col.doPatchCol       = 1;
  col.doPlayerCurveCol = 1;
  col.dbg.surfUpdate   = 1;
  load.noCurves        = 0;
  load.developer       = 1;
}

//..............................
// Map loading state variables
// extern in local.h, for usage in the col/ module
cMap  cm;         // Currently loaded clipMap data
byte* cmod_base;  // Base clipModel raw data

// Statistics tracking
// Counters for successfully registered collision checks so far
i32 c_pointcontents;
i32 c_traces;        // Number of trace sweeps
i32 c_brush_traces;  // Moving checks through brushes
i32 c_patch_traces;  // Moving checks through patches
i32 c_totalPatchBlocks;
// debug counters are only bumped when running single threaded,
// because they are an awful coherence problem
i32 c_active_windings;
i32 c_peak_windings;
i32 c_winding_allocs;
i32 c_winding_points;

//..............................
// Patch debugging
const PatchCol* debugPatchCollide;
const Facet*    debugFacet;
bool            debugBlock;
vec3            debugBlockPoints[4];
//..............................
void CM_ClearLevelPatches(void) {
  debugPatchCollide = NULL;
  debugFacet        = NULL;
}
//..............................
// Patch BSP generation (PatchCol)
i32        numPlanes;
PatchPlane planes[MAX_PATCH_PLANES];
i32        numFacets;
Facet      facets[MAX_FACETS];

//..............................
// Temporary player BSP from its AABB (capsules are not converted to bsp)
cModel  box_model;
cPlane* box_planes;
cBrush* box_brush;
//..............................
// CM_InitBoxHull
//   Set up the planes and nodes so that the six floats of a bounding box
//   can just be stored out and get a proper clipping hull structure.
void CM_InitBoxHull(void) {
  box_planes                        = &cm.planes[cm.numPlanes];
  box_brush                         = &cm.brushes[cm.numBrushes];
  box_brush->numsides               = 6;
  box_brush->sides                  = cm.BSides + cm.numBSides;
  box_brush->contents               = CONTENTS_BODY;
  box_model.leaf.numLeafBrushes     = 1;
  //	box_model.leaf.firstLeafBrush = cm.numBrushes;
  box_model.leaf.firstLeafBrush     = cm.numLeafBrushes;
  cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

  for (i32 i = 0; i < 6; i++) {
    i32 side        = i & 1;

    // brush sides
    cBSide* s       = &cm.BSides[cm.numBSides + i];
    s->plane        = cm.planes + (cm.numPlanes + i * 2 + side);
    s->surfaceFlags = 0;

    // planes
    cPlane* p       = &box_planes[i * 2];
    p->type         = i >> 1;
    p->signbits     = 0;
    GVec3Clear(p->normal);
    p->normal[i >> 1] = 1;

    p                 = &box_planes[i * 2 + 1];
    p->type           = 3 + (i >> 1);
    p->signbits       = 0;
    GVec3Clear(p->normal);
    p->normal[i >> 1] = -1;

    SetPlaneSignbits(p);
  }
}


//.................................
// Storing data as the current state
//.................................

//..................
// CM_StoreLeafs
//   Stores the leaves of the given LeafList into the currently loaded map data (col/c/state.c)
//..................
void CM_StoreLeafs(LeafList* ll, i32 nodeNum) {
  i32 leafNum = -1 - nodeNum;

  // store the lastLeaf even if the list is overflowed
  if (cm.leafs[leafNum].cluster != -1) { ll->lastLeaf = leafNum; }

  if (ll->count >= ll->maxcount) {
    ll->overflowed = true;
    return;
  }
  ll->list[ll->count++] = leafNum;
}

//..................
// CM_BoxLeafnums_r
//   Stores all the leafs touched by the given LeafList, recursively
//..................
void CM_BoxLeafnums_r(LeafList* ll, i32 nodeNum) {
  cPlane* plane;
  cNode*  node;
  i32     s;
  while (1) {
    if (nodeNum < 0) {              // Negative numbers are leaves
      ll->storeLeafs(ll, nodeNum);  // Store the current leaf
      return;
    }
    node  = &cm.nodes[nodeNum];
    plane = node->plane;
    s     = BoxOnPlaneSide(ll->bounds[0], ll->bounds[1], plane);
    if (s == 1) {
      nodeNum = node->children[0];
    } else if (s == 2) {
      nodeNum = node->children[1];
    } else {
      // go down both
      CM_BoxLeafnums_r(ll, node->children[0]);
      nodeNum = node->children[1];
    }
  }
}

//..................
// CM_BoxLeafnums
//   Stores all the leafs touched by the given AABB
//   Will recurse through all of the leaves contained in the given list
//   Only returns non-solid leafs
//   Overflow if return listsize and if *lastLeaf != list[listsize-1]
//..................
i32 CM_BoxLeafnums(const vec3 mins, const vec3 maxs, i32* list, i32 listsize, i32* lastLeaf) {
  cm.checkcount++;

  LeafList ll;
  GVec3Copy(mins, ll.bounds[0]);
  GVec3Copy(maxs, ll.bounds[1]);
  ll.count      = 0;
  ll.maxcount   = listsize;
  ll.list       = list;
  ll.storeLeafs = CM_StoreLeafs;
  ll.lastLeaf   = 0;
  ll.overflowed = false;

  CM_BoxLeafnums_r(&ll, 0);

  *lastLeaf = ll.lastLeaf;
  return ll.count;
}


//..................
// CM_StoreBrushes
//   Stores the brushes of the given LeafList into the currently loaded map data (col/c/state.c)
//..................
void CM_StoreBrushes(LeafList* ll, i32 nodeNum) {
  i32    leafnum = -1 - nodeNum;
  cLeaf* leaf    = &cm.leafs[leafnum];

  cBrush* b;
  i32     brushnum;
  i32     i;
  for (i32 k = 0; k < leaf->numLeafBrushes; k++) {
    brushnum = cm.leafbrushes[leaf->firstLeafBrush + k];
    b        = &cm.brushes[brushnum];
    if (b->checkcount == cm.checkcount) {
      continue;  // already checked this brush in another leaf
    }
    b->checkcount = cm.checkcount;
    for (i = 0; i < 3; i++) {
      if (b->bounds[0][i] >= ll->bounds[1][i] || b->bounds[1][i] <= ll->bounds[0][i]) { break; }
    }
    if (i != 3) { continue; }
    if (ll->count >= ll->maxcount) {
      ll->overflowed = true;
      return;
    }
    ((cBrush**)ll->list)[ll->count++] = b;
  }
}

//..................
// CM_BoxBrushes
//   Stores the brushes of the given LeafList, contained in the given AABB
//   into the currently loaded map data (col/c/state.c)
//..................
i32 CM_BoxBrushes(const vec3 mins, const vec3 maxs, cBrush** list, i32 listsize) {
  cm.checkcount++;

  LeafList ll;
  GVec3Copy(mins, ll.bounds[0]);
  GVec3Copy(maxs, ll.bounds[1]);
  ll.count      = 0;
  ll.maxcount   = listsize;
  ll.list       = (void*)list;
  ll.storeLeafs = CM_StoreBrushes;
  ll.lastLeaf   = 0;
  ll.overflowed = false;

  CM_BoxLeafnums_r(&ll, 0);

  return ll.count;
}

//.................................
// CM_TempBoxModel
//   Stores the given AABB into the box_model state variable, and returns its clipHandle
//   The function has two behaviors, depending on the bool value of `capsule`:
//   1. true:
//     store AABB in box_model
//     return CAPSULE_MODEL_HANDLE
//   2. false:
//     store AABB in box_model
//     convert AABB to bsp, and store the result in box_planes
//     store AABB in box_brush->bounds (aka box_brush AABB)
//     return BOX_MODEL_HANDLE
//...............
// id-Tech notes:
//   To keep everything uniform:
//   Bounding Boxes are turned into small BSP trees, instead of being compared directly.
//   Capsules are handled differently, though.
//.................................
cHandle CM_TempBoxModel(const vec3 mins, const vec3 maxs, i32 capsule) {
  GVec3Copy(mins, box_model.mins);
  GVec3Copy(maxs, box_model.maxs);

  if (capsule) { return CAPSULE_MODEL_HANDLE; }

  box_planes[0].dist  = maxs[0];
  box_planes[1].dist  = -maxs[0];
  box_planes[2].dist  = mins[0];
  box_planes[3].dist  = -mins[0];
  box_planes[4].dist  = maxs[1];
  box_planes[5].dist  = -maxs[1];
  box_planes[6].dist  = mins[1];
  box_planes[7].dist  = -mins[1];
  box_planes[8].dist  = maxs[2];
  box_planes[9].dist  = -maxs[2];
  box_planes[10].dist = mins[2];
  box_planes[11].dist = -mins[2];

  GVec3Copy(mins, box_brush->bounds[0]);
  GVec3Copy(maxs, box_brush->bounds[1]);

  return BOX_MODEL_HANDLE;
}

//.................................
// CM_GeneratePatchCollide
//   Creates an internal BSP structure
//   that will be used to perform collision detection with a patch mesh.
//   Points is packed as concatenated rows.
PatchCol* CM_GeneratePatchCollide(i32 width, i32 height, vec3* points) {
  if (width <= 2 || height <= 2 || !points) { err(ERR_DROP, "CM_GeneratePatchFacets: bad parameters: (%i, %i, %p)", width, height, (void*)points); }
  if (!(width & 1) || !(height & 1)) { err(ERR_DROP, "CM_GeneratePatchFacets: even sizes are invalid for quadratic meshes"); }
  if (width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) { err(ERR_DROP, "CM_GeneratePatchFacets: source is > MAX_GRID_SIZE"); }
  // build a grid
  cGrid grid;
  grid.width      = width;
  grid.height     = height;
  grid.wrapWidth  = false;
  grid.wrapHeight = false;
  for (i32 i = 0; i < width; i++) {
    for (i32 j = 0; j < height; j++) { GVec3Copy(points[j * width + i], grid.points[i][j]); }
  }
  // subdivide the grid
  CM_SetGridWrapWidth(&grid);
  CM_SubdivideGridColumns(&grid);
  CM_RemoveDegenerateColumns(&grid);
  CM_TransposeGrid(&grid);
  CM_SetGridWrapWidth(&grid);
  CM_SubdivideGridColumns(&grid);
  CM_RemoveDegenerateColumns(&grid);
  // we now have a grid of points exactly on the curve
  // the approximate surface defined by these points will be collided against
  PatchCol* pf = Hunk_Alloc(sizeof(*pf), h_high);
  ClearBounds(pf->bounds[0], pf->bounds[1]);
  for (i32 i = 0; i < grid.width; i++) {
    for (i32 j = 0; j < grid.height; j++) { AddPointToBounds(grid.points[i][j], pf->bounds[0], pf->bounds[1]); }
  }
  c_totalPatchBlocks += (grid.width - 1) * (grid.height - 1);
  // generate a bsp tree for the surface
  CM_PatchCollideFromGrid(&grid, pf);
  // expand by one unit for epsilon purposes
  pf->bounds[0][0] -= 1;
  pf->bounds[0][1] -= 1;
  pf->bounds[0][2] -= 1;
  pf->bounds[1][0] += 1;
  pf->bounds[1][1] += 1;
  pf->bounds[1][2] += 1;
  return pf;
}


//.................................
// Getting data from the current state
//.................................

//..................
// CM_ClipHandleToModel
//   Returns a pointer to the clipModel data, for the currently loaded map, stored at handle id number
//   Will return the box_model handle when passing BOX_MODEL_HANDLE as input
//..................
cModel* CM_ClipHandleToModel(cHandle handle) {
  if (handle < 0) { err(ERR_DROP, "%s: bad handle %i", __func__, handle); }
  if (handle < cm.numSubModels) { return &cm.cmodels[handle]; }
  if (handle == BOX_MODEL_HANDLE) { return &box_model; }
  if (handle < MAX_SUBMODELS) { err(ERR_DROP, "%s: bad handle %i < %i < %i", __func__, cm.numSubModels, handle, MAX_SUBMODELS); }
  err(ERR_DROP, "%s: bad handle %i", __func__, handle + MAX_SUBMODELS);
  return NULL;
}
//..................
// CM_InlineModel
//..................
cHandle CM_InlineModel(i32 index) {
  if (index < 0 || index >= cm.numSubModels) { err(ERR_DROP, "CM_InlineModel: bad number"); }
  return index;
}
//..................
// CM_NumClusters
//..................
i32 CM_NumClusters(void) { return cm.numClusters; }

//..................
// CM_NumInlineModels
//..................
i32 CM_NumInlineModels(void) { return cm.numSubModels; }

//..................
// CM_EntityString
//..................
char* CM_EntityString(void) { return cm.entityString; }

//..................
// CM_LeafCluster
//..................
i32 CM_LeafCluster(i32 leafnum) {
  if (leafnum < 0 || leafnum >= cm.numLeafs) { err(ERR_DROP, "CM_LeafCluster: bad number"); }
  return cm.leafs[leafnum].cluster;
}

//..................
// CM_LeafArea
//..................
i32 CM_LeafArea(i32 leafnum) {
  if (leafnum < 0 || leafnum >= cm.numLeafs) { err(ERR_DROP, "CM_LeafArea: bad number"); }
  return cm.leafs[leafnum].area;
}

//..................
// CM_PointContents
//   Returns the ORed contents mask of the given clipModel at a given point
//..................
i32 CM_PointContents(const vec3 p, cHandle model) {
  if (!cm.numNodes) { return 0; }  // map not loaded

  cModel* clipm;
  cLeaf*  leaf;
  i32     leafnum;
  if (model) {
    clipm = CM_ClipHandleToModel(model);
    leaf  = &clipm->leaf;
  } else {
    leafnum = CM_PointLeafnum_r(p, 0);
    leaf    = &cm.leafs[leafnum];
  }

  i32 contents = 0;
  for (i32 k = 0; k < leaf->numLeafBrushes; k++) {
    i32     brushId = cm.leafbrushes[leaf->firstLeafBrush + k];
    cBrush* b       = &cm.brushes[brushId];
    if (!CM_BoundsIntersectPoint(b->bounds[0], b->bounds[1], p)) { continue; }

    // see if the point is in the brush
    i32 sideId;
    for (sideId = 0; sideId < b->numsides; sideId++) {
      f32 dot = GVec3Dot(p, b->sides[sideId].plane->normal);
      if (dot > b->sides[sideId].plane->dist) { break; }
    }
    if (sideId == b->numsides) { contents |= b->contents; }
  }

  return contents;
}

//..................
// CM_TransformedPointContents
//   Returns the ORed contents mask of the given clipModel at a given point
//   Handles offseting and rotation of the end points (moving and rotating entities)
//..................
i32 CM_TransformedPointContents(const vec3 p, cHandle model, const vec3 origin, const vec3 angles) {
  // subtract origin offset
  vec3 p_l;
  GVec3Sub(p, origin, p_l);

  // rotate start and end into the models frame of reference
  if (model != BOX_MODEL_HANDLE && (angles[0] || angles[1] || angles[2])) {
    vec3 forward, right, up;
    AngleVectors(angles, forward, right, up);

    vec3 temp;
    GVec3Copy(p_l, temp);
    p_l[0] = GVec3Dot(temp, forward);
    p_l[1] = -GVec3Dot(temp, right);
    p_l[2] = GVec3Dot(temp, up);
  }

  return CM_PointContents(p_l, model);
}
