#include "../solve.h"

//..................
// Solve: Trace Checks  (aka sweep/line/movement)
//..................

//..................
// CM_TracePointThroughPatchCollide
//   Sweep a trace point through a patch
//   Special case for point traces because patch collide "brushes" have no volume
//..................
static void CM_TracePointThroughPatchCollide(TraceWork* tw, const PatchCol* pc) {
  if (!col.doPlayerCurveCol || !tw->isPoint) { return; }

  // determine the trace's relationship to all planes
  const PatchPlane* pp;
  pp = pc->planes;
  bool frontFacing[MAX_PATCH_PLANES];
  f32  intersection[MAX_PATCH_PLANES];
  f32  offset;
  f32  d1, d2;
  for (i32 planeId = 0; planeId < pc->numPlanes; planeId++, pp++) {
    offset = GVec3Dot(tw->offsets[pp->signbits], pp->plane);
    d1     = GVec3Dot(tw->start, pp->plane) - pp->plane[3] + offset;
    d2     = GVec3Dot(tw->end, pp->plane) - pp->plane[3] + offset;
    if (d1 <= 0) {
      frontFacing[planeId] = false;
    } else {
      frontFacing[planeId] = true;
    }
    if (d1 == d2) {
      intersection[planeId] = 99999;
    } else {
      intersection[planeId] = d1 / (d1 - d2);
      if (intersection[planeId] <= 0) { intersection[planeId] = 99999; }
    }
  }
  // see if any of the surface planes are intersected
  const Facet* facet;
  facet = pc->facets;
  f32 intersect;
  i32 planeBorderId;
  for (i32 facetId = 0; facetId < pc->numFacets; facetId++, facet++) {
    if (!frontFacing[facet->surfacePlane]) { continue; }
    intersect = intersection[facet->surfacePlane];
    if (intersect < 0) { continue; }                   // surface is behind the starting point
    if (intersect > tw->trace.fraction) { continue; }  // already hit something closer
    i32 borderId;
    for (borderId = 0; borderId < facet->numBorders; borderId++) {
      planeBorderId = facet->borderPlanes[borderId];
      if (frontFacing[planeBorderId] ^ facet->borderInward[borderId]) {
        if (intersection[planeBorderId] > intersect) { break; }
      } else {
        if (intersection[planeBorderId] < intersect) { break; }
      }
    }
    if (borderId == facet->numBorders) {
      // we hit this facet
      if (col.dbg.surfUpdate) {  // Store it as the debuggable patch
        debugPatchCollide = pc;
        debugFacet        = facet;
      }
      pp                 = &pc->planes[facet->surfacePlane];
      // calculate intersection with a slight pushoff
      offset             = GVec3Dot(tw->offsets[pp->signbits], pp->plane);
      d1                 = GVec3Dot(tw->start, pp->plane) - pp->plane[3] + offset;
      d2                 = GVec3Dot(tw->end, pp->plane) - pp->plane[3] + offset;
      tw->trace.fraction = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);

      if (tw->trace.fraction < 0) { tw->trace.fraction = 0; }

      GVec3Copy(pp->plane, tw->trace.plane.normal);
      tw->trace.plane.dist = pp->plane[3];
    }
  }
}

//..................
// CM_CheckFacetPlane
//..................
static i32 CM_CheckFacetPlane(const f32* plane, const vec3 start, const vec3 end, f32* enterFrac, f32* leaveFrac, i32* hit) {
  *hit   = false;
  f32 d1 = GVec3Dot(start, plane) - plane[3];
  f32 d2 = GVec3Dot(end, plane) - plane[3];
  // if completely in front of face, no intersection with the entire facet
  if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) { return false; }
  // if it doesn't cross the plane, the plane isn't relevant
  if (d1 <= 0 && d2 <= 0) { return true; }
  // crosses face
  if (d1 > d2) {  // enter
    f32 f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
    if (f < 0) { f = 0; }
    // always favor previous plane hits and thus also the surface plane hit
    if (f > *enterFrac) {
      *enterFrac = f;
      *hit       = true;
    }
  } else {  // leave
    f32 f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
    if (f > 1) { f = 1; }
    if (f < *leaveFrac) { *leaveFrac = f; }
  }
  return true;
}


//..................
// CM_TraceThroughPatchCollide
//   Checks if the given trace data (TraceWork) passes through any of the given patch facets
//..................
void CM_TraceThroughPatchCollide(TraceWork* tw, const PatchCol* pc) {
  if (!CM_BoundsIntersect(tw->bounds[0], tw->bounds[1], pc->bounds[0], pc->bounds[1])) { return; }
  if (tw->isPoint) {
    CM_TracePointThroughPatchCollide(tw, pc);
    return;
  }

  f32 bestplane[4];
  GVec4Set(bestplane, 0, 0, 0, 0);

  PatchPlane* pp;
  Facet*      facet = pc->facets;
  vec3        startp, endp;
  f32         offset, enterFrac, leaveFrac;
  f32         plane[4];
  i32         hit, hitnum;
  for (i32 facetId = 0; facetId < pc->numFacets; facetId++, facet++) {
    enterFrac = -1.0;
    leaveFrac = 1.0;
    hitnum    = -1;
    //
    pp        = &pc->planes[facet->surfacePlane];
    GVec3Copy(pp->plane, plane);
    plane[3] = pp->plane[3];
    if (tw->sphere.use) {
      // adjust the plane distance appropriately for radius
      plane[3] += tw->sphere.radius;

      // find the closest point on the capsule to the plane
      f32 t = GVec3Dot(plane, tw->sphere.offset);
      if (t > 0.0f) {
        GVec3Sub(tw->start, tw->sphere.offset, startp);
        GVec3Sub(tw->end, tw->sphere.offset, endp);
      } else {
        GVec3Add(tw->start, tw->sphere.offset, startp);
        GVec3Add(tw->end, tw->sphere.offset, endp);
      }
    } else {
      offset = GVec3Dot(tw->offsets[pp->signbits], plane);
      plane[3] -= offset;
      GVec3Copy(tw->start, startp);
      GVec3Copy(tw->end, endp);
    }

    if (!CM_CheckFacetPlane(plane, startp, endp, &enterFrac, &leaveFrac, &hit)) { continue; }
    if (hit) { GVec4Copy(plane, bestplane); }

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
          GVec3Sub(tw->end, tw->sphere.offset, endp);
        } else {
          GVec3Add(tw->start, tw->sphere.offset, startp);
          GVec3Add(tw->end, tw->sphere.offset, endp);
        }
      } else {
        // NOTE: this works even though the plane might be flipped because the bbox is centered
        offset = GVec3Dot(tw->offsets[pp->signbits], plane);
        plane[3] += fabs(offset);
        GVec3Copy(tw->start, startp);
        GVec3Copy(tw->end, endp);
      }

      if (!CM_CheckFacetPlane(plane, startp, endp, &enterFrac, &leaveFrac, &hit)) { break; }
      if (hit) {
        hitnum = borderId;
        GVec4Copy(plane, bestplane);
      }
    }
    if (borderId < facet->numBorders) continue;
    // never clip against the back side
    if (hitnum == facet->numBorders - 1) continue;

    if (enterFrac < leaveFrac && enterFrac >= 0) {
      if (enterFrac < tw->trace.fraction) {
        if (col.dbg.surfUpdate) {
          debugPatchCollide = pc;
          debugFacet        = facet;
        }
        tw->trace.fraction = enterFrac;
        GVec3Copy(bestplane, tw->trace.plane.normal);
        tw->trace.plane.dist = bestplane[3];
      }
    }
  }
}


//..................
// CM_TraceThroughPatch
//   Checks if the given trace data (TraceWork) passes through any of the given clipPatch facets
//   Increases the c_patch_traces counter
//..................
static void CM_TraceThroughPatch(TraceWork* tw, const cPatch* patch) {
  c_patch_traces++;
  f32 oldFrac = tw->trace.fraction;

  CM_TraceThroughPatchCollide(tw, patch->pc);

  if (tw->trace.fraction < oldFrac) {
    tw->trace.surfaceFlags = patch->surfaceFlags;
    tw->trace.contents     = patch->contents;
  }
}

//..................
// CM_TraceThroughBrush
//   Checks if the given trace data (TraceWork) passes through any of the given clipBrush planes
//   Increases the c_brush_traces counter
//..................
static void CM_TraceThroughBrush(TraceWork* tw, const cBrush* brush) {
  if (!brush->numsides) { return; }
  c_brush_traces++;

  bool getout       = false;
  bool startout     = false;

  f32     enterFrac = -1.0;
  f32     leaveFrac = 1.0;
  cPlane* clipplane = NULL;
  cBSide* leadside  = NULL;
  vec3    startp;
  vec3    endp;
  cBSide* side;
  cPlane* plane;
  if (tw->sphere.use) {
    // compare the trace against all planes of the brush
    // find the latest time the trace crosses a plane towards the interior
    // and the earliest time the trace crosses a plane towards the exterior
    for (i32 sideId = 0; sideId < brush->numsides; sideId++) {
      side     = brush->sides + sideId;
      plane    = side->plane;
      // adjust the plane distance appropriately for radius
      f32 dist = plane->dist + tw->sphere.radius;
      // find the closest point on the capsule to the plane
      f32 t    = GVec3Dot(plane->normal, tw->sphere.offset);
      if (t > 0) {
        DVec3Sub(tw->start, tw->sphere.offset, startp);
        DVec3Sub(tw->end, tw->sphere.offset, endp);
      } else {
        DVec3Add(tw->start, tw->sphere.offset, startp);
        DVec3Add(tw->end, tw->sphere.offset, endp);
      }
      f32 d1 = GVec3Dot(startp, plane->normal) - dist;
      f32 d2 = GVec3Dot(endp, plane->normal) - dist;
      if (d2 > 0) { getout = true; }  // endpoint is not in solid
      if (d1 > 0) { startout = true; }
      // if completely in front of face, no intersection with the entire brush
      if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) { return; }
      // if it doesn't cross the plane, the plane isn't relevant
      if (d1 <= 0 && d2 <= 0) { continue; }
      // crosses face
      if (d1 > d2) {  // enter
        f32 f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
        if (f < 0) { f = 0; }
        if (f > enterFrac) {
          enterFrac = f;
          clipplane = plane;
          leadside  = side;
        }
      } else {  // leave
        f32 f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
        if (f > 1) { f = 1; }
        if (f < leaveFrac) { leaveFrac = f; }
      }
    }
  } else {
    // compare the trace against all planes of the brush
    // find the latest time the trace crosses a plane towards the interior
    // and the earliest time the trace crosses a plane towards the exterior
    for (i32 sideId = 0; sideId < brush->numsides; sideId++) {
      side     = brush->sides + sideId;
      plane    = side->plane;

      // adjust the plane distance appropriately for mins/maxs
      f32 dist = plane->dist - GVec3Dot(tw->offsets[plane->signbits], plane->normal);
      f32 d1   = GVec3Dot(tw->start, plane->normal) - dist;
      f32 d2   = GVec3Dot(tw->end, plane->normal) - dist;
      if (d2 > 0) { getout = true; }  // endpoint is not in solid
      if (d1 > 0) { startout = true; }
      // if completely in front of face, no intersection with the entire brush
      if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) { return; }
      // if it doesn't cross the plane, the plane isn't relevant
      if (d1 <= 0 && d2 <= 0) { continue; }
      // crosses face
      if (d1 > d2) {  // enter
        f32 f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
        if (f < 0) { f = 0; }
        if (f > enterFrac) {
          enterFrac = f;
          clipplane = plane;
          leadside  = side;
        }
      } else {  // leave
        f32 f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
        if (f > 1) { f = 1; }
        if (f < leaveFrac) { leaveFrac = f; }
      }
    }
  }

  // all planes have been checked, and the trace was not completely outside the brush
  if (!startout) {  // original point was inside brush
    tw->trace.startsolid = true;
    if (!getout) {
      tw->trace.allsolid = true;
      tw->trace.fraction = 0;
      tw->trace.contents = brush->contents;
    }
    return;
  }

  if (enterFrac < leaveFrac) {
    if (enterFrac > -1 && enterFrac < tw->trace.fraction) {
      if (enterFrac < 0) { enterFrac = 0; }
      tw->trace.fraction = enterFrac;
      if (clipplane != NULL) { tw->trace.plane = *clipplane; }
      if (leadside != NULL) { tw->trace.surfaceFlags = leadside->surfaceFlags; }
      tw->trace.contents = brush->contents;
    }
  }
}

//..................
// CM_TraceThroughLeaf
//   Checks if the given trace data (TraceWork)
//   passes through any of the given clipLeaf brushes or patches
//..................
static void CM_TraceThroughLeaf(TraceWork* tw, const cLeaf* leaf) {
  // trace line against all brushes in the leaf
  for (i32 leafBrushId = 0; leafBrushId < leaf->numLeafBrushes; leafBrushId++) {
    i32     brushnum = cm.leafbrushes[leaf->firstLeafBrush + leafBrushId];
    cBrush* b        = &cm.brushes[brushnum];
    if (b->checkcount == cm.checkcount) { continue; }  // already checked this brush in another leaf
    b->checkcount = cm.checkcount;
    if (!(b->contents & tw->contents)) { continue; }
    if (!CM_BoundsIntersect(tw->bounds[0], tw->bounds[1], b->bounds[0], b->bounds[1])) { continue; }
    CM_TraceThroughBrush(tw, b);
    if (!tw->trace.fraction) { return; }
  }

  // trace line against all patches in the leaf
  if (col.doPatchCol) {
    for (i32 leafSurfId = 0; leafSurfId < leaf->numLeafSurfaces; leafSurfId++) {
      cPatch* patch = cm.surfaces[cm.leafsurfaces[leaf->firstLeafSurface + leafSurfId]];
      if (!patch) { continue; }
      if (patch->checkcount == cm.checkcount) {
        continue;  // already checked this patch in another leaf
      }
      patch->checkcount = cm.checkcount;
      if (!(patch->contents & tw->contents)) { continue; }
      CM_TraceThroughPatch(tw, patch);
      if (!tw->trace.fraction) { return; }
    }
  }
}

//..................
// CM_TraceThroughSphere
//   Checks if the given trace data (TraceWork)
//   passes through the given sphere
//   Gets the first intersection of the ray with the sphere
//..................
static void CM_TraceThroughSphere(TraceWork* tw, const vec3 origin, f32 radius, const vec3 start, const vec3 end) {
  // if inside the sphere
  vec3 dir;
  GVec3Sub(start, origin, dir);
  f32 l1 = Vec3LenSq(dir);
  if (l1 < Sqr(radius)) {
    tw->trace.fraction   = 0;
    tw->trace.startsolid = true;
    // test for allsolid
    GVec3Sub(end, origin, dir);
    l1 = Vec3LenSq(dir);
    if (l1 < Sqr(radius)) { tw->trace.allsolid = true; }
    return;
  }
  GVec3Sub(end, start, dir);
  f32 length = Vec3Norm(dir);
  l1         = CM_DistanceFromLineSquared(origin, start, end, dir);
  vec3 v1;
  GVec3Sub(end, origin, v1);
  f32 l2 = Vec3LenSq(v1);
  // if no intersection with the sphere and the end point is at least an epsilon away
  if (l1 >= Sqr(radius) && l2 > Sqr(radius + SURFACE_CLIP_EPSILON)) { return; }
  GVec3Sub(start, origin, v1);

  f32 b = 2.0f * (dir[0] * v1[0] + dir[1] * v1[1] + dir[2] * v1[2]);
  f32 c = v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

  f32 d = b * b - 4.0f * c;  // * a;
  if (d > 0) {
    f32 sqrtd    = f32Sqr(d);
    f32 fraction = (-b - sqrtd) * 0.5f;
    if (fraction < 0) {
      fraction = 0;
    } else {
      fraction /= length;
    }
    if (fraction < tw->trace.fraction) {
      tw->trace.fraction = fraction;
      GVec3Sub(end, start, dir);
      vec3 intersection;
      GVec3MA(start, fraction, dir, intersection);
      GVec3Sub(intersection, origin, dir);
      f32 scale = 1 / (radius + RADIUS_EPSILON);
      GVec3Scale(dir, scale, dir);
      GVec3Copy(dir, tw->trace.plane.normal);
      GVec3Add(tw->modelOrigin, intersection, intersection);
      tw->trace.plane.dist = GVec3Dot(tw->trace.plane.normal, intersection);
      tw->trace.contents   = CONTENTS_BODY;
    }
  } else if (d == 0) {
    // t1 = (- b ) / 2;
    //  slide along the sphere
  }
  // no intersection at all
}


//..................
// CM_TraceThroughVerticalCylinder
//   Gets the first intersection of the ray with the cylinder
//   the cylinder extends halfHeight above and below the origin
//..................
static void CM_TraceThroughVerticalCylinder(TraceWork* tw, const vec3 origin, f32 radius, f32 halfHeight, const vec3 start, const vec3 end) {
  // 2d coordinates
  vec3 dir, start2d, end2d, org2d;
  GVec3Set(start2d, start[0], start[1], 0);
  GVec3Set(end2d, end[0], end[1], 0);
  GVec3Set(org2d, origin[0], origin[1], 0);
  // if between lower and upper cylinder bounds
  if (start[2] <= origin[2] + halfHeight && start[2] >= origin[2] - halfHeight) {
    // if inside the cylinder
    GVec3Sub(start2d, org2d, dir);
    f32 l1 = Vec3LenSq(dir);
    if (l1 < Sqr(radius)) {
      tw->trace.fraction   = 0;
      tw->trace.startsolid = true;
      GVec3Sub(end2d, org2d, dir);
      l1 = Vec3LenSq(dir);
      if (l1 < Sqr(radius)) { tw->trace.allsolid = true; }
      return;
    }
  }
  //
  GVec3Sub(end2d, start2d, dir);
  f32 length = Vec3Norm(dir);
  //
  f32  l1    = CM_DistanceFromLineSquared(org2d, start2d, end2d, dir);
  vec3 v1;
  GVec3Sub(end2d, org2d, v1);
  f32 l2 = Vec3LenSq(v1);
  // if no intersection with the cylinder and the end point is at least an epsilon away
  if (l1 >= Sqr(radius) && l2 > Sqr(radius + SURFACE_CLIP_EPSILON)) { return; }

  GVec3Sub(start, origin, v1);
  f32 b = 2.0f * (v1[0] * dir[0] + v1[1] * dir[1]);
  f32 c = v1[0] * v1[0] + v1[1] * v1[1] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

  f32 d = b * b - 4.0f * c;  // * a;
  if (d > 0) {
    f32 sqrtd    = f32Sqr(d);
    f32 fraction = (-b - sqrtd) * 0.5f;
    //
    if (fraction < 0) {
      fraction = 0;
    } else {
      fraction /= length;
    }
    if (fraction < tw->trace.fraction) {
      GVec3Sub(end, start, dir);
      vec3 intersection;
      GVec3MA(start, fraction, dir, intersection);
      // if the intersection is between the cylinder lower and upper bound
      if (intersection[2] <= origin[2] + halfHeight && intersection[2] >= origin[2] - halfHeight) {
        //
        tw->trace.fraction = fraction;
        GVec3Sub(intersection, origin, dir);
        dir[2]    = 0;
        f32 scale = 1 / (radius + RADIUS_EPSILON);
        GVec3Scale(dir, scale, dir);
        GVec3Copy(dir, tw->trace.plane.normal);
        GVec3Add(tw->modelOrigin, intersection, intersection);
        tw->trace.plane.dist = GVec3Dot(tw->trace.plane.normal, intersection);
        tw->trace.contents   = CONTENTS_BODY;
      }
    }
  } else if (d == 0) {
    // t[0] = (- b ) / 2 * a;
    //  slide along the cylinder
  }
  // no intersection at all
}


//..................
// CM_TraceCapsuleThroughCapsule
//   capsule vs. capsule collision (not rotated)
//..................
static void CM_TraceCapsuleThroughCapsule(TraceWork* tw, cHandle model) {
  vec3 mins, maxs;
  CM_ModelBounds(model, mins, maxs);
  // test trace bounds vs. capsule bounds
  if (tw->bounds[0][0] > maxs[0] + RADIUS_EPSILON || tw->bounds[0][1] > maxs[1] + RADIUS_EPSILON || tw->bounds[0][2] > maxs[2] + RADIUS_EPSILON
      || tw->bounds[1][0] < mins[0] - RADIUS_EPSILON || tw->bounds[1][1] < mins[1] - RADIUS_EPSILON || tw->bounds[1][2] < mins[2] - RADIUS_EPSILON) {
    return;
  }
  // top origin and bottom origin of each sphere at start and end of trace
  vec3 starttop, startbottom, endtop, endbottom;
  GVec3Add(tw->start, tw->sphere.offset, starttop);
  GVec3Sub(tw->start, tw->sphere.offset, startbottom);
  GVec3Add(tw->end, tw->sphere.offset, endtop);
  GVec3Sub(tw->end, tw->sphere.offset, endbottom);

  // calculate top and bottom of the capsule spheres to collide with
  vec3 offset, symetricSize[2];
  for (i32 i = 0; i < 3; i++) {
    offset[i]          = (mins[i] + maxs[i]) * 0.5;
    symetricSize[0][i] = mins[i] - offset[i];
    symetricSize[1][i] = maxs[i] - offset[i];
  }
  f32  halfWidth  = symetricSize[1][0];
  f32  halfHeight = symetricSize[1][2];
  f32  radius     = (halfWidth > halfHeight) ? halfHeight : halfWidth;
  f32  offs       = halfHeight - radius;
  vec3 top, bottom;
  GVec3Copy(offset, top);
  top[2] += offs;
  GVec3Copy(offset, bottom);
  bottom[2] -= offs;
  // expand radius of spheres
  radius += tw->sphere.radius;
  // if there is horizontal movement
  if (tw->start[0] != tw->end[0] || tw->start[1] != tw->end[1]) {
    // height of the expanded cylinder is the height of both cylinders minus the radius of both spheres
    f32 h = halfHeight + tw->sphere.halfHeight - radius;
    // if the cylinder has a height
    if (h > 0) {
      // test for collisions between the cylinders
      CM_TraceThroughVerticalCylinder(tw, offset, radius, h, tw->start, tw->end);
    }
  }
  // test for collision between the spheres
  CM_TraceThroughSphere(tw, top, radius, startbottom, endbottom);
  CM_TraceThroughSphere(tw, bottom, radius, starttop, endtop);
}


//..................
// CM_TraceBoundingBoxThroughCapsule
//   bounding box vs. capsule collision
//..................
static void CM_TraceBoundingBoxThroughCapsule(TraceWork* tw, cHandle model) {
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
  CM_TraceThroughLeaf(tw, &cmod->leaf);
}


//..................
// CM_TraceThroughTree
//   Traverse all the contacted leafs from the start to the end position.
//   If the trace is a point, they will be exactly in order,
//   but for larger trace volumes it is possible to hit something in a later leaf
//   with a smaller intercept fraction.
//..................
static void CM_TraceThroughTree(TraceWork* tw, i32 num, f32 p1f, f32 p2f, const vec3 p1, const vec3 p2) {
  if (tw->trace.fraction <= p1f) { return; }  // already hit something nearer
  // if < 0, we are in a leaf node
  if (num < 0) {
    CM_TraceThroughLeaf(tw, &cm.leafs[-1 - num]);
    return;
  }
  // find the point distances to the separating plane
  // and the offset for the size of the box
  cNode*  node  = cm.nodes + num;
  cPlane* plane = node->plane;
  // adjust the plane distance appropriately for mins/maxs
  f64 t1, t2, offset;
  if (plane->type < 3) {
    t1     = p1[plane->type] - plane->dist;
    t2     = p2[plane->type] - plane->dist;
    offset = tw->extents[plane->type];
  } else {
    t1 = GVec3Dot(plane->normal, p1) - plane->dist;
    t2 = GVec3Dot(plane->normal, p2) - plane->dist;
    if (tw->isPoint) {
      offset = 0;
    } else {
      // this is silly
      offset = 2048;
    }
  }
  // see which sides we need to consider
  if (t1 >= offset + 1 && t2 >= offset + 1) {
    CM_TraceThroughTree(tw, node->children[0], p1f, p2f, p1, p2);
    return;
  }
  if (t1 < -offset - 1 && t2 < -offset - 1) {
    CM_TraceThroughTree(tw, node->children[1], p1f, p2f, p1, p2);
    return;
  }
  // put the crosspoint SURFACE_CLIP_EPSILON pixels on the near side
  f32 idist;
  i32 side;
  f32 frac, frac2;
  if (t1 < t2) {
    idist = 1.0 / (t1 - t2);
    side  = 1;
    frac2 = (t1 + offset + SURFACE_CLIP_EPSILON) * idist;
    frac  = (t1 - offset + SURFACE_CLIP_EPSILON) * idist;
  } else if (t1 > t2) {
    idist = 1.0 / (t1 - t2);
    side  = 0;
    frac2 = (t1 - offset - SURFACE_CLIP_EPSILON) * idist;
    frac  = (t1 + offset + SURFACE_CLIP_EPSILON) * idist;
  } else {
    side  = 0;
    frac  = 1;
    frac2 = 0;
  }
  // move up to the node
  if (frac < 0) {
    frac = 0;
  } else if (frac > 1) {
    frac = 1;
  }
  f32 midf = p1f + (p2f - p1f) * frac;

  vec3 mid;
  mid[0] = p1[0] + frac * (p2[0] - p1[0]);
  mid[1] = p1[1] + frac * (p2[1] - p1[1]);
  mid[2] = p1[2] + frac * (p2[2] - p1[2]);

  CM_TraceThroughTree(tw, node->children[side], p1f, midf, p1, mid);

  // go past the node
  if (frac2 < 0) {
    frac2 = 0;
  } else if (frac2 > 1) {
    frac2 = 1;
  }

  midf   = p1f + (p2f - p1f) * frac2;

  mid[0] = p1[0] + frac2 * (p2[0] - p1[0]);
  mid[1] = p1[1] + frac2 * (p2[1] - p1[1]);
  mid[2] = p1[2] + frac2 * (p2[2] - p1[2]);

  CM_TraceThroughTree(tw, node->children[side ^ 1], midf, p2f, mid, p2);
}


//..................
// CM_Trace
//..................
static void CM_Trace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, const vec3 origin, i32 brushmask,
                     bool capsule, const Sphere* sphere) {
  cModel* cmod = CM_ClipHandleToModel(model);

  cm.checkcount++;  // for multi-check avoidance
  c_traces++;       // for statistics, may be zeroed

  // fill in a default trace
  TraceWork tw;
  memset(&tw, 0, sizeof(tw));
  tw.trace.fraction = 1;  // assume it goes the entire distance until shown otherwise
  GVec3Copy(origin, tw.modelOrigin);

  if (!cm.numNodes) {
    *results = tw.trace;
    return;  // map not loaded, shouldn't happen
  }

  // allow NULL to be passed in for 0,0,0
  if (!mins) { mins = vec3_origin; }
  if (!maxs) { maxs = vec3_origin; }

  // set basic parms
  tw.contents = brushmask;

  // adjust so that mins and maxs are always symetric
  // avoids some complications with plane expanding of rotated bmodels
  vec3 offset;
  for (i32 i = 0; i < 3; i++) {
    offset[i]     = (mins[i] + maxs[i]) * 0.5;
    tw.size[0][i] = mins[i] - offset[i];
    tw.size[1][i] = maxs[i] - offset[i];
    tw.start[i]   = start[i] + offset[i];
    tw.end[i]     = end[i] + offset[i];
  }

  // if a sphere is already specified
  if (sphere) {
    tw.sphere = *sphere;
  } else {
    tw.sphere.use        = capsule;
    tw.sphere.radius     = (tw.size[1][0] > tw.size[1][2]) ? tw.size[1][2] : tw.size[1][0];
    tw.sphere.halfHeight = tw.size[1][2];
    GVec3Set(tw.sphere.offset, 0, 0, tw.size[1][2] - tw.sphere.radius);
  }

  tw.maxOffset     = tw.size[1][0] + tw.size[1][1] + tw.size[1][2];

  // tw.offsets[signbits] = vector to appropriate corner from origin
  tw.offsets[0][0] = tw.size[0][0];
  tw.offsets[0][1] = tw.size[0][1];
  tw.offsets[0][2] = tw.size[0][2];

  tw.offsets[1][0] = tw.size[1][0];
  tw.offsets[1][1] = tw.size[0][1];
  tw.offsets[1][2] = tw.size[0][2];

  tw.offsets[2][0] = tw.size[0][0];
  tw.offsets[2][1] = tw.size[1][1];
  tw.offsets[2][2] = tw.size[0][2];

  tw.offsets[3][0] = tw.size[1][0];
  tw.offsets[3][1] = tw.size[1][1];
  tw.offsets[3][2] = tw.size[0][2];

  tw.offsets[4][0] = tw.size[0][0];
  tw.offsets[4][1] = tw.size[0][1];
  tw.offsets[4][2] = tw.size[1][2];

  tw.offsets[5][0] = tw.size[1][0];
  tw.offsets[5][1] = tw.size[0][1];
  tw.offsets[5][2] = tw.size[1][2];

  tw.offsets[6][0] = tw.size[0][0];
  tw.offsets[6][1] = tw.size[1][1];
  tw.offsets[6][2] = tw.size[1][2];

  tw.offsets[7][0] = tw.size[1][0];
  tw.offsets[7][1] = tw.size[1][1];
  tw.offsets[7][2] = tw.size[1][2];

  //
  // calculate bounds
  //
  if (tw.sphere.use) {
    for (i32 i = 0; i < 3; i++) {
      if (tw.start[i] < tw.end[i]) {
        tw.bounds[0][i] = tw.start[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
        tw.bounds[1][i] = tw.end[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
      } else {
        tw.bounds[0][i] = tw.end[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
        tw.bounds[1][i] = tw.start[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
      }
    }
  } else {
    for (i32 i = 0; i < 3; i++) {
      if (tw.start[i] < tw.end[i]) {
        tw.bounds[0][i] = tw.start[i] + tw.size[0][i];
        tw.bounds[1][i] = tw.end[i] + tw.size[1][i];
      } else {
        tw.bounds[0][i] = tw.end[i] + tw.size[0][i];
        tw.bounds[1][i] = tw.start[i] + tw.size[1][i];
      }
    }
  }

  // check for position test special case
  if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
    if (model) {
#if defined ALWAYS_BBOX_VS_BBOX  // FIXME - compile time flag?
      if (model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE) {
        tw.sphere.use = false;
        CM_TestInLeaf(&tw, &cmod->leaf);
      } else
#elif defined ALWAYS_CAPSULE_VS_CAPSULE
      if (model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE) {
        CM_TestCapsuleInCapsule(&tw, model);
      } else
#endif
        if (model == CAPSULE_MODEL_HANDLE) {
        if (tw.sphere.use) {
          CM_TestCapsuleInCapsule(&tw, model);
        } else {
          CM_TestBoundingBoxInCapsule(&tw, model);
        }
      } else {
        CM_TestInLeaf(&tw, &cmod->leaf);
      }
    } else {
      CM_PositionTest(&tw);
    }
  } else {
    // check for point special case
    if (tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0) {
      tw.isPoint = true;
      GVec3Clear(tw.extents);
    } else {
      tw.isPoint    = false;
      tw.extents[0] = tw.size[1][0];
      tw.extents[1] = tw.size[1][1];
      tw.extents[2] = tw.size[1][2];
    }

    // general sweeping through world
    if (model) {
#if defined ALWAYS_BBOX_VS_BBOX
      if (model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE) {
        tw.sphere.use = false;
        CM_TraceThroughLeaf(&tw, &cmod->leaf);
      } else
#elif defined ALWAYS_CAPSULE_VS_CAPSULE
      if (model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE) {
        CM_TraceCapsuleThroughCapsule(&tw, model);
      } else
#endif
        if (model == CAPSULE_MODEL_HANDLE) {
        if (tw.sphere.use) {
          CM_TraceCapsuleThroughCapsule(&tw, model);
        } else {
          CM_TraceBoundingBoxThroughCapsule(&tw, model);
        }
      } else {
        CM_TraceThroughLeaf(&tw, &cmod->leaf);
      }
    } else {
      CM_TraceThroughTree(&tw, 0, 0, 1, tw.start, tw.end);
    }
  }

  // generate endpos from the original, unmodified start/end
  if (tw.trace.fraction == 1) {
    GVec3Copy(end, tw.trace.endpos);
  } else {
    for (i32 i = 0; i < 3; i++) { tw.trace.endpos[i] = start[i] + tw.trace.fraction * (end[i] - start[i]); }
  }

  // If allsolid is set (was entirely inside something solid), the plane is not valid.
  // If fraction == 1.0, we never hit anything, and thus the plane is not valid.
  // Otherwise, the normal on the plane should have unit length
  assert(tw.trace.allsolid || tw.trace.fraction == 1.0 || Vec3LenSq(tw.trace.plane.normal) > 0.9999);
  *results = tw.trace;
}


//..................
// CM_BoxTrace
//..................
void CM_BoxTrace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, i32 brushmask, bool capsule) {
  CM_Trace(results, start, end, mins, maxs, model, vec3_origin, brushmask, capsule, NULL);
}

//..................
// CM_TransformedBoxTrace
//   Handles offseting and rotation of the end points for moving and rotating entities
//..................
void CM_TransformedBoxTrace(Trace* results, const vec3 start, const vec3 end, const vec3 mins, const vec3 maxs, cHandle model, i32 brushmask, const vec3 origin,
                            const vec3 angles, bool capsule) {
  if (!mins) { mins = vec3_origin; }
  if (!maxs) { maxs = vec3_origin; }

  // adjust so that mins and maxs are always symetric, which
  // avoids some complications with plane expanding of rotated bmodels
  vec3 offset;
  vec3 symetricSize[2];
  vec3 start_l, end_l;
  for (i32 i = 0; i < 3; i++) {
    offset[i]          = (mins[i] + maxs[i]) * 0.5;
    symetricSize[0][i] = mins[i] - offset[i];
    symetricSize[1][i] = maxs[i] - offset[i];
    start_l[i]         = start[i] + offset[i];
    end_l[i]           = end[i] + offset[i];
  }

  // subtract origin offset
  GVec3Sub(start_l, origin, start_l);
  GVec3Sub(end_l, origin, end_l);

  // rotate start and end into the models frame of reference
  bool rotated   = (model != BOX_MODEL_HANDLE && (angles[0] || angles[1] || angles[2]));

  f32 halfWidth  = symetricSize[1][0];
  f32 halfHeight = symetricSize[1][2];

  Sphere sphere;
  sphere.use        = capsule;
  sphere.radius     = (halfWidth > halfHeight) ? halfHeight : halfWidth;
  sphere.halfHeight = halfHeight;
  f32 t             = halfHeight - sphere.radius;

  vec3 matrix[3], transpose[3];
  if (rotated) {
    // rotation on trace line (start-end) instead of rotating the bmodel
    // NOTE: This is still incorrect for bounding boxes because the actual bounding
    //		 box that is swept through the model is not rotated. We cannot rotate
    //		 the bounding box or the bmodel because that would make all the brush
    //		 bevels invalid.
    //		 However this is correct for capsules since a capsule itself is rotated too.
    CreateRotationMatrix(angles, matrix);
    RotatePoint(start_l, matrix);
    RotatePoint(end_l, matrix);
    // rotated sphere offset for capsule
    sphere.offset[0] = matrix[0][2] * t;
    sphere.offset[1] = -matrix[1][2] * t;
    sphere.offset[2] = matrix[2][2] * t;
  } else {
    GVec3Set(sphere.offset, 0, 0, t);
  }

  // sweep the box through the model
  Trace trace;
  CM_Trace(&trace, start_l, end_l, symetricSize[0], symetricSize[1], model, origin, brushmask, capsule, &sphere);

  // if the bmodel was rotated and there was a collision
  if (rotated && trace.fraction != 1.0) {
    // rotation of bmodel collision plane
    TransposeMatrix(matrix, transpose);
    RotatePoint(trace.plane.normal, transpose);
  }

  // re-calculate the end position of the trace because the trace.endpos
  // calculated by CM_Trace could be rotated and have an offset
  trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
  trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
  trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);
  *results        = trace;
}
