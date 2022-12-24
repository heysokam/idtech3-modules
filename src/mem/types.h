#ifndef MEM_TYPES_H
#define MEM_TYPES_H
//..............................

#include "../types.h"
#include "./cfg.h"

//............................
// Cvar replacement
//............................
typedef struct MemCfg_s {
  int zoneMegs;   // was com_zoneMegs "XSTRING(DEF_COMZONEMEGS)"
  int hunkMegs;   // The size of the hunk memory segment. was com_hunkMegs "XSTRING(DEF_COMHUNKMEGS)"
  int developer;  // was Com_DPrintf alternate function
} MemCfg;


//..............................
// Zone Types
//..............................

//..............................
typedef struct zone_stats_s {
  i32 zoneSegments;
  i32 zoneBlocks;
  i32 zoneBytes;
  i32 botlibBytes;
  i32 rendererBytes;
  i32 freeBytes;
  i32 freeBlocks;
  i32 freeSmallest;
  i32 freeLargest;
} zone_stats_t;

//..............................
typedef enum {
  TAG_FREE,
  TAG_GENERAL,
  TAG_PACK,
  TAG_SEARCH_PATH,
  TAG_SEARCH_PACK,
  TAG_SEARCH_DIR,
  TAG_BOTLIB,
  TAG_RENDERER,
  TAG_CLIENTS,
  TAG_SMALL,
  TAG_STATIC,
  TAG_COUNT
} memtag_t;

//..............................
typedef struct freeblock_s {
  struct freeblock_s* prev;
  struct freeblock_s* next;
} freeblock_t;

//..............................
typedef struct memblock_s {
  struct memblock_s *next, *prev;
  i32                size;  // including the header and possibly tiny fragments
  memtag_t           tag;   // a tag of 0 is a free block
  i32                id;    // should be ZONEID
#ifdef ZONE_DEBUG
  zonedebug_t d;
#endif
} memblock_t;

//..............................
typedef struct memzone_s {
  i32        size;       // total bytes malloced, including header
  i32        used;       // total bytes used
  memblock_t blocklist;  // start / end cap for linked list
#ifdef USE_MULTI_SEGMENT
  memblock_t  dummy0;  // just to allocate some space before freelist
  freeblock_t freelist_tiny;
  memblock_t  dummy1;
  freeblock_t freelist_small;
  memblock_t  dummy2;
  freeblock_t freelist_medium;
  memblock_t  dummy3;
  freeblock_t freelist;
#else
  memblock_t* rover;
#endif
} memzone_t;

//..............................
#ifdef ZONE_DEBUG
typedef struct zonedebug_s {
  char* label;
  char* file;
  i32   line;
  i32   allocSize;
} zonedebug_t;
#endif


//..............................
// Hunk Types
//..............................

//..............................
typedef struct {
  u32 magic;
  u32 size;
} hunkHeader_t;
//..............................
typedef struct {
  i32 mark;
  i32 permanent;
  i32 temp;
  i32 tempHighwater;
} hunkUsed_t;
//..............................
typedef struct hunkblock_s {
  i32                 size;
  byte                printed;
  struct hunkblock_s* next;
  char*               label;
  char*               file;
  i32                 line;
} hunkblock_t;
//..............................
typedef enum { h_high, h_low, h_dontcare } ha_pref;
//..............................


//..............................
#endif  // MEM_TYPES_H
