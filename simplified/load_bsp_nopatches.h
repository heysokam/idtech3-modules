#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  // For varargs
#include <stdio.h>   // For fprintf, vsnprintf
#include <inttypes.h>
#include <stdbool.h>

//......................................................................................................................
// Interface
// .........................
// In -one- source file, add:
//   #define IMP_LoadBspNopatches
//   #include "load_bsp_nopatches.h"
//
// Include this header wherever a bsp needs to be loaded
// The only external function is:
//   void CM_LoadMap(cMap* cm, const char* name, bool clientload, i32* checksum, bool developer, AllocPtr alloc);
//   Loads the map and all submodels.
//   Stores the data in the given cMap pointer.
//   name      : path to the .bsp file.
//   checksum  : output of the bsp checksum i32.
//   developer : prints debugging information to console.
//   alloc     : Optional calloc-like allocator function. Will use stdlib's calloc when passing NULL.
//
// After the call, the pointer will contain valid collision data.
// NOTE: This file is made to work with the simplified collision solver of col_bbox_bsp.h
//       Patch loading is currently disabled, and not included in this file.
//       CM_InitBoxHull is disabled. This means that generated cModel collision cannot be performed with this data.
//       Those features are irrelevant for bbox-to-bsp collision, which is what this file is meant to be used for.
//       Use the non-simplified version of the code if you need them.
//......................................................................................................................


//......................................................................................................................
// types/base.h
typedef int32_t       i32;
typedef uint32_t      u32;
typedef float         f32;
typedef unsigned char byte;
typedef f32           vec3[3];
//..................
const vec3 vec3_origin = { 0, 0, 0 };

// #include "./tools/core.h"
// #include "./files/core.h"

//......................................................................................................................
// load.h
//..............................
// Memory allocation function Prototype:
//   void*(alloc)(size_t nitems, size_t size);
//   Same shape as calloc.
//   The module will default to using stdlib's calloc when passing a NULL pointer.
//..............................
typedef void* (*AllocPtr)(size_t nitems, size_t size);  // alias for:  void*(alloc)(size_t nitems, size_t size)

//..............................
// Math
#define MAX_I32 0x7fffffff
//..............................
#define BSP_VERSION 46
#define MAX_SUBMODELS 256
#define MAX_PATCH_VERTS 1024
//..............................
// to allow boxes to be treated as brush models,
// some extra indexes are allocated along with those needed by the map
#define BOX_BRUSHES 1
#define BOX_SIDES 6
#define BOX_LEAFS 2
#define BOX_PLANES 12


//..............................
// Lump IDs
#define LUMP_ENTITIES 0
#define LUMP_SHADERS 1
#define LUMP_PLANES 2
#define LUMP_NODES 3
#define LUMP_LEAFS 4
#define LUMP_LEAFSURFACES 5
#define LUMP_LEAFBRUSHES 6
#define LUMP_MODELS 7
#define LUMP_BRUSHES 8
#define LUMP_BSIDES 9
#define LUMP_DRAWVERTS 10
#define LUMP_DRAWINDEXES 11
#define LUMP_FOGS 12
#define LUMP_SURFACES 13
#define LUMP_LIGHTMAPS 14
#define LUMP_LIGHTGRID 15
#define LUMP_VISIBILITY 16
#define LUMP_COUNT 17
#define VIS_HEADER 8
//..............................
// types/rawBSP  :  All data
//..............................
typedef struct {
  i32 fileofs;
  i32 filelen;
} Lump;
//..............................
// planes x^1 is allways the opposite of plane x
typedef struct {
  f32 normal[3];
  f32 dist;
} dPlane;
//..............................
typedef struct {
  i32 planeNum;
  i32 children[2];  // negative numbers are -(leafs+1), not nodes
  i32 mins[3];      // for frustom culling
  i32 maxs[3];
} dNode;
//..............................
typedef struct {
  i32 cluster;  // -1 = opaque cluster (do I still store these?)
  i32 area;
  i32 mins[3];  // for frustum culling
  i32 maxs[3];
  i32 firstLeafSurface;
  i32 numLeafSurfaces;
  i32 firstLeafBrush;
  i32 numLeafBrushes;
} dLeaf;
//..............................
typedef struct {
  i32 firstSide;
  i32 numSides;
  i32 shaderNum;  // the shader that determines the contents flags
} dBrush;
//..............................
typedef struct {
  i32  ident;
  i32  version;
  Lump lumps[LUMP_COUNT];
} dHeader;
//..............................
// Drawing related types
typedef struct {
  i32 planeNum;  // positive plane side faces out of the leaf
  i32 shaderNum;
} dBSide_t;
//..............................
typedef struct {
  f32 mins[3], maxs[3];
  i32 firstSurface;
  i32 numSurfaces;
  i32 firstBrush;
  i32 numBrushes;
} dModel;
//....................................
typedef enum { MST_BAD, MST_PLANAR, MST_PATCH, MST_TRIANGLE_SOUP, MST_FLARE } mapSurfaceType_t;
//....................................
typedef struct {
  i32  shaderNum;
  i32  fogNum;
  i32  surfaceType;
  i32  firstVert;
  i32  numVerts;
  i32  firstIndex;
  i32  numIndexes;
  i32  lightmapNum;
  i32  lightmapX;
  i32  lightmapY;
  i32  lightmapWidth;
  i32  lightmapHeight;
  vec3 lightmapOrigin;
  vec3 lightmapVecs[3];  // for patches, [0] and [1] are lodbounds
  i32  patchWidth;
  i32  patchHeight;
} dSurf;
//..............................
typedef union {
  byte rgba[4];
  u32  u32;
} color4ub_t;
//..............................
typedef struct {
  vec3       xyz;
  f32        st[2];
  f32        lightmap[2];
  vec3       normal;
  color4ub_t color;
} dVert;
//..............................
// types/solve.h  :  Collision data
//..............................
// cfg.h
#define MAX_PATHLEN 64  // the maximum size of game relative pathnames
//....................................
// BSP data types  (clipType = cType : drawType = dType)
//....................................
typedef struct cplane_s {
  vec3 normal;
  f32  dist;
  byte type;      // for fast side tests: 0,1,2 = axial, 3 = nonaxial
  byte signbits;  // signx + (signy<<1) + (signz<<2), used as lookup during collision
  byte pad[2];
} cPlane;
//..................
typedef struct {
  cPlane* plane;
  i32     children[2];  // negative numbers are leafs
} cNode;
//..................
typedef struct {
  char shader[MAX_PATHLEN];
  i32  surfaceFlags;
  i32  contentFlags;
} dShader;
//..................
typedef struct {
  cPlane* plane;
  i32     surfaceFlags;
  i32     shaderNum;
} cBSide;
//..................
typedef struct {
  i32 cluster;
  i32 area;
  i32 firstLeafBrush;
  i32 numLeafBrushes;
  i32 firstLeafSurface;
  i32 numLeafSurfaces;
} cLeaf;
//..................
typedef struct cmodel_s {
  vec3  mins, maxs;
  cLeaf leaf;  // submodels don't reference the main tree
} cModel;
//..................
typedef struct {
  i32     shaderNum;  // the shader that determined the contents
  i32     contents;
  vec3    bounds[2];
  i32     numsides;
  cBSide* sides;
  i32     checkcount;  // to avoid repeated testings
} cBrush;
//..................
typedef struct {
  i32 floodNum;
  i32 floodValid;
} cArea;
//..................
typedef struct {
  i32                    checkcount;  // to avoid repeated testings
  i32                    surfaceFlags;
  i32                    contents;
  struct patchCollide_s* pc;
} cPatch;
//..................
typedef struct {
  char     name[MAX_PATHLEN];
  i32      numShaders;
  dShader* shaders;
  i32      numBSides;
  cBSide*  BSides;
  i32      numPlanes;
  cPlane*  planes;
  i32      numNodes;
  cNode*   nodes;
  i32      numLeafs;
  cLeaf*   leafs;
  i32      numLeafBrushes;
  i32*     leafbrushes;
  i32      numLeafSurfaces;
  i32*     leafsurfaces;
  i32      numSubModels;
  cModel*  cmodels;
  i32      numBrushes;
  cBrush*  brushes;
  i32      numClusters;
  i32      clusterBytes;
  byte*    visibility;
  bool     vised;  // if false, visibility is just a single cluster of ffs
  i32      numEntityChars;
  char*    entityString;
  i32      numAreas;
  cArea*   areas;
  i32*     areaPortals;  // [ numAreas*numAreas ] reference counts
  i32      numSurfaces;
  cPatch** surfaces;  // non-patches will be NULL
  i32      floodValid;
  i32      checkcount;  // incremented on each trace
  u32      checksum;
} cMap;
//......................................................................................................................


//..............................
// Interface :
//..............................

//..............................
// CM_LoadMap :
//   Loads the map and all submodels.
//   Stores the data in the given cMap pointer.
//   name      : path to the .bsp file.
//   checksum  : output of the bsp checksum i32.
//   developer : prints debugging information to console.
//   alloc     : Optional calloc-like allocator function. Will use stdlib's calloc when passing NULL.
void CM_LoadMap(cMap* cm, const char* name, bool clientload, i32* checksum, bool developer, AllocPtr alloc);
//..............................
#if defined IMP_LoadBspNopatches
//..............................


//......................................................................................................................
// tools.h
//..............................
#  define MAX_PRINTMSG 8192
//..............................
// Error types
typedef enum {
  ERR_EXIT,  // Exit the entire game, and prints the message to stderr
  ERR_DROP,  // print to console and disconnect from game
  // ERR_SVDISCONNECT,  // don't kill server
  // ERR_CLDISCONNECT,  // client disconnected from the server
} ErrorType;
//..............................

//..............................
// tools.c
//..............................
// echo
//   Prints a newline message to stdout
//   Replaces both  Com_Printf  and  Com_DPrintf
//   Mod version just outputs to stdout instead
//..............................
static void echo(const char* fmt, ...) {
  va_list argptr;
  char    msg[MAX_PRINTMSG];
  va_start(argptr, fmt);
  vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);
  fprintf(stdout, "%s\n", msg);
}
//..............................
// Com_Error
//   Mod version just outputs to stderr.
//..............................
static void err(ErrorType type, const char* msg, ...) {
  va_list argptr;
  char    text[1024];
  va_start(argptr, msg);
  vsnprintf(text, sizeof(text), msg, argptr);
  va_end(argptr);

  // Handle each error type
  switch (type) {
    case ERR_EXIT: fprintf(stderr, "%s\n", text); exit(-1);
    default: fprintf(stderr, "Unknown error:\t %s\n", text); exit(-1);
  }
}
//..............................
// Q_strncpyz
//   Safe strncpy that ensures a trailing zero
//..............................
static void strncpyz(char* dest, const char* src, int destsize) {
  if (!dest) { err(ERR_EXIT, "Q_strncpyz: NULL dest"); }
  if (!src) { err(ERR_EXIT, "Q_strncpyz: NULL src"); }
  if (destsize < 1) { err(ERR_EXIT, "Q_strncpyz: destsize < 1"); }
#  if 1
  // do not fill whole remaining buffer with zeros
  // this is obvious behavior change but actually it may affect only buggy QVMs
  // which passes overlapping or short buffers to cvar reading routines
  // what is rather good than bad because it will no longer cause overwrites, maybe
  while (--destsize > 0 && (*dest++ = *src++) != '\0')
    ;
  *dest = '\0';
#  else
  strncpy(dest, src, destsize - 1);
  dest[destsize - 1] = '\0';
#  endif
}
//......................................................................................................................


//......................................................................................................................
// io.c
//...............................
// Reads the file at src, and returns a filestream pointer.
static FILE* FileOpen(const char* src, char* mode) {
  FILE* f = fopen(src, mode);
  if (f != NULL) { return f; }
  printf("Error opening file %s in mode: %s\n", src, mode);
  return NULL;
}
//...............................
// Returns the size of the given filestream
static long StreamGetSize(FILE* src) {
  fpos_t initPos = { 0 };
  fgetpos(src, &initPos);   // Save current initial position
  fseek(src, 0, SEEK_END);  // Move to the end
  long size = ftell(src);   // End pointer will be the size
  fsetpos(src, &initPos);   // Restore initial position
  return size;
}
//...............................
// FS_ReadFile
//   Q3 style version, using stdlib only.
//   File length is returned and the buffer is given.
//   Uses calloc, instead of Hunk_AllocateTempMemory(len).
static i32 FileRead(const char* src, void** buf) {
  FILE*  file   = FileOpen(src, "rb");
  long   size   = StreamGetSize(file);
  byte*  buffer = (byte*)calloc(size, sizeof(byte));  // Hunk_AllocateTempMemory
  size_t read   = fread(buffer, size, 1, file);
  if (!read) {
    free(buffer);
    printf("%s: Failed to store Stream data into the Buffer\n", __func__);
    return -1;
  }
  *buf = buffer;
  return size;
}
//...............................
// FS_FreeFile
//   Simple memory free alias.
//   Original Q3 code freed memory with Hunk_FreeTempMemory.
//   If freeing made the temp hunk empty,
//   it then cleared its memory with Hunk_ClearTempMemory
static void FileFree(void* buffer) { free(buffer); }
//......................................................................................................................


//......................................................................................................................
// checksum generation (from md4.c)
//.....................
struct mdfour {
  u32 A, B, C, D;
  u32 totalN;
};
//.....................
static struct mdfour* m;
//.....................
#  define F(X, Y, Z) (((X) & (Y)) | ((~(X)) & (Z)))
#  define G(X, Y, Z) (((X) & (Y)) | ((X) & (Z)) | ((Y) & (Z)))
#  define H(X, Y, Z) ((X) ^ (Y) ^ (Z))
#  define lshift(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
//.....................
#  define ROUND1(a, b, c, d, k, s) a = lshift(a + F(b, c, d) + X[k], s)
#  define ROUND2(a, b, c, d, k, s) a = lshift(a + G(b, c, d) + X[k] + 0x5A827999, s)
#  define ROUND3(a, b, c, d, k, s) a = lshift(a + H(b, c, d) + X[k] + 0x6ED9EBA1, s)
/* this applies md4 to 64 byte chunks */
static void mdfour64(u32* M) {
  i32 j;
  u32 AA, BB, CC, DD;
  u32 X[16];
  u32 A, B, C, D;
  for (j = 0; j < 16; j++) X[j] = M[j];
  A  = m->A;
  B  = m->B;
  C  = m->C;
  D  = m->D;
  AA = A;
  BB = B;
  CC = C;
  DD = D;
  ROUND1(A, B, C, D, 0, 3);
  ROUND1(D, A, B, C, 1, 7);
  ROUND1(C, D, A, B, 2, 11);
  ROUND1(B, C, D, A, 3, 19);
  ROUND1(A, B, C, D, 4, 3);
  ROUND1(D, A, B, C, 5, 7);
  ROUND1(C, D, A, B, 6, 11);
  ROUND1(B, C, D, A, 7, 19);
  ROUND1(A, B, C, D, 8, 3);
  ROUND1(D, A, B, C, 9, 7);
  ROUND1(C, D, A, B, 10, 11);
  ROUND1(B, C, D, A, 11, 19);
  ROUND1(A, B, C, D, 12, 3);
  ROUND1(D, A, B, C, 13, 7);
  ROUND1(C, D, A, B, 14, 11);
  ROUND1(B, C, D, A, 15, 19);

  ROUND2(A, B, C, D, 0, 3);
  ROUND2(D, A, B, C, 4, 5);
  ROUND2(C, D, A, B, 8, 9);
  ROUND2(B, C, D, A, 12, 13);
  ROUND2(A, B, C, D, 1, 3);
  ROUND2(D, A, B, C, 5, 5);
  ROUND2(C, D, A, B, 9, 9);
  ROUND2(B, C, D, A, 13, 13);
  ROUND2(A, B, C, D, 2, 3);
  ROUND2(D, A, B, C, 6, 5);
  ROUND2(C, D, A, B, 10, 9);
  ROUND2(B, C, D, A, 14, 13);
  ROUND2(A, B, C, D, 3, 3);
  ROUND2(D, A, B, C, 7, 5);
  ROUND2(C, D, A, B, 11, 9);
  ROUND2(B, C, D, A, 15, 13);

  ROUND3(A, B, C, D, 0, 3);
  ROUND3(D, A, B, C, 8, 9);
  ROUND3(C, D, A, B, 4, 11);
  ROUND3(B, C, D, A, 12, 15);
  ROUND3(A, B, C, D, 2, 3);
  ROUND3(D, A, B, C, 10, 9);
  ROUND3(C, D, A, B, 6, 11);
  ROUND3(B, C, D, A, 14, 15);
  ROUND3(A, B, C, D, 1, 3);
  ROUND3(D, A, B, C, 9, 9);
  ROUND3(C, D, A, B, 5, 11);
  ROUND3(B, C, D, A, 13, 15);
  ROUND3(A, B, C, D, 3, 3);
  ROUND3(D, A, B, C, 11, 9);
  ROUND3(C, D, A, B, 7, 11);
  ROUND3(B, C, D, A, 15, 15);
  A += AA;
  B += BB;
  C += CC;
  D += DD;
  for (j = 0; j < 16; j++) X[j] = 0;
  m->A = A;
  m->B = B;
  m->C = C;
  m->D = D;
}
static void copy64(u32* M, const byte* in) {
  for (i32 i = 0; i < 16; i++) M[i] = ((u32)in[i * 4 + 3] << 24) | ((u32)in[i * 4 + 2] << 16) | ((u32)in[i * 4 + 1] << 8) | ((u32)in[i * 4 + 0] << 0);
}
static void copy4(byte* out, u32 x) {
  out[0] = x & 0xFF;
  out[1] = (x >> 8) & 0xFF;
  out[2] = (x >> 16) & 0xFF;
  out[3] = (x >> 24) & 0xFF;
}
static void mdfour_begin(struct mdfour* md) {
  md->A      = 0x67452301;
  md->B      = 0xefcdab89;
  md->C      = 0x98badcfe;
  md->D      = 0x10325476;
  md->totalN = 0;
}
static void mdfour_tail(const byte* in, i32 n) {
  byte buf[128];
  u32  M[16];
  u32  b;
  m->totalN += n;
  b = m->totalN * 8;
  memset(buf, 0, 128);
  if (n) memcpy(buf, in, n);
  buf[n] = 0x80;
  if (n <= 55) {
    copy4(buf + 56, b);
    copy64(M, buf);
    mdfour64(M);
  } else {
    copy4(buf + 120, b);
    copy64(M, buf);
    mdfour64(M);
    copy64(M, buf + 64);
    mdfour64(M);
  }
}
static void mdfour_update(struct mdfour* md, const byte* in, i32 n) {
  u32 M[16];
  m = md;
  if (n == 0) mdfour_tail(in, n);
  while (n >= 64) {
    copy64(M, in);
    mdfour64(M);
    in += 64;
    n -= 64;
    m->totalN += 64;
  }
  mdfour_tail(in, n);
}

static void mdfour_result(struct mdfour* md, byte* out) {
  copy4(out, md->A);
  copy4(out + 4, md->B);
  copy4(out + 8, md->C);
  copy4(out + 12, md->D);
}
static void mdfour(byte* out, const byte* in, i32 n) {
  struct mdfour md;
  mdfour_begin(&md);
  mdfour_update(&md, in, n);
  mdfour_result(&md, out);
}
unsigned Com_BlockChecksum(const void* buffer, i32 length) {
  i32      digest[4];
  unsigned val;
  mdfour((byte*)digest, (byte*)buffer, length);
  val = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];
  return val;
}
//......................................................................................................................


//......................................................................................................................
// load.c
//..............................
// plane types are used to speed some tests
// 0-2 are axial planes
#  define PLANE_X 0
#  define PLANE_Y 1
#  define PLANE_Z 2
#  define PLANE_NON_AXIAL 3
//..............................
#  define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)))

//..............................
// Loading: Forward declare
//..............................
void CM_ClearMap(cMap* cm);
// Collision data
void CMod_LoadLeafs(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadLeafBrushes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadLeafSurfaces(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadPlanes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadBrushes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadNodes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_CheckLeafBrushes(cMap* cm);
//..............................
// Drawing Data
void CMod_LoadShaders(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadBSides(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadSubmodels(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadEntityString(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadVisibility(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc);
void CMod_LoadPatches(const byte* rawBSP, const Lump* surfs, const Lump* verts, cMap* cm, AllocPtr alloc);
void CM_FloodAreaConnections(cMap* cm);
//..............................


// Interface
//..............................
void CM_LoadMap(cMap* cm, const char* name, bool clientload, i32* checksum, bool developer, AllocPtr alloc) {
  if (!name || !name[0]) { err(ERR_DROP, "%s: NULL name", __func__); }
  if (developer) { echo("%s( '%s', %i )", __func__, name, clientload); }

  if (!strcmp(cm->name, name) && clientload) {
    *checksum = cm->checksum;
    return;
  }

  // free old stuff
  CM_ClearMap(cm);

  // load the file
  void* buf;
  i32   length = FileRead(name, &buf);

  if (!buf) { err(ERR_DROP, "%s: couldn't load %s", __func__, name); }
  if (length < sizeof(dHeader)) { err(ERR_DROP, "%s: %s has truncated header", __func__, name); }

  *checksum = cm->checksum = Com_BlockChecksum(buf, length);  // TODO: Why is the checksum coming from md4 loading code?
  dHeader header           = *(dHeader*)buf;
  for (size_t i = 0; i < sizeof(dHeader) / sizeof(i32); i++) { ((i32*)&header)[i] = ((i32*)&header)[i]; }

  if (header.version != BSP_VERSION) { err(ERR_DROP, "%s: %s has wrong version number (%i should be %i)", __func__, name, header.version, BSP_VERSION); }

  for (i32 lumpId = 0; lumpId < LUMP_COUNT; lumpId++) {
    i32 ofs = header.lumps[lumpId].fileofs;
    i32 len = header.lumps[lumpId].filelen;
    if ((u32)ofs > MAX_I32 || (u32)len > MAX_I32 || ofs + len > length || ofs + len < 0) {
      err(ERR_DROP, "%s: %s has wrong lump[%i] size/offset", __func__, name, lumpId);
    }
  }

  byte* rawBSP = (byte*)buf;  // was global  byte* cmod_base

  // load into heap  (with alloc)
  CMod_LoadShaders(rawBSP, &header.lumps[LUMP_SHADERS], cm, alloc);
  CMod_LoadLeafs(rawBSP, &header.lumps[LUMP_LEAFS], cm, alloc);
  CMod_LoadLeafBrushes(rawBSP, &header.lumps[LUMP_LEAFBRUSHES], cm, alloc);
  CMod_LoadLeafSurfaces(rawBSP, &header.lumps[LUMP_LEAFSURFACES], cm, alloc);
  CMod_LoadPlanes(rawBSP, &header.lumps[LUMP_PLANES], cm, alloc);
  CMod_LoadBSides(rawBSP, &header.lumps[LUMP_BSIDES], cm, alloc);  // Textures
  CMod_LoadBrushes(rawBSP, &header.lumps[LUMP_BRUSHES], cm, alloc);
  CMod_LoadSubmodels(rawBSP, &header.lumps[LUMP_MODELS], cm, alloc);
  CMod_LoadNodes(rawBSP, &header.lumps[LUMP_NODES], cm, alloc);
  CMod_LoadEntityString(rawBSP, &header.lumps[LUMP_ENTITIES], cm, alloc);
  CMod_LoadVisibility(rawBSP, &header.lumps[LUMP_VISIBILITY], cm, alloc);
#  if 0   // disabled patches
  CMod_LoadPatches(rawBSP, &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS], cm, alloc);
#  endif  // disabled patches

  CMod_CheckLeafBrushes(cm);

  // Only free the buffer. The file is cached by the caller
  FileFree(buf);

  // Initialize the stored data
  // CM_InitBoxHull();
  CM_FloodAreaConnections(cm);
  // Allow this to be cached if it is loaded by the server
  if (!clientload) { strncpyz(cm->name, name, sizeof(cm->name)); }
}

//..............................
// CM_ClearMap
//   Free/Erase the currently stored clipMap data
//..............................
void CM_ClearMap(cMap* cm) {
  memset(cm, 0, sizeof(*cm));
  // CM_ClearLevelPatches();
}

//............................
// Collision Data
//............................

//............................
// CMod_LoadLeafs
//............................
void CMod_LoadLeafs(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dLeaf* in = (dLeaf*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no leafs", __func__);
  cm->leafs    = (cLeaf*)alloc((BOX_LEAFS + count), sizeof(*cm->leafs));
  cm->numLeafs = count;
  cLeaf* out   = cm->leafs;
  for (i32 i = 0; i < count; i++, in++, out++) {
    out->cluster          = in->cluster;
    out->area             = in->area;
    out->firstLeafBrush   = in->firstLeafBrush;
    out->numLeafBrushes   = in->numLeafBrushes;
    out->firstLeafSurface = in->firstLeafSurface;
    out->numLeafSurfaces  = in->numLeafSurfaces;
    if (out->cluster >= cm->numClusters) cm->numClusters = out->cluster + 1;
    if (out->area >= cm->numAreas) cm->numAreas = out->area + 1;
  }
  cm->areas       = (cArea*)alloc(cm->numAreas, sizeof(*cm->areas));
  cm->areaPortals = (i32*)alloc(cm->numAreas * cm->numAreas, sizeof(*cm->areaPortals));
}

//............................
// CMod_LoadLeafBrushes
//............................
void CMod_LoadLeafBrushes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  i32* in = (i32*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count          = l->filelen / sizeof(*in);
  cm->leafbrushes    = (i32*)alloc((count + BOX_BRUSHES), sizeof(*cm->leafbrushes));
  cm->numLeafBrushes = count;
  i32* out           = cm->leafbrushes;
  for (i32 i = 0; i < count; i++, in++, out++) { *out = *in; }
}


//............................
// CMod_LoadLeafSurfaces
//............................
void CMod_LoadLeafSurfaces(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  i32* in = (i32*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count           = l->filelen / sizeof(*in);
  cm->leafsurfaces    = (i32*)alloc(count, sizeof(*cm->leafsurfaces));
  cm->numLeafSurfaces = count;
  i32* out            = cm->leafsurfaces;
  for (i32 i = 0; i < count; i++, in++, out++) { *out = *in; }
}

//............................
// CMod_LoadPlanes
//............................
void CMod_LoadPlanes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dPlane* in = (dPlane*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no planes", __func__);
  cm->planes    = (cPlane*)alloc((BOX_PLANES + count), sizeof(*cm->planes));
  cm->numPlanes = count;
  cPlane* out   = cm->planes;
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
void CMod_LoadBrushes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dBrush* in = (dBrush*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count      = l->filelen / sizeof(*in);
  cm->brushes    = (cBrush*)alloc((BOX_BRUSHES + count), sizeof(*cm->brushes));
  cm->numBrushes = count;
  cBrush* out    = cm->brushes;
  for (i32 i = 0; i < count; i++, out++, in++) {
    out->sides     = cm->BSides + in->firstSide;
    out->numsides  = in->numSides;
    out->shaderNum = in->shaderNum;
    if (out->shaderNum < 0 || out->shaderNum >= cm->numShaders) { err(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum); }
    out->contents = cm->shaders[out->shaderNum].contentFlags;
    CM_BoundBrush(out);
  }
}
//............................
// CMod_LoadNodes
//............................
void CMod_LoadNodes(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dNode* in = (dNode*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map has no nodes", __func__);
  cm->nodes    = (cNode*)alloc(count, sizeof(*cm->nodes));
  cm->numNodes = count;
  i32    child;
  cNode* out = cm->nodes;
  for (i32 i = 0; i < count; i++, out++, in++) {
    out->plane = cm->planes + in->planeNum;
    for (i32 j = 0; j < 2; j++) {
      child            = in->children[j];
      out->children[j] = child;
    }
  }
}


//..............................
// CMod_CheckLeafBrushes
//..............................
void CMod_CheckLeafBrushes(cMap* cm) {
  for (i32 i = 0; i < cm->numLeafBrushes; i++) {
    if (cm->leafbrushes[i] < 0 || cm->leafbrushes[i] >= cm->numBrushes) {
      echo("[%i] invalid leaf brush %08x", i, cm->leafbrushes[i]);
      cm->leafbrushes[i] = 0;
    }
  }
}

//............................
// Drawing Data
//............................

//............................
// CMod_LoadShaders
//............................
void CMod_LoadShaders(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dShader* in = (dShader*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) { err(ERR_DROP, "%s: funny lump size", __func__); }
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no shaders", __func__);
  cm->shaders    = (dShader*)alloc(count, sizeof(*cm->shaders));
  cm->numShaders = count;
  memcpy(cm->shaders, in, count * sizeof(*cm->shaders));
}

//............................
// CMod_LoadBSides
//............................
void CMod_LoadBSides(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dBSide_t* in = (dBSide_t*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) { err(ERR_DROP, "%s: funny lump size", __func__); }
  i32 count     = l->filelen / sizeof(*in);
  cm->BSides    = (cBSide*)alloc((BOX_SIDES + count), sizeof(*cm->BSides));
  cm->numBSides = count;
  cBSide* out   = cm->BSides;
  for (i32 i = 0; i < count; i++, in++, out++) {
    i32 num        = in->planeNum;
    out->plane     = &cm->planes[num];
    out->shaderNum = in->shaderNum;
    if (out->shaderNum < 0 || out->shaderNum >= cm->numShaders) { err(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum); }
    out->surfaceFlags = cm->shaders[out->shaderNum].surfaceFlags;
  }
}

//............................
// CMod_LoadSubmodels
//............................
void CMod_LoadSubmodels(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dModel* in = (dModel*)(void*)(rawBSP + l->fileofs);
  if (l->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count = l->filelen / sizeof(*in);
  if (count < 1) err(ERR_DROP, "%s: map with no models", __func__);
  cm->cmodels      = (cModel*)alloc(count, sizeof(*cm->cmodels));
  cm->numSubModels = count;
  if (count > MAX_SUBMODELS) err(ERR_DROP, "%s: MAX_SUBMODELS exceeded", __func__);
  i32* indexes;
  for (i32 i = 0; i < count; i++, in++) {
    cModel* out = &cm->cmodels[i];
    for (i32 j = 0; j < 3; j++) {  // spread the mins / maxs by a pixel
      out->mins[j] = in->mins[j] - 1;
      out->maxs[j] = in->maxs[j] + 1;
    }
    if (i == 0) { continue; }  // world model doesn't need other info
    // make a "leaf" just to hold the model's brushes and surfaces
    out->leaf.numLeafBrushes = in->numBrushes;
    indexes                  = (i32*)alloc(out->leaf.numLeafBrushes, 4);
    out->leaf.firstLeafBrush = indexes - cm->leafbrushes;
    for (i32 j = 0; j < out->leaf.numLeafBrushes; j++) { indexes[j] = in->firstBrush + j; }
    out->leaf.numLeafSurfaces  = in->numSurfaces;
    indexes                    = (i32*)alloc(out->leaf.numLeafSurfaces, 4);
    out->leaf.firstLeafSurface = indexes - cm->leafsurfaces;
    for (i32 j = 0; j < out->leaf.numLeafSurfaces; j++) { indexes[j] = in->firstSurface + j; }
  }
}

//..............................
// CMod_LoadEntityString
//..............................
void CMod_LoadEntityString(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  cm->entityString   = (char*)alloc(1, l->filelen);
  cm->numEntityChars = l->filelen;
  memcpy(cm->entityString, rawBSP + l->fileofs, l->filelen);
}

//..............................
// CMod_LoadVisibility
//..............................
void CMod_LoadVisibility(const byte* rawBSP, const Lump* l, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  i32 len = l->filelen;
  if (!len) {
    cm->clusterBytes = (cm->numClusters + 31) & ~31;
    cm->visibility   = (byte*)alloc(1, cm->clusterBytes);
    memset(cm->visibility, 255, cm->clusterBytes);
    return;
  }
  byte* buf        = (byte*)rawBSP + l->fileofs;
  cm->vised        = true;
  cm->visibility   = (byte*)alloc(1, len);
  cm->numClusters  = ((i32*)buf)[0];
  cm->clusterBytes = ((i32*)buf)[1];
  memcpy(cm->visibility, buf + VIS_HEADER, len - VIS_HEADER);
}

//..............................
// CM_FloodArea_r
//   Recursively flood through all areas of the given clipMap
//   starting at areaNum, and setting the passed through areas to floodNum
//..............................
static void CM_FloodArea_r(cMap* cm, i32 areaNum, i32 floodNum) {
  cArea* area = &cm->areas[areaNum];
  if (area->floodValid == cm->floodValid) {
    if (area->floodNum == floodNum) return;
    err(ERR_DROP, "%s: reflooded", __func__);
  }
  area->floodNum   = floodNum;
  area->floodValid = cm->floodValid;
  i32* con         = cm->areaPortals + areaNum * cm->numAreas;
  for (i32 areaId = 0; areaId < cm->numAreas; areaId++) {
    if (con[areaId] > 0) { CM_FloodArea_r(cm, areaId, floodNum); }
  }
}

//..............................
// CM_FloodAreaConnections
//   Floods through all connected areas of the given clipMap
//   Starts counting from floodNum 0
//   Increases floodValid by one, and sets this number to all passed through areas
//..............................
void CM_FloodAreaConnections(cMap* cm) {
  // all current floods are now invalid
  cm->floodValid++;
  i32 floodNum = 0;

  for (i32 areaId = 0; areaId < cm->numAreas; areaId++) {
    cArea* area = &cm->areas[areaId];
    if (area->floodValid == cm->floodValid) { continue; }  // already flooded into
    floodNum++;
    CM_FloodArea_r(cm, areaId, floodNum);
  }
}

//..............................
// CMod_LoadPatches
//..............................
#  if 0   // disabled patches
void CMod_LoadPatches(const byte* rawBSP, const Lump* surfs, const Lump* verts, cMap* cm, AllocPtr alloc) {
  if (!alloc) alloc = calloc;
  dSurf* in = (void*)(rawBSP + surfs->fileofs);
  if (surfs->filelen % sizeof(*in)) err(ERR_DROP, "%s: funny lump size", __func__);
  i32 count;
  cm->numSurfaces = count = surfs->filelen / sizeof(*in);
  cm->surfaces            = alloc(cm->numSurfaces, sizeof(cm->surfaces[0]));
  dVert* dv               = (void*)(rawBSP + verts->fileofs);
  if (verts->filelen % sizeof(*dv)) err(ERR_DROP, "%s: funny lump size", __func__);
  // scan through all the surfaces, but only load patches, not planar faces
  cPatch* patch;
  for (i32 i = 0; i < count; i++, in++) {
    if (in->surfaceType != MST_PATCH) { continue; }  // ignore other surfaces
    // FIXME: check for non-colliding patches
    cm->surfaces[i] = patch = alloc(1, sizeof(*patch));
    // load the full drawverts onto the stack
    i32 width               = in->patchWidth;
    i32 height              = in->patchHeight;
    i32 c                   = width * height;
    if (c > MAX_PATCH_VERTS) { err(ERR_DROP, "%s: MAX_PATCH_VERTS", __func__); }
    dVert* dv_p = dv + in->firstVert;
    vec3   points[MAX_PATCH_VERTS];
    for (i32 j = 0; j < c; j++, dv_p++) {
      points[j][0] = dv_p->xyz[0];
      points[j][1] = dv_p->xyz[1];
      points[j][2] = dv_p->xyz[2];
    }
    i32 shaderNum       = in->shaderNum;
    patch->contents     = cm->shaders[shaderNum].contentFlags;
    patch->surfaceFlags = cm->shaders[shaderNum].surfaceFlags;
    // create the internal facet structure
    patch->pc           = CM_GeneratePatchCollide(width, height, points);
  }
}
#  endif  // disabled patches
//......................................................................................................................
#endif  // IMP_LoadBspNopatches
