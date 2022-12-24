#include "../debug.h"

//............................
// AllocWinding
//............................
static Winding* AllocWinding(i32 points) {
  // Update debug counters
  c_winding_allocs++;
  c_winding_points += points;
  c_active_windings++;
  if (c_active_windings > c_peak_windings) c_peak_windings = c_active_windings;
  // Allocate
  Winding* w;
  size_t   s = sizeof(*w) - sizeof(w->p) + sizeof(w->p[0]) * points;
  w          = Z_Malloc(s);
  Std_memset(w, 0, s);
  return w;
}

//............................
// FreeWinding
//............................
void FreeWinding(Winding* w) {
  if (*(unsigned*)w == 0xdeaddead) err(ERR_EXIT, "%s: freed a freed winding", __func__);
  *(unsigned*)w = 0xdeaddead;
  c_active_windings--;
  Z_Free(w);
}


//............................
// BaseWindingForPlane
//............................
Winding* BaseWindingForPlane(vec3 normal, f32 dist) {
  // find the major axis
  f32 max = -MAX_MAP_BOUNDS;
  i32 x   = -1;
  for (i32 i = 0; i < 3; i++) {
    f32 v = fabs(normal[i]);
    if (v > max) {
      x   = i;
      max = v;
    }
  }
  if (x == -1) err(ERR_DROP, "BaseWindingForPlane: no axis found");

  vec3 vup;
  GVec3Copy(vec3_origin, vup);
  switch (x) {
    case 0:
    case 1: vup[2] = 1; break;
    case 2: vup[0] = 1; break;
  }

  f64 dot = GVec3Dot(vup, normal);
  GVec3MA(vup, -dot, normal, vup);
  DVec3Norm(vup);

  vec3 org;
  GVec3Scale(normal, dist, org);

  vec3 vright;
  DVec3Cross(vup, normal, vright);

  GVec3Scale(vup, MAX_MAP_BOUNDS, vup);
  GVec3Scale(vright, MAX_MAP_BOUNDS, vright);

  // project a really big	axis aligned box onto the plane
  Winding* w = AllocWinding(4);

  GVec3Sub(org, vright, w->p[0]);
  GVec3Add(w->p[0], vup, w->p[0]);

  GVec3Add(org, vright, w->p[1]);
  GVec3Add(w->p[1], vup, w->p[1]);

  GVec3Add(org, vright, w->p[2]);
  GVec3Sub(w->p[2], vup, w->p[2]);

  GVec3Sub(org, vright, w->p[3]);
  GVec3Sub(w->p[3], vup, w->p[3]);

  w->numpoints = 4;
  return w;
}

//............................
// ChopWindingInPlace
//............................
void ChopWindingInPlace(Winding** inout, const vec3 normal, f32 dist, f32 epsilon) {
  f64      dot, d1, d2;
  i32      i, j;
  f32 *    p1, *p2;
  vec3     mid;
  i32      maxpts;
  f32      dists[MAX_POINTS_ON_WINDING + 4];
  i32      sides[MAX_POINTS_ON_WINDING + 4];
  i32      counts[3];
  Winding* in = *inout;
  counts[0] = counts[1] = counts[2] = 0;
  Std_memset(dists, 0, sizeof(dists));
  Std_memset(sides, 0, sizeof(sides));

  // determine sides for each point
  for (i = 0; i < in->numpoints; i++) {
    dot      = DVec3Dotf(in->p[i], normal) - dist;
    // dot -= dist;
    dists[i] = dot;
    if (dot > epsilon) sides[i] = SIDE_FRONT;
    else if (dot < -epsilon) sides[i] = SIDE_BACK;
    else { sides[i] = SIDE_ON; }
    counts[sides[i]]++;
  }
  sides[i] = sides[0];
  dists[i] = dists[0];

  if (!counts[0]) {
    FreeWinding(in);
    *inout = NULL;
    return;
  }
  if (!counts[1]) return;          // inout stays the same
  maxpts     = in->numpoints + 4;  // cant use counts[0]+2 because of fp grouping errors

  Winding* f = AllocWinding(maxpts);
  for (i = 0; i < in->numpoints; i++) {
    p1 = in->p[i];
    if (sides[i] == SIDE_ON) {
      GVec3Copy(p1, f->p[f->numpoints]);
      f->numpoints++;
      continue;
    }

    if (sides[i] == SIDE_FRONT) {
      GVec3Copy(p1, f->p[f->numpoints]);
      f->numpoints++;
    }

    if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i]) continue;

    // generate a split point
    p2  = in->p[(i + 1) % in->numpoints];
    d1  = dists[i];
    d2  = dists[i + 1];
    dot = d1 / (d1 - d2);

    for (j = 0; j < 3; j++) {  // avoid round off error when possible
      if (normal[j] == 1.0) mid[j] = dist;
      else if (normal[j] == -1.0) mid[j] = -dist;
      else {
        d1     = p1[j];
        d2     = p2[j];
        mid[j] = d1 + dot * (d2 - d1);
      }
    }

    GVec3Copy(mid, f->p[f->numpoints]);
    f->numpoints++;
  }
  if (f->numpoints > maxpts) err(ERR_DROP, "ClipWinding: points exceeded estimate");
  if (f->numpoints > MAX_POINTS_ON_WINDING) err(ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");
  FreeWinding(in);
  *inout = f;
}

//............................
// WindingBounds
//............................
void WindingBounds(const Winding* w, vec3 mins, vec3 maxs) {
  mins[0] = mins[1] = mins[2] = MAX_MAP_BOUNDS;
  maxs[0] = maxs[1] = maxs[2] = -MAX_MAP_BOUNDS;
  for (i32 i = 0; i < w->numpoints; i++) {
    for (i32 j = 0; j < 3; j++) {
      f32 v = w->p[i][j];
      if (v < mins[j]) mins[j] = v;
      if (v > maxs[j]) maxs[j] = v;
    }
  }
}


//............................
// CM_ValidateFacet
//   If the facet isn't bounded by its borders, we screwed up.
//............................
bool CM_ValidateFacet(const Facet* facet) {
  if (facet->surfacePlane == -1) { return false; }
  f32 plane[4];
  GVec4Copy(planes[facet->surfacePlane].plane, plane);
  Winding* w = BaseWindingForPlane(plane, plane[3]);
  for (i32 j = 0; j < facet->numBorders && w; j++) {
    if (facet->borderPlanes[j] == -1) {
      FreeWinding(w);
      return false;
    }
    GVec4Copy(planes[facet->borderPlanes[j]].plane, plane);
    if (!facet->borderInward[j]) {
      GVec3Sub(vec3_origin, plane, plane);
      plane[3] = -plane[3];
    }
    ChopWindingInPlace(&w, plane, plane[3], 0.1f);
  }

  if (!w) {
    return false;  // winding was completely chopped away
  }

  // see if the facet is unreasonably large
  vec3 bounds[2];
  WindingBounds(w, bounds[0], bounds[1]);
  FreeWinding(w);

  for (i32 j = 0; j < 3; j++) {
    if (bounds[1][j] - bounds[0][j] > MAX_MAP_BOUNDS) {
      return false;  // we must be missing a plane
    }
    if (bounds[0][j] >= MAX_MAP_BOUNDS) { return false; }
    if (bounds[1][j] <= -MAX_MAP_BOUNDS) { return false; }
  }
  return true;  // winding is fine
}

//............................
// CM_SnapVector
//............................
static void CM_SnapVector(vec3 normal) {
  for (i32 i = 0; i < 3; i++) {
    if (fabs(normal[i] - 1) < NORMAL_EPSILON) {
      GVec3Clear(normal);
      normal[i] = 1;
      break;
    }
    if (fabs(normal[i] - -1) < NORMAL_EPSILON) {
      GVec3Clear(normal);
      normal[i] = -1;
      break;
    }
  }
}

//............................
// CopyWinding
//............................
Winding* CopyWinding(const Winding* w) {
  Winding* c    = AllocWinding(w->numpoints);
  size_t   size = sizeof(*w) - sizeof(w->p) + sizeof(w->p[0]) * w->numpoints;
  Std_memcpy(c, w, size);
  return c;
}

//............................
// CM_AddFacetBevels
//............................
bool CM_PlaneEqual(const PatchPlane* p, const f32 plane[4], i32* flipped);
i32  CM_FindPlane2(const f32 plane[4], i32* flipped);
//............................
void CM_AddFacetBevels(Facet* facet) {
  f32 plane[4], newplane[4];
  GVec4Copy(planes[facet->surfacePlane].plane, plane);

  Winding* w = BaseWindingForPlane(plane, plane[3]);
  i32      i, j, k, l;
  for (j = 0; j < facet->numBorders && w; j++) {
    if (facet->borderPlanes[j] == facet->surfacePlane) continue;
    GVec4Copy(planes[facet->borderPlanes[j]].plane, plane);

    if (!facet->borderInward[j]) {
      GVec3Sub(vec3_origin, plane, plane);
      plane[3] = -plane[3];
    }
    ChopWindingInPlace(&w, plane, plane[3], 0.1f);
  }
  if (!w) { return; }
  vec3 mins, maxs, vec, vec2;
  WindingBounds(w, mins, maxs);
  // add the axial planes
  i32 flipped;
  i32 order = 0;
  for (i32 axis = 0; axis < 3; axis++) {
    for (i32 dir = -1; dir <= 1; dir += 2, order++) {
      GVec3Clear(plane);
      plane[axis] = dir;
      if (dir == 1) {
        plane[3] = maxs[axis];
      } else {
        plane[3] = -mins[axis];
      }
      // if it's the surface plane
      if (CM_PlaneEqual(&planes[facet->surfacePlane], plane, &flipped)) { continue; }
      // see if the plane is already present
      for (i = 0; i < facet->numBorders; i++) {
        if (CM_PlaneEqual(&planes[facet->borderPlanes[i]], plane, &flipped)) break;
      }

      if (i == facet->numBorders) {
        if (facet->numBorders >= 4 + 6 + 16) {
          echo("ERROR: too many bevels");
          continue;
        }
        facet->borderPlanes[facet->numBorders]   = CM_FindPlane2(plane, &flipped);
        facet->borderNoAdjust[facet->numBorders] = 0;
        facet->borderInward[facet->numBorders]   = flipped;
        facet->numBorders++;
      }
    }
  }
  // add the edge bevels
  //
  // test the non-axial plane edges
  f64 d, d1[3], d2[3];
  for (j = 0; j < w->numpoints; j++) {
    k = (j + 1) % w->numpoints;
    GVec3Copy(w->p[j], d1);
    GVec3Copy(w->p[k], d2);
    GVec3Sub(d1, d2, vec);
    // if it's a degenerate edge
    if (DVec3Norm(vec) < 0.5) continue;
    CM_SnapVector(vec);
    for (k = 0; k < 3; k++)
      if (vec[k] == -1 || vec[k] == 1) break;  // axial
    if (k < 3) continue;                       // only test non-axial edges

    // try the six possible slanted axials from this edge
    for (i32 axis = 0; axis < 3; axis++) {
      for (i32 dir = -1; dir <= 1; dir += 2) {
        // construct a plane
        GVec3Clear(vec2);
        vec2[axis] = dir;
        DVec3Cross(vec, vec2, plane);
        if (DVec3Norm(plane) < 0.5) continue;
        plane[3] = DVec3Dotf(w->p[j], plane);

        // if all the points of the facet winding are
        // behind this plane, it is a proper edge bevel
        for (l = 0; l < w->numpoints; l++) {
          d = DVec3Dotf(w->p[l], plane) - plane[3];
          if (d > 0.1) break;  // point in front
        }
        if (l < w->numpoints) continue;

        // if it's the surface plane
        if (CM_PlaneEqual(&planes[facet->surfacePlane], plane, &flipped)) { continue; }
        // see if the plane is already present
        for (i = 0; i < facet->numBorders; i++) {
          if (CM_PlaneEqual(&planes[facet->borderPlanes[i]], plane, &flipped)) { break; }
        }

        if (i == facet->numBorders) {
          if (facet->numBorders >= 4 + 6 + 16) {
            echo("ERROR: too many bevels");
            continue;
          }
          facet->borderPlanes[facet->numBorders] = CM_FindPlane2(plane, &flipped);

          for (k = 0; k < facet->numBorders; k++) {
            if (facet->borderPlanes[facet->numBorders] == facet->borderPlanes[k]) echo("WARNING: bevel plane already used");
          }

          facet->borderNoAdjust[facet->numBorders] = 0;
          facet->borderInward[facet->numBorders]   = flipped;
          //
          Winding* w2                              = CopyWinding(w);
          GVec4Copy(planes[facet->borderPlanes[facet->numBorders]].plane, newplane);
          if (!facet->borderInward[facet->numBorders]) {
            GVec3Neg(newplane, newplane);
            newplane[3] = -newplane[3];
          }  // end if
          ChopWindingInPlace(&w2, newplane, newplane[3], 0.1f);
          if (!w2) {
            if (load.developer) echo("WARNING: CM_AddFacetBevels... invalid bevel");
            continue;
          } else {
            FreeWinding(w2);
          }
          //
          facet->numBorders++;
          // already got a bevel
          //					break;
        }
      }
    }
  }
  FreeWinding(w);
  // add opposite plane
  if (facet->numBorders >= 4 + 6 + 16) {
    echo("ERROR: too many bevels");
    return;
  }
  facet->borderPlanes[facet->numBorders]   = facet->surfacePlane;
  facet->borderNoAdjust[facet->numBorders] = 0;
  facet->borderInward[facet->numBorders]   = true;
  facet->numBorders++;
}

// ...................
// Debug Rendering
// ...................
// BotDrawDebugPolygons
//   TODO: fix. Call to this function is commented out
// ...................
void BotDrawDebugPolygons(void (*drawPoly)(i32 color, i32 numPoints, f32* points), i32 value);
// ...................
// CM_DrawDebugSurface
//   Called from the renderer
// ...................
void CM_DrawDebugSurface(void (*drawPoly)(i32 color, i32 numPoints, f32* points)) {
  if (!col.dbg.surf) { col.dbg.surf = 0; }
  // if (col.dbg.surf != 1) {
  //   BotDrawDebugPolygons(drawPoly, col.dbg.surf);
  //   return;
  // }
  if (!debugPatchCollide) { return; }

  vec3 mins = { -15, -15, -28 }, maxs = { 15, 15, 28 };
  // vec3 mins = {0, 0, 0}, maxs = {0, 0, 0};
  vec3 v1, v2;

  if (!col.dbg.size) { col.dbg.size = 2; }
  const PatchCol* pc = debugPatchCollide;

  Facet* facet;
  i32    i, j, k, n;
  f32    plane[4];
  i32    curplanenum, planenum, curinward, inward;
  for (i = 0, facet = pc->facets; i < pc->numFacets; i++, facet++) {
    for (k = 0; k < facet->numBorders + 1; k++) {
      //
      if (k < facet->numBorders) {
        planenum = facet->borderPlanes[k];
        inward   = facet->borderInward[k];
      } else {
        planenum = facet->surfacePlane;
        inward   = false;
        // continue;
      }

      GVec4Copy(pc->planes[planenum].plane, plane);

      // planenum = facet->surfacePlane;
      if (inward) {
        GVec3Sub(vec3_origin, plane, plane);
        plane[3] = -plane[3];
      }

      plane[3] += col.dbg.size;
      //*
      for (n = 0; n < 3; n++) {
        if (plane[n] > 0) v1[n] = maxs[n];
        else v1[n] = mins[n];
      }  // end for
      GVec3Neg(plane, v2);
      plane[3] += fabs(GVec3Dot(v1, v2));
      //*/

      Winding* w = BaseWindingForPlane(plane, plane[3]);
      for (j = 0; j < facet->numBorders + 1 && w; j++) {
        //
        if (j < facet->numBorders) {
          curplanenum = facet->borderPlanes[j];
          curinward   = facet->borderInward[j];
        } else {
          curplanenum = facet->surfacePlane;
          curinward   = false;
          // continue;
        }
        //
        if (curplanenum == planenum) continue;

        GVec4Copy(pc->planes[curplanenum].plane, plane);
        if (!curinward) {
          GVec3Sub(vec3_origin, plane, plane);
          plane[3] = -plane[3];
        }
        //			if ( !facet->borderNoAdjust[j] ) {
        plane[3] -= col.dbg.size;
        //			}
        for (n = 0; n < 3; n++) {
          if (plane[n] > 0) v1[n] = maxs[n];
          else v1[n] = mins[n];
        }  // end for
        GVec3Neg(plane, v2);
        plane[3] -= fabs(GVec3Dot(v1, v2));

        ChopWindingInPlace(&w, plane, plane[3], 0.1f);
      }
      if (w) {
        if (facet == debugFacet) {
          drawPoly(4, w->numpoints, w->p[0]);
          // echo("blue facet has %d border planes", facet->numBorders);
        } else {
          drawPoly(1, w->numpoints, w->p[0]);
        }
        FreeWinding(w);
      } else echo("winding chopped away by border planes");
    }
  }

  // draw the debug block
  {
    vec3 v[3];
    GVec3Copy(debugBlockPoints[0], v[0]);
    GVec3Copy(debugBlockPoints[1], v[1]);
    GVec3Copy(debugBlockPoints[2], v[2]);
    drawPoly(2, 3, v[0]);
    GVec3Copy(debugBlockPoints[2], v[0]);
    GVec3Copy(debugBlockPoints[3], v[1]);
    GVec3Copy(debugBlockPoints[0], v[2]);
    drawPoly(2, 3, v[0]);
  }
}
