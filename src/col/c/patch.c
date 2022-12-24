#include "../patch.h"

/*

This file does not reference any globals, and has these entry points:

void CM_ClearLevelPatches( void );
struct patchCollide_s *CM_GeneratePatchCollide( i32 width, i32 height, const vec3 *points );
void CM_TraceThroughPatchCollide( TraceWork *tw, const struct patchCollide_s *pc );
bool CM_PositionTestInPatchCollide( TraceWork *tw, const struct patchCollide_s *pc );
void CM_DrawDebugSurface( void (*drawPoly)(i32 color, i32 numPoints, flaot *points) );


Issues for collision against curved surfaces:

Surface edges need to be handled differently than surface planes

Plane expansion causes raw surfaces to expand past expanded bounding box

Position test of a volume against a surface is tricky.

Position test of a point against a surface is not well defined,
because the surface has no volume.

Tracing leading edge points instead of volumes?
Position test by tracing corner to corner? (8*7 traces -- ouch)

coplanar edges
triangulated patches
degenerate patches

  endcaps
  degenerate

WARNING: this may misbehave with meshes that have rows or columns that only
degenerate a few triangles.  Completely degenerate rows and columns are handled
properly.
*/

//............................
// CM_SetGridWrapWidth
//   If the left and right columns are exactly equal, set grid->wrapWidth true
//............................
void CM_SetGridWrapWidth(cGrid* grid) {
  i32 i, j;
  for (i = 0; i < grid->height; i++) {
    for (j = 0; j < 3; j++) {
      f32 d = grid->points[0][i][j] - grid->points[grid->width - 1][i][j];
      if (d < -WRAP_POINT_EPSILON || d > WRAP_POINT_EPSILON) { break; }
    }
    if (j != 3) { break; }
  }
  if (i == grid->height) {
    grid->wrapWidth = true;
  } else {
    grid->wrapWidth = false;
  }
}

//............................
// Grid Subdivision
//............................

//............................
// CM_NeedsSubdivision
//   Returns true if the given quadratic curve is not flat enough
//   for our collision detection purposes
//............................
bool CM_NeedsSubdivision(const vec3 a, const vec3 b, const vec3 c) {
  vec3 cmid, lmid, delta;
  // calculate the linear midpoint
  for (i32 i = 0; i < 3; i++) { lmid[i] = 0.5 * (a[i] + c[i]); }
  // calculate the exact curve midpoint
  for (i32 i = 0; i < 3; i++) { cmid[i] = 0.5 * (0.5 * (a[i] + b[i]) + 0.5 * (b[i] + c[i])); }
  // see if the curve is far enough away from the linear mid
  GVec3Sub(cmid, lmid, delta);
  f32 dist = Vec3Len(delta);
  return dist >= SUBDIVIDE_DISTANCE;
}

//............................
// CM_TransposeGrid
//   Swaps the rows and columns in place
//............................
void CM_TransposeGrid(cGrid* grid) {
  vec3 temp;
  i32  i, j, l;
  if (grid->width > grid->height) {
    for (i = 0; i < grid->height; i++) {
      for (j = i + 1; j < grid->width; j++) {
        if (j < grid->height) {  // swap the value
          GVec3Copy(grid->points[i][j], temp);
          GVec3Copy(grid->points[j][i], grid->points[i][j]);
          GVec3Copy(temp, grid->points[j][i]);
        } else {  // just copy
          GVec3Copy(grid->points[j][i], grid->points[i][j]);
        }
      }
    }
  } else {
    for (i = 0; i < grid->width; i++) {
      for (j = i + 1; j < grid->height; j++) {
        if (j < grid->width) {  // swap the value
          GVec3Copy(grid->points[j][i], temp);
          GVec3Copy(grid->points[i][j], grid->points[j][i]);
          GVec3Copy(temp, grid->points[i][j]);
        } else {  // just copy
          GVec3Copy(grid->points[i][j], grid->points[j][i]);
        }
      }
    }
  }
  l                = grid->width;
  grid->width      = grid->height;
  grid->height     = l;
  bool tempWrap    = grid->wrapWidth;
  grid->wrapWidth  = grid->wrapHeight;
  grid->wrapHeight = tempWrap;
}


//............................
// CM_Subdivide
//   a, b, and c are control points.
//   the subdivided sequence will be: a, out1, out2, out3, c
//............................
void CM_Subdivide(const vec3 a, const vec3 b, const vec3 c, vec3 out1, vec3 out2, vec3 out3) {
  for (i32 i = 0; i < 3; i++) {
    out1[i] = 0.5 * (a[i] + b[i]);
    out3[i] = 0.5 * (b[i] + c[i]);
    out2[i] = 0.5 * (out1[i] + out3[i]);
  }
}


//............................
// CM_SubdivideGridColumns
//   Adds columns as necessary to the grid until
//   all the aproximating points are within SUBDIVIDE_DISTANCE
//   from the true curve
//............................
void CM_SubdivideGridColumns(cGrid* grid) {
  i32 i, j, k;
  for (i = 0; i < grid->width - 2;) {
    // grid->points[i][x] is an interpolating control point
    // grid->points[i+1][x] is an aproximating control point
    // grid->points[i+2][x] is an interpolating control point
    //
    // first see if we can collapse the aproximating column away
    for (j = 0; j < grid->height; j++) {
      if (CM_NeedsSubdivision(grid->points[i][j], grid->points[i + 1][j], grid->points[i + 2][j])) { break; }
    }
    if (j == grid->height) {
      // all of the points were close enough to the linear midpoints
      // that we can collapse the entire column away
      for (j = 0; j < grid->height; j++) {
        // remove the column
        for (k = i + 2; k < grid->width; k++) { GVec3Copy(grid->points[k][j], grid->points[k - 1][j]); }
      }
      grid->width--;
      // go to the next curve segment
      i++;
      continue;
    }
    // we need to subdivide the curve
    for (j = 0; j < grid->height; j++) {
      vec3 prev, mid, next;
      // save the control points now
      GVec3Copy(grid->points[i][j], prev);
      GVec3Copy(grid->points[i + 1][j], mid);
      GVec3Copy(grid->points[i + 2][j], next);

      // make room for two additional columns in the grid
      // columns i+1 will be replaced, column i+2 will become i+4
      // i+1, i+2, and i+3 will be generated
      for (k = grid->width - 1; k > i + 1; k--) { GVec3Copy(grid->points[k][j], grid->points[k + 2][j]); }

      // generate the subdivided points
      CM_Subdivide(prev, mid, next, grid->points[i + 1][j], grid->points[i + 2][j], grid->points[i + 3][j]);
    }
    grid->width += 2;
    // the new aproximating point at i+1 may need to be removed
    // or subdivided farther, so don't advance i
  }
}

//............................
// CM_ComparePoints
//............................
bool CM_ComparePoints(const f32* a, const f32* b) {
  f32 d = a[0] - b[0];
  if (d < -GRID_POINT_EPSILON || d > GRID_POINT_EPSILON) { return false; }
  d = a[1] - b[1];
  if (d < -GRID_POINT_EPSILON || d > GRID_POINT_EPSILON) { return false; }
  d = a[2] - b[2];
  if (d < -GRID_POINT_EPSILON || d > GRID_POINT_EPSILON) { return false; }
  return true;
}

//............................
// CM_RemoveDegenerateColumns
//   If there are any identical columns, remove them
//............................
void CM_RemoveDegenerateColumns(cGrid* grid) {
  i32 i, j, k;
  for (i = 0; i < grid->width - 1; i++) {
    for (j = 0; j < grid->height; j++) {
      if (!CM_ComparePoints(grid->points[i][j], grid->points[i + 1][j])) { break; }
    }
    if (j != grid->height) { continue; }  // not degenerate
    for (j = 0; j < grid->height; j++) {
      // remove the column
      for (k = i + 2; k < grid->width; k++) { GVec3Copy(grid->points[k][j], grid->points[k - 1][j]); }
    }
    grid->width--;
    // check against the next column
    i--;
  }
}

//............................
// CM_GridPlane
//............................
static i32 CM_GridPlane(i32 gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], i32 i, i32 j, i32 tri) {
  i32 p = gridPlanes[i][j][tri];
  if (p != -1) { return p; }
  p = gridPlanes[i][j][!tri];
  if (p != -1) { return p; }
  // should never happen
  echo("WARNING: %s unresolvable", __func__);
  return -1;
}

//............................
// CM_PlaneFromPoints
// Returns false if the triangle is degenrate.
// The normal will point out of the clock for clockwise ordered points
//............................
static bool CM_PlaneFromPoints(vec4 plane, const vec3 a, const vec3 b, const vec3 c) {
  vec3 d1, d2;
  GVec3Sub(b, a, d1);
  GVec3Sub(c, a, d2);
  DVec3Cross(d2, d1, plane);
  if (DVec3Norm(plane) == 0.0) return false;
  plane[3] = DVec3Dotf(a, plane);
  return true;
}

//............................
// CM_SignbitsForNormal
//............................
static i32 CM_SignbitsForNormal(const vec3 normal) {
  i32 bits = 0;
  for (i32 j = 0; j < 3; j++) {
    if (normal[j] < 0) { bits |= 1 << j; }
  }
  return bits;
}

//............................
// CM_FindPlane
//............................
static i32 CM_FindPlane(const f32* p1, const f32* p2, const f32* p3) {
  f32 plane[4];
  if (!CM_PlaneFromPoints(plane, p1, p2, p3)) { return -1; }
  // see if the points are close enough to an existing plane
  for (i32 i = 0; i < numPlanes; i++) {
    if (GVec3Dot(plane, planes[i].plane) < 0) { continue; }  // allow backwards planes?
    f32 d = GVec3Dot(p1, planes[i].plane) - planes[i].plane[3];
    if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) { continue; }
    d = GVec3Dot(p2, planes[i].plane) - planes[i].plane[3];
    if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) { continue; }
    d = GVec3Dot(p3, planes[i].plane) - planes[i].plane[3];
    if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) { continue; }
    // found it
    return i;
  }
  // add a new plane
  if (numPlanes == MAX_PATCH_PLANES) { err(ERR_DROP, "MAX_PATCH_PLANES"); }
  GVec4Copy(plane, planes[numPlanes].plane);
  planes[numPlanes].signbits = CM_SignbitsForNormal(plane);
  numPlanes++;
  return numPlanes - 1;
}


//............................
// CM_EdgePlaneNum
//............................
static i32 CM_EdgePlaneNum(const cGrid* grid, i32 gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], i32 i, i32 j, i32 k) {
  const f32 *p1, *p2;
  vec3       up;
  i32        p;
  switch (k) {
    case 0:  // top border
      p1 = grid->points[i][j];
      p2 = grid->points[i + 1][j];
      p  = CM_GridPlane(gridPlanes, i, j, 0);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p1, p2, up);

    case 2:  // bottom border
      p1 = grid->points[i][j + 1];
      p2 = grid->points[i + 1][j + 1];
      p  = CM_GridPlane(gridPlanes, i, j, 1);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p2, p1, up);

    case 3:  // left border
      p1 = grid->points[i][j];
      p2 = grid->points[i][j + 1];
      p  = CM_GridPlane(gridPlanes, i, j, 1);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p2, p1, up);

    case 1:  // right border
      p1 = grid->points[i + 1][j];
      p2 = grid->points[i + 1][j + 1];
      p  = CM_GridPlane(gridPlanes, i, j, 0);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p1, p2, up);

    case 4:  // diagonal out of triangle 0
      p1 = grid->points[i + 1][j + 1];
      p2 = grid->points[i][j];
      p  = CM_GridPlane(gridPlanes, i, j, 0);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p1, p2, up);

    case 5:  // diagonal out of triangle 1
      p1 = grid->points[i][j];
      p2 = grid->points[i + 1][j + 1];
      p  = CM_GridPlane(gridPlanes, i, j, 1);
      if (p == -1) { return -1; }
      GVec3MA(p1, 4, planes[p].plane, up);
      return CM_FindPlane(p1, p2, up);
  }
  err(ERR_DROP, "%s: bad k", __func__);
  return -1;
}

//............................
// CM_PointOnPlaneSide
//............................
static i32 CM_PointOnPlaneSide(const f32* p, i32 planeNum) {
  if (planeNum == -1) { return SIDE_ON; }
  const f32* plane = planes[planeNum].plane;
  f64        dot   = DVec3Dotf(p, plane) - plane[3];
  if (dot > PLANE_TRI_EPSILON) { return SIDE_FRONT; }
  if (dot < -PLANE_TRI_EPSILON) { return SIDE_BACK; }
  return SIDE_ON;
}

//............................
// CM_SetBorderInward
//............................
static void CM_SetBorderInward(Facet* facet, const cGrid* grid, i32 gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], i32 i, i32 j, i32 which) {
  const f32* points[4];
  i32        numPoints;
  switch (which) {
    case -1:
      points[0] = grid->points[i][j];
      points[1] = grid->points[i + 1][j];
      points[2] = grid->points[i + 1][j + 1];
      points[3] = grid->points[i][j + 1];
      numPoints = 4;
      break;
    case 0:
      points[0] = grid->points[i][j];
      points[1] = grid->points[i + 1][j];
      points[2] = grid->points[i + 1][j + 1];
      numPoints = 3;
      break;
    case 1:
      points[0] = grid->points[i + 1][j + 1];
      points[1] = grid->points[i][j + 1];
      points[2] = grid->points[i][j];
      numPoints = 3;
      break;
    default:
      err(ERR_EXIT, "CM_SetBorderInward: bad parameter");
      numPoints = 0;
      break;
  }

  for (i32 k = 0; k < facet->numBorders; k++) {
    i32 front = 0;
    i32 back  = 0;

    for (i32 l = 0; l < numPoints; l++) {
      i32 side = CM_PointOnPlaneSide(points[l], facet->borderPlanes[k]);
      if (side == SIDE_FRONT) {
        front++;
      } else if (side == SIDE_BACK) {
        back++;
      }
    }

    if (front && !back) {
      facet->borderInward[k] = true;
    } else if (back && !front) {
      facet->borderInward[k] = false;
    } else if (!front && !back) {
      // flat side border
      facet->borderPlanes[k] = -1;
    } else {
      // bisecting side border
      echo("WARNING: CM_SetBorderInward: mixed plane sides");
      facet->borderInward[k] = false;
      if (!debugBlock) {
        debugBlock = true;
        GVec3Copy(grid->points[i][j], debugBlockPoints[0]);
        GVec3Copy(grid->points[i + 1][j], debugBlockPoints[1]);
        GVec3Copy(grid->points[i + 1][j + 1], debugBlockPoints[2]);
        GVec3Copy(grid->points[i][j + 1], debugBlockPoints[3]);
      }
    }
  }
}

//............................
// CM_PlaneEqual
//............................
bool CM_PlaneEqual(const PatchPlane* p, const f32 plane[4], i32* flipped) {
  if (fabs(p->plane[0] - plane[0]) < NORMAL_EPSILON && fabs(p->plane[1] - plane[1]) < NORMAL_EPSILON && fabs(p->plane[2] - plane[2]) < NORMAL_EPSILON
      && fabs(p->plane[3] - plane[3]) < DIST_EPSILON) {
    *flipped = false;
    return true;
  }
  f32 invplane[4];
  GVec3Neg(plane, invplane);
  invplane[3] = -plane[3];
  if (fabs(p->plane[0] - invplane[0]) < NORMAL_EPSILON && fabs(p->plane[1] - invplane[1]) < NORMAL_EPSILON && fabs(p->plane[2] - invplane[2]) < NORMAL_EPSILON
      && fabs(p->plane[3] - invplane[3]) < DIST_EPSILON) {
    *flipped = true;
    return true;
  }
  return false;
}



//............................
// CM_FindPlane2
//............................
i32 CM_FindPlane2(const f32 plane[4], i32* flipped) {
  // see if the points are close enough to an existing plane
  for (i32 i = 0; i < numPlanes; i++) {
    if (CM_PlaneEqual(&planes[i], plane, flipped)) return i;
  }
  // add a new plane
  if (numPlanes == MAX_PATCH_PLANES) { err(ERR_DROP, "MAX_PATCH_PLANES"); }
  GVec4Copy(plane, planes[numPlanes].plane);
  planes[numPlanes].signbits = CM_SignbitsForNormal(plane);
  numPlanes++;
  *flipped = false;
  return numPlanes - 1;
}



//............................
// CM_PatchCollideFromGrid
//............................
void CM_PatchCollideFromGrid(const cGrid* grid, PatchCol* pf) {
  // Clear the stored state
  numPlanes = 0;
  numFacets = 0;

  const f32 *p1, *p2, *p3;
  i32        gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2];
  // find the planes for each triangle of the grid
  i32 i, j;
  for (i = 0; i < grid->width - 1; i++) {
    for (j = 0; j < grid->height - 1; j++) {
      p1                  = grid->points[i][j];
      p2                  = grid->points[i + 1][j];
      p3                  = grid->points[i + 1][j + 1];
      gridPlanes[i][j][0] = CM_FindPlane(p1, p2, p3);

      p1                  = grid->points[i + 1][j + 1];
      p2                  = grid->points[i][j + 1];
      p3                  = grid->points[i][j];
      gridPlanes[i][j][1] = CM_FindPlane(p1, p2, p3);
    }
  }

  Facet* facet;
  i32    borders[4];
  bool   noAdjust[4];
  // create the borders for each facet
  for (i = 0; i < grid->width - 1; i++) {
    for (j = 0; j < grid->height - 1; j++) {
      borders[EN_TOP] = -1;
      if (j > 0) {
        borders[EN_TOP] = gridPlanes[i][j - 1][1];
      } else if (grid->wrapHeight) {
        borders[EN_TOP] = gridPlanes[i][grid->height - 2][1];
      }
      noAdjust[EN_TOP] = (borders[EN_TOP] == gridPlanes[i][j][0]);
      if (borders[EN_TOP] == -1 || noAdjust[EN_TOP]) { borders[EN_TOP] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 0); }

      borders[EN_BOTTOM] = -1;
      if (j < grid->height - 2) {
        borders[EN_BOTTOM] = gridPlanes[i][j + 1][0];
      } else if (grid->wrapHeight) {
        borders[EN_BOTTOM] = gridPlanes[i][0][0];
      }
      noAdjust[EN_BOTTOM] = (borders[EN_BOTTOM] == gridPlanes[i][j][1]);
      if (borders[EN_BOTTOM] == -1 || noAdjust[EN_BOTTOM]) { borders[EN_BOTTOM] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 2); }

      borders[EN_LEFT] = -1;
      if (i > 0) {
        borders[EN_LEFT] = gridPlanes[i - 1][j][0];
      } else if (grid->wrapWidth) {
        borders[EN_LEFT] = gridPlanes[grid->width - 2][j][0];
      }
      noAdjust[EN_LEFT] = (borders[EN_LEFT] == gridPlanes[i][j][1]);
      if (borders[EN_LEFT] == -1 || noAdjust[EN_LEFT]) { borders[EN_LEFT] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 3); }

      borders[EN_RIGHT] = -1;
      if (i < grid->width - 2) {
        borders[EN_RIGHT] = gridPlanes[i + 1][j][1];
      } else if (grid->wrapWidth) {
        borders[EN_RIGHT] = gridPlanes[0][j][1];
      }
      noAdjust[EN_RIGHT] = (borders[EN_RIGHT] == gridPlanes[i][j][0]);
      if (borders[EN_RIGHT] == -1 || noAdjust[EN_RIGHT]) { borders[EN_RIGHT] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 1); }

      if (numFacets == MAX_FACETS) { err(ERR_DROP, "MAX_FACETS"); }
      facet = &facets[numFacets];
      Std_memset(facet, 0, sizeof(*facet));

      if (gridPlanes[i][j][0] == gridPlanes[i][j][1]) {
        if (gridPlanes[i][j][0] == -1) { continue; }  // degenrate
        facet->surfacePlane      = gridPlanes[i][j][0];
        facet->numBorders        = 4;
        facet->borderPlanes[0]   = borders[EN_TOP];
        facet->borderNoAdjust[0] = noAdjust[EN_TOP];
        facet->borderPlanes[1]   = borders[EN_RIGHT];
        facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
        facet->borderPlanes[2]   = borders[EN_BOTTOM];
        facet->borderNoAdjust[2] = noAdjust[EN_BOTTOM];
        facet->borderPlanes[3]   = borders[EN_LEFT];
        facet->borderNoAdjust[3] = noAdjust[EN_LEFT];
        CM_SetBorderInward(facet, grid, gridPlanes, i, j, -1);
        if (CM_ValidateFacet(facet)) {
          CM_AddFacetBevels(facet);
          numFacets++;
        }
      } else {
        // two separate triangles
        facet->surfacePlane      = gridPlanes[i][j][0];
        facet->numBorders        = 3;
        facet->borderPlanes[0]   = borders[EN_TOP];
        facet->borderNoAdjust[0] = noAdjust[EN_TOP];
        facet->borderPlanes[1]   = borders[EN_RIGHT];
        facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
        facet->borderPlanes[2]   = gridPlanes[i][j][1];
        if (facet->borderPlanes[2] == -1) {
          facet->borderPlanes[2] = borders[EN_BOTTOM];
          if (facet->borderPlanes[2] == -1) { facet->borderPlanes[2] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 4); }
        }
        CM_SetBorderInward(facet, grid, gridPlanes, i, j, 0);
        if (CM_ValidateFacet(facet)) {
          CM_AddFacetBevels(facet);
          numFacets++;
        }

        if (numFacets == MAX_FACETS) { err(ERR_DROP, "MAX_FACETS"); }
        facet = &facets[numFacets];
        Std_memset(facet, 0, sizeof(*facet));

        facet->surfacePlane      = gridPlanes[i][j][1];
        facet->numBorders        = 3;
        facet->borderPlanes[0]   = borders[EN_BOTTOM];
        facet->borderNoAdjust[0] = noAdjust[EN_BOTTOM];
        facet->borderPlanes[1]   = borders[EN_LEFT];
        facet->borderNoAdjust[1] = noAdjust[EN_LEFT];
        facet->borderPlanes[2]   = gridPlanes[i][j][0];
        if (facet->borderPlanes[2] == -1) {
          facet->borderPlanes[2] = borders[EN_TOP];
          if (facet->borderPlanes[2] == -1) { facet->borderPlanes[2] = CM_EdgePlaneNum(grid, gridPlanes, i, j, 5); }
        }
        CM_SetBorderInward(facet, grid, gridPlanes, i, j, 1);
        if (CM_ValidateFacet(facet)) {
          CM_AddFacetBevels(facet);
          numFacets++;
        }
      }
    }
  }

  // copy the results out
  pf->numPlanes = numPlanes;
  pf->numFacets = numFacets;
  pf->facets    = Hunk_Alloc(numFacets * sizeof(*pf->facets), h_high);
  Std_memcpy(pf->facets, facets, numFacets * sizeof(*pf->facets));
  pf->planes = Hunk_Alloc(numPlanes * sizeof(*pf->planes), h_high);
  Std_memcpy(pf->planes, planes, numPlanes * sizeof(*pf->planes));
}
