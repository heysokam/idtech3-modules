#ifndef MEM_ZONE_H
#define MEM_ZONE_H
//..............................

// Engine dependencies
#include "../tools/core.h"
// Module dependencies
#include "./types.h"
#include "./cfg.h"
#include "./base.h"
#include "./state.h"

//..............................
// ZONE MEMORY ALLOCATION
//..............................
// There is never any space between memblocks,
// and there will never be two contiguous free memblocks.
//
// The rover can be left pointing at a non-empty block
//
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.
//..............................


//..............................
#ifdef ZONE_DEBUG
#  define Z_TagMalloc(size, tag) Z_TagMallocDebug(size, tag, #size, __FILE__, __LINE__)
#  define Z_Malloc(size) Z_MallocDebug(size, #size, __FILE__, __LINE__)
#  define S_Malloc(size) S_MallocDebug(size, #size, __FILE__, __LINE__)
void* Z_TagMallocDebug(int size, memtag_t tag, char* label, char* file, int line);  // NOT 0 filled memory
void* Z_MallocDebug(int size, char* label, char* file, int line);                   // returns 0 filled memory
void* S_MallocDebug(int size, char* label, char* file, int line);                   // returns 0 filled memory
#else
void* Z_TagMalloc(int size, memtag_t tag);  // NOT 0 filled memory
void* Z_Malloc(int size);                   // returns 0 filled memory
void* S_Malloc(int size);                   // NOT 0 filled memory only for small allocations
#endif
void Z_Free(void* ptr);
int  Z_FreeTags(memtag_t tag);
int  Z_AvailableMemory(void);

//..............................
// Initialize Zone Memory
void Com_InitSmallZoneMemory(void);
void Com_InitZoneMemory(void);


//..............................
#ifdef MEM_LOG
void Z_LogHeap(void);  // BROKEN
#endif


//..............................
#endif  // MEM_ZONE_H
