#include "../load.h"


//............................
// MAP LOADING
//............................

//............................
// CMod_LoadShaders
//............................
static void CMod_LoadShaders(const Lump* l) {
  dShader* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) { err(ERR_DROP, "%s: funny lump size", __func__); }
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no shaders", __func__);
  cm.shaders    = Hunk_Alloc(count * sizeof(*cm.shaders), h_high);
  cm.numShaders = count;
  memcpy(cm.shaders, in, count * sizeof(*cm.shaders));
}

//............................
// CMod_LoadSubmodels
//............................
static void CMod_LoadSubmodels(const Lump* l) {
  dModel* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no models", __func__);
  cm.cmodels      = Hunk_Alloc(count * sizeof(*cm.cmodels), h_high);
  cm.numSubModels = count;
  if (count > MAX_SUBMODELS) err(ERR_DROP, "%s: MAX_SUBMODELS exceeded", __func__);
  i32* indexes;
  for (i32 i = 0; i < count; i++, in++) {
    cModel* out = &cm.cmodels[i];
    for (i32 j = 0; j < 3; j++) {  // spread the mins / maxs by a pixel
      out->mins[j] = in->mins[j] - 1;
      out->maxs[j] = in->maxs[j] + 1;
    }
    if (i == 0) { continue; }  // world model doesn't need other info
    // make a "leaf" just to hold the model's brushes and surfaces
    out->leaf.numLeafBrushes = in->numBrushes;
    indexes                  = Hunk_Alloc(out->leaf.numLeafBrushes * 4, h_high);
    out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
    for (i32 j = 0; j < out->leaf.numLeafBrushes; j++) { indexes[j] = in->firstBrush + j; }
    out->leaf.numLeafSurfaces  = in->numSurfaces;
    indexes                    = Hunk_Alloc(out->leaf.numLeafSurfaces * 4, h_high);
    out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
    for (i32 j = 0; j < out->leaf.numLeafSurfaces; j++) { indexes[j] = in->firstSurface + j; }
  }
}

//............................
// CMod_LoadNodes
//............................
static void CMod_LoadNodes(const Lump* l) {
  dNode* in = (dNode*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map has no nodes", __func__);
  cm.nodes    = Hunk_Alloc(count * sizeof(*cm.nodes), h_high);
  cm.numNodes = count;
  i32    child;
  cNode* out = cm.nodes;
  for (i32 i = 0; i < count; i++, out++, in++) {
    out->plane = cm.planes + in->planeNum;
    for (i32 j = 0; j < 2; j++) {
      child            = in->children[j];
      out->children[j] = child;
    }
  }
}

//............................
// CM_BoundBrush
//............................
static void CM_BoundBrush(cBrush* b) {
  b->bounds[0][0] = -b->sides[0].plane->dist;
  b->bounds[1][0] = b->sides[1].plane->dist;
  b->bounds[0][1] = -b->sides[2].plane->dist;
  b->bounds[1][1] = b->sides[3].plane->dist;
  b->bounds[0][2] = -b->sides[4].plane->dist;
  b->bounds[1][2] = b->sides[5].plane->dist;
}

//............................
// CMod_LoadBrushes
//............................
static void CMod_LoadBrushes(const Lump* l) {
  dBrush* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count     = l->filelen / sizeof(*in);
  cm.brushes    = Hunk_Alloc((BOX_BRUSHES + count) * sizeof(*cm.brushes), h_high);
  cm.numBrushes = count;
  cBrush* out   = cm.brushes;
  for (i32 i = 0; i < count; i++, out++, in++) {
    out->sides     = cm.BSides + in->firstSide;
    out->numsides  = in->numSides;
    out->shaderNum = in->shaderNum;
    if (out->shaderNum < 0 || out->shaderNum >= cm.numShaders) { err(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum); }
    out->contents = cm.shaders[out->shaderNum].contentFlags;
    CM_BoundBrush(out);
  }
}

//............................
// CMod_LoadLeafs
//............................
static void CMod_LoadLeafs(const Lump* l) {
  dLeaf* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no leafs", __func__);
  cm.leafs    = Hunk_Alloc((BOX_LEAFS + count) * sizeof(*cm.leafs), h_high);
  cm.numLeafs = count;
  cLeaf* out  = cm.leafs;
  for (i32 i = 0; i < count; i++, in++, out++) {
    out->cluster          = in->cluster;
    out->area             = in->area;
    out->firstLeafBrush   = in->firstLeafBrush;
    out->numLeafBrushes   = in->numLeafBrushes;
    out->firstLeafSurface = in->firstLeafSurface;
    out->numLeafSurfaces  = in->numLeafSurfaces;
    if (out->cluster >= cm.numClusters) cm.numClusters = out->cluster + 1;
    if (out->area >= cm.numAreas) cm.numAreas = out->area + 1;
  }
  cm.areas       = Hunk_Alloc(cm.numAreas * sizeof(*cm.areas), h_high);
  cm.areaPortals = Hunk_Alloc(cm.numAreas * cm.numAreas * sizeof(*cm.areaPortals), h_high);
}

//............................
// CMod_LoadBSides
//............................
static void CMod_LoadBSides(const Lump* l) {
  dBSide_t* in = (dBSide_t*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) { err(ERR_DROP, "%s: funny lump size", __func__); }
  i32 count    = l->filelen / sizeof(*in);
  cm.BSides    = Hunk_Alloc((BOX_SIDES + count) * sizeof(*cm.BSides), h_high);
  cm.numBSides = count;
  cBSide* out  = cm.BSides;
  for (i32 i = 0; i < count; i++, in++, out++) {
    i32 num        = in->planeNum;
    out->plane     = &cm.planes[num];
    out->shaderNum = in->shaderNum;
    if (out->shaderNum < 0 || out->shaderNum >= cm.numShaders) { err(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum); }
    out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
  }
}


//............................
// CMod_LoadPlanes
//............................
static void CMod_LoadPlanes(const Lump* l) {
  dPlane* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no planes", __func__);
  cm.planes    = Hunk_Alloc((BOX_PLANES + count) * sizeof(*cm.planes), h_high);
  cm.numPlanes = count;
  cPlane* out  = cm.planes;
  for (i32 i = 0; i < count; i++, in++, out++) {
    i32 bits = 0;
    for (i32 j = 0; j < 3; j++) {
      out->normal[j] = in->normal[j];
      if (out->normal[j] < 0) bits |= 1 << j;
    }
    out->dist     = in->dist;
    out->type     = PlaneTypeForNormal(out->normal);
    out->signbits = bits;
  }
}


//............................
// CMod_LoadLeafBrushes
//............................
static void CMod_LoadLeafBrushes(const Lump* l) {
  i32* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count         = l->filelen / sizeof(*in);
  cm.leafbrushes    = Hunk_Alloc((count + BOX_BRUSHES) * sizeof(*cm.leafbrushes), h_high);
  cm.numLeafBrushes = count;
  i32* out          = cm.leafbrushes;
  for (i32 i = 0; i < count; i++, in++, out++) { *out = *in; }
}


//............................
// CMod_LoadLeafSurfaces
//............................
static void CMod_LoadLeafSurfaces(const Lump* l) {
  i32* in = (void*)(cmod_base + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count          = l->filelen / sizeof(*in);
  cm.leafsurfaces    = Hunk_Alloc(count * sizeof(*cm.leafsurfaces), h_high);
  cm.numLeafSurfaces = count;
  i32* out           = cm.leafsurfaces;
  for (i32 i = 0; i < count; i++, in++, out++) { *out = *in; }
}


//..............................
// CMod_CheckLeafBrushes
//..............................
static void CMod_CheckLeafBrushes(void) {
  for (i32 i = 0; i < cm.numLeafBrushes; i++) {
    if (cm.leafbrushes[i] < 0 || cm.leafbrushes[i] >= cm.numBrushes) {
      echo("[%i] invalid leaf brush %08x", i, cm.leafbrushes[i]);
      cm.leafbrushes[i] = 0;
    }
  }
}


//..............................
// CMod_LoadEntityString
//..............................
static void CMod_LoadEntityString(const Lump* l) {
  cm.entityString   = Hunk_Alloc(l->filelen, h_high);
  cm.numEntityChars = l->filelen;
  Std_memcpy(cm.entityString, cmod_base + l->fileofs, l->filelen);
}

//..............................
// CMod_LoadVisibility
//..............................
static void CMod_LoadVisibility(const Lump* l) {
  i32 len = l->filelen;
  if (!len) {
    cm.clusterBytes = (cm.numClusters + 31) & ~31;
    cm.visibility   = Hunk_Alloc(cm.clusterBytes, h_high);
    Std_memset(cm.visibility, 255, cm.clusterBytes);
    return;
  }
  byte* buf       = cmod_base + l->fileofs;
  cm.vised        = true;
  cm.visibility   = Hunk_Alloc(len, h_high);
  cm.numClusters  = ((i32*)buf)[0];
  cm.clusterBytes = ((i32*)buf)[1];
  Std_memcpy(cm.visibility, buf + VIS_HEADER, len - VIS_HEADER);
}


//..............................
// CMod_LoadPatches
//..............................
static void CMod_LoadPatches(const Lump* surfs, const Lump* verts) {
  dSurf* in = (void*)(cmod_base + surfs->fileofs);
  if (surfs->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count;
  cm.numSurfaces = count = surfs->filelen / sizeof(*in);
  cm.surfaces            = Hunk_Alloc(cm.numSurfaces * sizeof(cm.surfaces[0]), h_high);
  dVert* dv              = (void*)(cmod_base + verts->fileofs);
  if (verts->filelen % sizeof(*dv)) err(ERR_DROP, "%s: funny lump size", __func__);
  // scan through all the surfaces, but only load patches, not planar faces
  cPatch* patch;
  for (i32 i = 0; i < count; i++, in++) {
    if (in->surfaceType != MST_PATCH) { continue; }  // ignore other surfaces
    // FIXME: check for non-colliding patches
    cm.surfaces[i] = patch = Hunk_Alloc(sizeof(*patch), h_high);
    // load the full drawverts onto the stack
    i32 width              = in->patchWidth;
    i32 height             = in->patchHeight;
    i32 c                  = width * height;
    if (c > MAX_PATCH_VERTS) { err(ERR_DROP, "%s: MAX_PATCH_VERTS", __func__); }
    dVert* dv_p = dv + in->firstVert;
    vec3   points[MAX_PATCH_VERTS];
    for (i32 j = 0; j < c; j++, dv_p++) {
      points[j][0] = dv_p->xyz[0];
      points[j][1] = dv_p->xyz[1];
      points[j][2] = dv_p->xyz[2];
    }
    i32 shaderNum       = in->shaderNum;
    patch->contents     = cm.shaders[shaderNum].contentFlags;
    patch->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
    // create the internal facet structure
    patch->pc           = CM_GeneratePatchCollide(width, height, points);
  }
}


//..............................
// CM_LoadMap
// Loads the map and all submodels
// Stores the data in the heap allocated state.c variables
//..............................
void CM_LoadMap(const char* name, bool clientload, i32* checksum) {
  if (!name || !name[0]) { err(ERR_DROP, "%s: NULL name", __func__); }
  CM_InitCfg();  // New function. Replacement for intializing config (og was Cvar_Get)
  if (load.developer) { echo("%s( '%s', %i )", __func__, name, clientload); }

  if (!strcmp(cm.name, name) && clientload) {
    *checksum = cm.checksum;
    return;
  }

  // free old stuff
  CM_ClearMap();

  // load the file
  void* buf;
  i32   length = FileRead(name, &buf);

  if (!buf) { err(ERR_DROP, "%s: couldn't load %s", __func__, name); }
  if (length < sizeof(dHeader)) { err(ERR_DROP, "%s: %s has truncated header", __func__, name); }

  *checksum = cm.checksum = Com_BlockChecksum(buf, length);  // TODO: Why is the checksum coming from md4 loading code?
  dHeader header          = *(dHeader*)buf;
  for (size_t i = 0; i < sizeof(dHeader) / sizeof(i32); i++) { ((i32*)&header)[i] = ((i32*)&header)[i]; }

  if (header.version != BSP_VERSION) { err(ERR_DROP, "%s: %s has wrong version number (%i should be %i)", __func__, name, header.version, BSP_VERSION); }

  for (i32 lumpId = 0; lumpId < LUMP_COUNT; lumpId++) {
    i32 ofs = header.lumps[lumpId].fileofs;
    i32 len = header.lumps[lumpId].filelen;
    if ((u32)ofs > MAX_I32 || (u32)len > MAX_I32 || ofs + len > length || ofs + len < 0) {
      err(ERR_DROP, "%s: %s has wrong lump[%i] size/offset", __func__, name, lumpId);
    }
  }

  cmod_base = (byte*)buf;

  // load into heap  (with Hunk_Alloc)
  CMod_LoadShaders(&header.lumps[LUMP_SHADERS]);
  CMod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
  CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
  CMod_LoadLeafSurfaces(&header.lumps[LUMP_LEAFSURFACES]);
  CMod_LoadPlanes(&header.lumps[LUMP_PLANES]);
  CMod_LoadBSides(&header.lumps[LUMP_BSideS]);
  CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
  CMod_LoadSubmodels(&header.lumps[LUMP_MODELS]);
  CMod_LoadNodes(&header.lumps[LUMP_NODES]);
  CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES]);
  CMod_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
  CMod_LoadPatches(&header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS]);

  CMod_CheckLeafBrushes();

  // We only free the buffer, because the file is cached for the ref (drawing)
  FileFree(buf);

  // Initialize the stored data
  CM_InitBoxHull();
  CM_FloodAreaConnections();
  // Allow this to be cached if it is loaded by the server
  if (!clientload) { strncpyz(cm.name, name, sizeof(cm.name)); }
}


//..............................
// CM_ClearMap
//   Free/Erase the currently stored clipMap data
//..............................
void CM_ClearMap(void) {
  memset(&cm, 0, sizeof(cm));
  CM_ClearLevelPatches();
}
