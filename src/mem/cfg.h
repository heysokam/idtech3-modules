#ifndef MEM_CFG_H
#define MEM_CFG_H
//....................

//....................
// Compile time Memory Manager config
//....................

//....................
// Zone Config
#define USE_STATIC_TAGS
#define USE_TRASH_TEST


#define MINFRAGMENT 64

#define USE_MULTI_SEGMENT  // allocate additional zone segments on demand
#ifdef USE_MULTI_SEGMENT
#  if 1  // forward lookup, faster allocation
#    define DIRECTION next
// we may have up to 4 lists to group free blocks by size
// #define TINY_SIZE	32
#    define SMALL_SIZE 64
#    define MEDIUM_SIZE 128
#  else  // backward lookup, better free space consolidation
#    define DIRECTION prev
#    define TINY_SIZE 64
#    define SMALL_SIZE 128
#    define MEDIUM_SIZE 256
#  endif
#endif

#ifdef USE_MULTI_SEGMENT
#  define DEF_COMZONEMEGS 12
#else
#  define DEF_COMZONEMEGS 25
#endif

#define ZONEID 0x1d4a11

#if defined _DEBUG
#  define ZONE_DEBUG
#endif

//....................
// Hunk Config
#ifdef DEDICATED
#  define MIN_COMHUNKMEGS 48
#  define DEF_COMHUNKMEGS 56
#else
#  define MIN_COMHUNKMEGS 128
#  define DEF_COMHUNKMEGS 512
#endif

//....................
#endif  // MEM_CFG_H
