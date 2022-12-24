#include "../solve.h"

//..................
// Solve: Position Checks
//..................

//..................
// CM_PointLeafnum_r
//   Returns the id number of the leaf where the point is positioned
//   with a negative id offset of num
//   Increases the state of the c_pointcontents counter on success
//..................
i32 CM_PointLeafnum_r(const vec3 p, i32 num) {
  f32     d;
  cNode*  node;
  cPlane* plane;
  while (num >= 0) {
    node  = cm.nodes + num;
    plane = node->plane;

    if (plane->type < 3) d = p[plane->type] - plane->dist;
    else d = GVec3Dot(plane->normal, p) - plane->dist;
    if (d < 0) num = node->children[1];
    else num = node->children[0];
  }
  c_pointcontents++;  // optimize counter
  return -1 - num;
}

//..................
// CM_PointLeafnum
//   Returns the id number of the leaf where the point is positioned
//   Increases the state of the c_pointcontents counter on success
//..................
i32 CM_PointLeafnum(const vec3 p) {
  if (!cm.numNodes) { return 0; }  // map not loaded
  return CM_PointLeafnum_r(p, 0);
}

//..................
// CM_BoundsIntersect
//   Checks if the given AABBs intersect with each other
//   aka AABBtoAABB
//..................
bool CM_BoundsIntersect(const vec3 mins, const vec3 maxs, const vec3 mins2, const vec3 maxs2) {  // clang-format off
  if (   maxs[0] < mins2[0] - BOUNDS_CLIP_EPSILON 
      || maxs[1] < mins2[1] - BOUNDS_CLIP_EPSILON 
      || maxs[2] < mins2[2] - BOUNDS_CLIP_EPSILON
      || mins[0] > maxs2[0] + BOUNDS_CLIP_EPSILON 
      || mins[1] > maxs2[1] + BOUNDS_CLIP_EPSILON 
      || mins[2] > maxs2[2] + BOUNDS_CLIP_EPSILON) {  // clang-format on
    return false;
  }
  return true;
}
//..................
// CM_BoundsIntersectPoint
//   Checks if the given AABB intersects with the given point
//   aka AABBtoPoint
//..................
bool CM_BoundsIntersectPoint(const vec3 mins, const vec3 maxs, const vec3 point) {  // clang-format off
  if (   maxs[0] < point[0] - BOUNDS_CLIP_EPSILON
      || maxs[1] < point[1] - BOUNDS_CLIP_EPSILON
      || maxs[2] < point[2] - BOUNDS_CLIP_EPSILON
      || mins[0] > point[0] + BOUNDS_CLIP_EPSILON
      || mins[1] > point[1] + BOUNDS_CLIP_EPSILON
      || mins[2] > point[2] + BOUNDS_CLIP_EPSILON) {  // clang-format on
    return false;
  }
  return true;
}

//..................
// CM_PositionTestInPatchCollide
//   Checks if the given trace data (TraceWork) is inside any of the given patch facets
//..................
bool CM_PositionTestInPatchCollide(TraceWork* tw, const PatchCol* pc) {
  if (tw->isPoint) { return false; }

  f32    plane[4];
  vec3   startp;
  Facet* facet = pc->facets;
  for (i32 facetId = 0; facetId < pc->numFacets; facetId++, facet++) {
    PatchPlane* pp = &pc->planes[facet->surfacePlane];
    GVec3Copy(pp->plane, plane);
    plane[3] = pp->plane[3];
    if (tw->sphere.use) {
      // adjust the plane distance appropriately for radius
      plane[3] += tw->sphere.radius;

      // find the closest point on the capsule to the plane
      f32 t = GVec3Dot(plane, tw->sphere.offset);
      if (t > 0) {
        GVec3Sub(tw->start, tw->sphere.offset, startp);
      } else {
        GVec3Add(tw->start, tw->sphere.offset, startp);
      }
    } else {
      f32 offset = GVec3Dot(tw->offsets[pp->signbits], plane);
      plane[3] -= offset;
      GVec3Copy(tw->start, startp);
    }

    if (GVec3Dot(plane, startp) - plane[3] > 0.0f) { continue; }

    i32 borderId;
    for (borderId = 0; borderId < facet->numBorders; borderId++) {
      pp = &pc->planes[facet->borderPlanes[borderId]];
      if (facet->borderInward[borderId]) {
        GVec3Neg(pp->plane, plane);
        plane[3] = -pp->plane[3];
      } else {
        GVec3Copy(pp->plane, plane);
        plane[3] = pp->plane[3];
      }
      if (tw->sphere.use) {
        // adjust the plane distance appropriately for radius
        plane[3] += tw->sphere.radius;

        // find the closest point on the capsule to the plane
        f32 t = GVec3Dot(plane, tw->sphere.offset);
        if (t > 0.0f) {
          GVec3Sub(tw->start, tw->sphere.offset, startp);
        } else {
          GVec3Add(tw->start, tw->sphere.offset, startp);
        }
      } else {
        // NOTE: this works even though the plane might be flipped because the bbox is centered
        f32 offset = GVec3Dot(tw->offsets[pp->signbits], plane);
        plane[3] += fabs(offset);
        GVec3Copy(tw->start, startp);
      }

      if (GVec3Dot(plane, startp) - plane[3] > 0.0f) { break; }
    }
    if (borderId < facet->numBorders) { continue; }
    // inside this patch facet
    return true;
  }
  return false;
}

//..............................
// BoxOnPlaneSide
//   Checks on which plane side the AABB is standing
//   Returns:  1, 2, or 1 + 2
//   Guessing this means:
//     1 = 01b  : In front
//     2 = 10b  : Behind
//     3 = 11b  : Straddling
//     dist[0] appears to be the max corner point
//     dist[1] is the min corner point.
//     Selects which one is 0 and which one is 1
//       assuming the normal is stored as all positive numbers,
//       with single bits to represent the sign
//..............................
i32 BoxOnPlaneSide(const vec3 emins, const vec3 emaxs, const cPlane* p) {
  // fast axial cases
  if (p->type < 3) {
    if (p->dist <= emins[p->type]) return 1;
    if (p->dist >= emaxs[p->type]) return 2;
    return 3;
  }
  // general case
  f32 dist[2];
  dist[0] = dist[1] = 0;
  if (p->signbits < 8) {  // >= 8: default case is original code (dist[0]=dist[1]=0)
    i32 b;
    for (i32 i = 0; i < 3; i++) {
      b = (p->signbits >> i) & 1;
      dist[b] += p->normal[i] * emaxs[i];
      dist[!b] += p->normal[i] * emins[i];
    }
  }

  i32 sides = 0;
  if (dist[0] >= p->dist) sides = 1;
  if (dist[1] < p->dist) sides |= 2;

  return sides;
}


//..................
// CM_TestBoxInBrush
//..................
static void CM_TestBoxInBrush(TraceWork* tw, const cBrush* brush) {
  if (!brush->numsides) { return; }
  // special test for axial
  if (tw->bounds[0][0] > brush->bounds[1][0] || tw->bounds[0][1] > brush->bounds[1][1] || tw->bounds[0][2] > brush->bounds[1][2]
      || tw->bounds[1][0] < brush->bounds[0][0] || tw->bounds[1][1] < brush->bounds[0][1] || tw->bounds[1][2] < brush->bounds[0][2]) {
    return;
  }
  cPlane* plane;
  f64     dist;
  f64     d1;
  cBSide* side;
  f64     t;
  vec3    startp;
  if (tw->sphere.use) {
    // the first six planes are the axial planes, so we only
    // need to test the remainder
    for (i32 i = 6; i < brush->numsides; i++) {
      side  = brush->sides + i;
      plane = side->plane;

      // adjust the plane distance appropriately for radius
      dist  = plane->dist + tw->sphere.radius;
      // find the closest point on the capsule to the plane
      t     = DVec3Dot(plane->normal, tw->sphere.offset);
      if (t > 0) {
        DVec3Sub(tw->start, tw->sphere.offset, startp);
      } else {
        DVec3Add(tw->start, tw->sphere.offset, startp);
      }
      d1 = DVec3Dot(startp, plane->normal) - dist;
      // if completely in front of face, no intersection
      if (d1 > 0) { return; }
    }
  } else {
    // the first six planes are the axial planes, so we only
    // need to test the remainder
    for (i32 i = 6; i < brush->numsides; i++) {
      side  = brush->sides + i;
      plane = side->plane;

      // adjust the plane distance appropriately for mins/maxs
      dist  = plane->dist - GVec3Dot(tw->offsets[plane->signbits], plane->normal);
      d1    = DVec3Dot(tw->start, plane->normal) - dist;

      // if completely in front of face, no intersection
      if (d1 > 0) { return; }
    }
  }

  // inside this brush
  tw->trace.startsolid = true;
  tw->trace.allsolid   = true;
  tw->trace.fraction   = 0;
  tw->trace.contents   = brush->contents;
}

//..................
// CM_TestInLeaf
//..................
void CM_TestInLeaf(TraceWork* tw, const cLeaf* leaf) {
  // test box position against all brushes in the leaf
  cBrush* b;
  for (i32 k = 0; k < leaf->numLeafBrushes; k++) {
    i32 brushnum = cm.leafbrushes[leaf->firstLeafBrush + k];
    b            = &cm.brushes[brushnum];
    if (b->checkcount == cm.checkcount) {
      continue;  // already checked this brush in another leaf
    }
    b->checkcount = cm.checkcount;

    if (!(b->contents & tw->contents)) { continue; }

    CM_TestBoxInBrush(tw, b);
    if (tw->trace.allsolid) { return; }
  }

  // test against all patches
  cPatch* patch;
  if (col.doPatchCol) {
    for (i32 k = 0; k < leaf->numLeafSurfaces; k++) {
      patch = cm.surfaces[cm.leafsurfaces[leaf->firstLeafSurface + k]];
      if (!patch) { continue; }
      if (patch->checkcount == cm.checkcount) {
        continue;  // already checked this brush in another leaf
      }
      patch->checkcount = cm.checkcount;

      if (!(patch->contents & tw->contents)) { continue; }

      if (CM_PositionTestInPatchCollide(tw, patch->pc)) {
        tw->trace.startsolid = tw->trace.allsolid = true;
        tw->trace.fraction                        = 0;
        tw->trace.contents                        = patch->contents;
        return;
      }
    }
  }
}

//..................
// CM_TestCapsuleInCapsule
//   Check if the given trace data (TraceWork) capsule
//   is inside the given clipModel handle capsule
//..................
void CM_TestCapsuleInCapsule(TraceWork* tw, cHandle model) {
  vec3 mins, maxs;
  CM_ModelBounds(model, mins, maxs);

  vec3 top, bottom;
  GVec3Add(tw->start, tw->sphere.offset, top);
  GVec3Sub(tw->start, tw->sphere.offset, bottom);
  vec3 offset, symetricSize[2];
  for (i32 i = 0; i < 3; i++) {
    offset[i]          = (mins[i] + maxs[i]) * 0.5;
    symetricSize[0][i] = mins[i] - offset[i];
    symetricSize[1][i] = maxs[i] - offset[i];
  }
  f32 halfWidth  = symetricSize[1][0];
  f32 halfHeight = symetricSize[1][2];
  f32 radius     = (halfWidth > halfHeight) ? halfHeight : halfWidth;
  f32 offs       = halfHeight - radius;

  // check if any of the spheres overlap
  vec3 p1, p2, tmp;
  GVec3Copy(offset, p1);
  p1[2] += offs;
  GVec3Sub(p1, top, tmp);
  f32 r = Sqr(tw->sphere.radius + radius);
  if (Vec3LenSq(tmp) < r) {
    tw->trace.allsolid   = true;
    tw->trace.startsolid = true;
    tw->trace.fraction   = 0;
  }
  GVec3Sub(p1, bottom, tmp);
  if (Vec3LenSq(tmp) < r) {
    tw->trace.allsolid   = true;
    tw->trace.startsolid = true;
    tw->trace.fraction   = 0;
  }
  GVec3Copy(offset, p2);
  p2[2] -= offs;
  GVec3Sub(p2, top, tmp);
  if (Vec3LenSq(tmp) < r) {
    tw->trace.allsolid   = true;
    tw->trace.startsolid = true;
    tw->trace.fraction   = 0;
  }
  GVec3Sub(p2, bottom, tmp);
  if (Vec3LenSq(tmp) < r) {
    tw->trace.allsolid   = true;
    tw->trace.startsolid = true;
    tw->trace.fraction   = 0;
  }
  // if between cylinder up and lower bounds
  if ((top[2] >= p1[2] && top[2] <= p2[2]) || (bottom[2] >= p1[2] && bottom[2] <= p2[2])) {
    // 2d coordinates
    top[2] = p1[2] = 0;
    // if the cylinders overlap
    GVec3Sub(top, p1, tmp);
    if (Vec3LenSq(tmp) < r) {
      tw->trace.allsolid   = true;
      tw->trace.startsolid = true;
      tw->trace.fraction   = 0;
    }
  }
}

//..................
// CM_TestBoundingBoxInCapsule
//   Check if the given trace data (TraceWork) AABB
//   is inside the given clipModel handle capsule
//..................
void CM_TestBoundingBoxInCapsule(TraceWork* tw, cHandle model) {
  // mins maxs of the capsule
  vec3 mins, maxs;
  CM_ModelBounds(model, mins, maxs);

  // offset for capsule center
  vec3 offset, size[2];
  for (i32 i = 0; i < 3; i++) {
    offset[i]  = (mins[i] + maxs[i]) * 0.5;
    size[0][i] = mins[i] - offset[i];
    size[1][i] = maxs[i] - offset[i];
    tw->start[i] -= offset[i];
    tw->end[i] -= offset[i];
  }

  // replace the bounding box with the capsule
  tw->sphere.use        = true;
  tw->sphere.radius     = (size[1][0] > size[1][2]) ? size[1][2] : size[1][0];
  tw->sphere.halfHeight = size[1][2];
  GVec3Set(tw->sphere.offset, 0, 0, size[1][2] - tw->sphere.radius);

  // replace the capsule with the bounding box
  cHandle h    = CM_TempBoxModel(tw->size[0], tw->size[1], false);
  // calculate collision
  cModel* cmod = CM_ClipHandleToModel(h);
  CM_TestInLeaf(tw, &cmod->leaf);
}

//..................
// CM_PositionTest
//..................
void CM_PositionTest(TraceWork* tw) {
  LeafList ll;
  // identify the leafs we are touching
  GVec3Add(tw->start, tw->size[0], ll.bounds[0]);
  GVec3Add(tw->start, tw->size[1], ll.bounds[1]);

  for (i32 i = 0; i < 3; i++) {
    ll.bounds[0][i] -= 1;
    ll.bounds[1][i] += 1;
  }

  i32 leafs[MAX_POSITION_LEAFS];
  ll.list       = leafs;
  ll.count      = 0;
  ll.maxcount   = MAX_POSITION_LEAFS;
  ll.storeLeafs = CM_StoreLeafs;
  ll.lastLeaf   = 0;
  ll.overflowed = false;

  cm.checkcount++;

  CM_BoxLeafnums_r(&ll, 0);

  cm.checkcount++;

  // test the contents of the leafs
  for (i32 i = 0; i < ll.count; i++) {
    CM_TestInLeaf(tw, &cm.leafs[leafs[i]]);
    if (tw->trace.allsolid) { break; }
  }
}

