#ifndef MEM_CORE_H
#define MEM_CORE_H
//............................

//....................
// Memory model:
//....................
// --- low memory ----
//  server vm
//  server clipmap
// ---mark---
//  renderer initialization (shaders, etc)
//  UI vm
//  cgame vm
//  renderer map
//  renderer models
//
// ---free---
//
//  temp file loading
// --- high memory ---
//....................

#include "./types.h"
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
#include "./zone.h"

//..............................
// HUNK MEMORY ALLOCATION
//..............................
// Goals:
//   reproducible without history effects -- no out of memory errors on weird map to map changes
//   allow restarting of the client without fragmentation
//   minimize total pages in use at run time
//   minimize total pages needed during load time
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
#include "./hunk.h"  // Depends on zone alloc for initialization

//............................
#endif  // MEM_CORE_H
