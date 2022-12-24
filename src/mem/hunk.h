#ifndef MEM_HUNK_H
#define MEM_HUNK_H
//..............................

// Engine dependencies
#include "../types.h"
#include "../tools/core.h"
// Memory module dependencies
#include "./base.h"
#include "./state.h"
#include "./zone.h"  // Depends on zone alloc for initializing

//..............................
// Hunk Goals:
//   Reproducible without history effects -- no out of memory errors on weird map to map changes
//   Allow restarting of the client without fragmentation
//   Minimize total pages in use at run time
//   Minimize total pages needed during load time
//
//   Single block of memory with stack allocators coming from both ends towards the middle.
//
//   One side is designated the temporary memory allocator.
//
//   Temporary memory can be allocated and freed in any order.
//
//   A highwater mark is kept of the most in use at any time.
//
//   When there is no temporary memory allocated, the permanent and temp sides
//   can be switched, allowing the already touched temp memory to be used for
//   permanent storage.
//
//   Temp memory must never be allocated on two ends at once, or fragmentation
//   could occur.
//
//   If we have any in-use temp memory, additional temp allocations must come from
//   that side.
//
//   If not, we can choose to make either side the new temp side and push future
//   permanent allocations to the other side.  Permanent allocations should be
//   kept on the side that has the current greatest wasted highwater mark.
//..............................

#define HUNK_MAGIC 0x89537892
#define HUNK_FREE_MAGIC 0x89537893

//..............................
#if defined _DEBUG
#  define HUNK_DEBUG
#endif
//..............................

//..............................
#ifdef HUNK_DEBUG
#  define Hunk_Alloc(size, preference) Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void* Hunk_AllocDebug(i32 size, ha_pref preference, char* label, char* file, i32 line);
#else
void* Hunk_Alloc(i32 size, ha_pref preference);
#endif
//..............................
void  Hunk_Clear(void);
void  Hunk_ClearToMark(void);
void  Hunk_SetMark(void);
bool  Hunk_CheckMark(void);
void  Hunk_ClearTempMemory(void);
void* Hunk_AllocateTempMemory(int size);
void  Hunk_FreeTempMemory(void* buf);
int   Hunk_MemoryRemaining(void);

//..............................
// Initialize Hunk Memory
// Data is stored in state.c
void Com_InitHunkMemory(void);

//..............................
#ifdef MEM_LOG
void Hunk_Log(void);  // BROKEN
#endif  // MEM_LOG


//..............................
#endif  // MEM_HUNK_H
