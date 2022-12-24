#include "../zone.h"


//..............................
// General tools for Zone Alloc
//..............................

//..............................
#ifdef USE_MULTI_SEGMENT
//........
static void InitFree(freeblock_t* fb) {
  memblock_t* block = (memblock_t*)((byte*)fb - sizeof(memblock_t));
  Std_memset(block, 0, sizeof(*block));
}
//........
static void RemoveFree(memblock_t* block) {
  freeblock_t* fb = (freeblock_t*)(block + 1);
  freeblock_t* prev;
  freeblock_t* next;

#  ifdef ZONE_DEBUG
  if (fb->next == NULL || fb->prev == NULL || fb->next == fb || fb->prev == fb) {
    err(ERR_EXIT, "RemoveFree: bad pointers fb->next: %p, fb->prev: %p\n", fb->next, fb->prev);
  }
#  endif

  prev       = fb->prev;
  next       = fb->next;

  prev->next = next;
  next->prev = prev;
}
//........
static void InsertFree(memzone_t* zone, memblock_t* block) {
  freeblock_t* fb = (freeblock_t*)(block + 1);
  freeblock_t *prev, *next;
#  ifdef TINY_SIZE
  if (block->size <= TINY_SIZE) prev = &zone->freelist_tiny;
  else
#  endif
#  ifdef SMALL_SIZE
    if (block->size <= SMALL_SIZE)
    prev = &zone->freelist_small;
  else
#  endif
#  ifdef MEDIUM_SIZE
    if (block->size <= MEDIUM_SIZE)
    prev = &zone->freelist_medium;
  else
#  endif
    prev = &zone->freelist;

  next = prev->next;

#  ifdef ZONE_DEBUG
  if (block->size < sizeof(*fb) + sizeof(*block)) { err(ERR_EXIT, "InsertFree: bad block size: %i\n", block->size); }
#  endif

  prev->next = fb;
  next->prev = fb;

  fb->prev   = prev;
  fb->next   = next;
}
//........
// NewBlock
// Allocates new free block within specified memory zone
// Separator is needed to avoid additional runtime checks in Z_Free()
// to prevent merging it with previous free block
//........
static freeblock_t* NewBlock(memzone_t* zone, i32 size) {
  memblock_t *prev, *next;
  memblock_t *block, *sep;
  i32         alloc_size;

  // zone->prev is pointing on last block in the list
  prev       = zone->blocklist.prev;
  next       = prev->next;

  size       = PAD(size, 1 << 21);  // round up to 2M blocks
  // allocate separator block before new free block
  alloc_size = size + sizeof(*sep);

  sep        = (memblock_t*)calloc(alloc_size, 1);
  if (sep == NULL) {
    err(ERR_EXIT, "Z_Malloc: failed on allocation of %i bytes from the %s zone", size, zone == smallzone ? "small" : "main");
    return NULL;
  }
  block       = sep + 1;

  // link separator with prev
  prev->next  = sep;
  sep->prev   = prev;

  // link separator with block
  sep->next   = block;
  block->prev = sep;

  // link block with next
  block->next = next;
  next->prev  = block;

  sep->tag    = TAG_GENERAL;  // in-use block
  sep->id     = -ZONEID;
  sep->size   = 0;

  block->tag  = TAG_FREE;
  block->id   = ZONEID;
  block->size = size;

  // update zone statistics
  zone->size += alloc_size;
  zone->used += sizeof(*sep);

  InsertFree(zone, block);

  return (freeblock_t*)(block + 1);
}
//........
static memblock_t* SearchFree(memzone_t* zone, i32 size) {
  const freeblock_t* fb;
  memblock_t*        base;

#  ifdef TINY_SIZE
  if (size <= TINY_SIZE) fb = zone->freelist_tiny.DIRECTION;
  else
#  endif
#  ifdef SMALL_SIZE
    if (size <= SMALL_SIZE)
    fb = zone->freelist_small.DIRECTION;
  else
#  endif
#  ifdef MEDIUM_SIZE
    if (size <= MEDIUM_SIZE)
    fb = zone->freelist_medium.DIRECTION;
  else
#  endif
    fb = zone->freelist.DIRECTION;

  for (;;) {
    // not found, allocate new segment?
    if (fb == &zone->freelist) {
      fb = NewBlock(zone, size);
    } else {
#  ifdef TINY_SIZE
      if (fb == &zone->freelist_tiny) {
        fb = zone->freelist_small.DIRECTION;
        continue;
      }
#  endif
#  ifdef SMALL_SIZE
      if (fb == &zone->freelist_small) {
        fb = zone->freelist_medium.DIRECTION;
        continue;
      }
#  endif
#  ifdef MEDIUM_SIZE
      if (fb == &zone->freelist_medium) {
        fb = zone->freelist.DIRECTION;
        continue;
      }
#  endif
    }
    base = (memblock_t*)((byte*)fb - sizeof(*base));
    fb   = fb->DIRECTION;
    if (base->size >= size) { return base; }
  }
  return NULL;
}
//..............................
#endif  // USE_MULTI_SEGMENT
//..............................


//..............................
// Zone Memory Management
//..............................

//..............................
// Z_ClearZone
//..............................
static void Z_ClearZone(memzone_t* zone, memzone_t* head, i32 size, i32 segnum) {
  memblock_t* block;
  i32         min_fragment;

#ifdef USE_MULTI_SEGMENT
  min_fragment = sizeof(memblock_t) + sizeof(freeblock_t);
#else
  min_fragment = sizeof(memblock_t);
#endif

  if (minfragment < min_fragment) {
    // in debug mode size of memblock_t may exceed MINFRAGMENT
    minfragment = PAD(min_fragment, sizeof(intptr_t));
    if (mem.developer) echo("zone.minfragment adjusted to %i bytes\n", minfragment);  // sk.add -> Com_DPrintf replacement
  }

  // set the entire zone to one free block
  zone->blocklist.next = zone->blocklist.prev = block = (memblock_t*)(zone + 1);
  zone->blocklist.tag                                 = TAG_GENERAL;  // in use block
  zone->blocklist.id                                  = -ZONEID;
  zone->blocklist.size                                = 0;
#ifndef USE_MULTI_SEGMENT
  zone->rover = block;
#endif
  zone->size  = size;
  zone->used  = 0;

  block->prev = block->next = &zone->blocklist;
  block->tag                = TAG_FREE;  // free block
  block->id                 = ZONEID;

  block->size               = size - sizeof(memzone_t);

#ifdef USE_MULTI_SEGMENT
  InitFree(&zone->freelist);
  zone->freelist.next = zone->freelist.prev = &zone->freelist;

  InitFree(&zone->freelist_medium);
  zone->freelist_medium.next = zone->freelist_medium.prev = &zone->freelist_medium;

  InitFree(&zone->freelist_small);
  zone->freelist_small.next = zone->freelist_small.prev = &zone->freelist_small;

  InitFree(&zone->freelist_tiny);
  zone->freelist_tiny.next = zone->freelist_tiny.prev = &zone->freelist_tiny;

  InsertFree(zone, block);
#endif
}
//..............................


//..............................
// Z_AvailableZoneMemory
//..............................
static i32 Z_AvailableZoneMemory(const memzone_t* zone) {
#ifdef USE_MULTI_SEGMENT
  return (1024 * 1024 * 1024);  // unlimited
#else
  return zone->size - zone->used;
#endif
}
//..............................
// Z_AvailableMemory
//..............................
i32 Z_AvailableMemory(void) { return Z_AvailableZoneMemory(mainzone); }
//..............................
// MergeBlock
//..............................
static void MergeBlock(memblock_t* curr_free, const memblock_t* next) {
  curr_free->size += next->size;
  curr_free->next       = next->next;
  curr_free->next->prev = curr_free;
}
//..............................
// Z_Free
//..............................
void Z_Free(void* ptr) {
  memblock_t *block, *other;
  memzone_t*  zone;

  if (!ptr) { err(ERR_DROP, "Z_Free: NULL pointer"); }

  block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
  if (block->id != ZONEID) { err(ERR_EXIT, "Z_Free: freed a pointer without ZONEID"); }

  if (block->tag == TAG_FREE) { err(ERR_EXIT, "Z_Free: freed a freed pointer"); }

  // if static memory
#ifdef USE_STATIC_TAGS
  if (block->tag == TAG_STATIC) { return; }
#endif

  // check the memory trash tester
#ifdef USE_TRASH_TEST
  if (*(i32*)((byte*)block + block->size - 4) != ZONEID) { err(ERR_EXIT, "Z_Free: memory block wrote past end"); }
#endif

  if (block->tag == TAG_SMALL) {
    zone = smallzone;
  } else {
    zone = mainzone;
  }

  zone->used -= block->size;

  // set the block to something that should cause problems
  // if it is referenced...
  Std_memset(ptr, 0xaa, block->size - sizeof(*block));

  block->tag = TAG_FREE;  // mark as free
  block->id  = ZONEID;

  other      = block->prev;
  if (other->tag == TAG_FREE) {
#ifdef USE_MULTI_SEGMENT
    RemoveFree(other);
#endif
    // merge with previous free block
    MergeBlock(other, block);
#ifndef USE_MULTI_SEGMENT
    if (block == zone->rover) { zone->rover = other; }
#endif
    block = other;
  }

#ifndef USE_MULTI_SEGMENT
  zone->rover = block;
#endif

  other = block->next;
  if (other->tag == TAG_FREE) {
#ifdef USE_MULTI_SEGMENT
    RemoveFree(other);
#endif
    // merge the next free block onto the end
    MergeBlock(block, other);
  }

#ifdef USE_MULTI_SEGMENT
  InsertFree(zone, block);
#endif
}

//..............................
// Z_FreeTags
//..............................
i32 Z_FreeTags(memtag_t tag) {
  memzone_t* zone;
  if (tag == TAG_STATIC) {
    err(ERR_EXIT, "Z_FreeTags( TAG_STATIC )");
    return 0;
  } else if (tag == TAG_SMALL) {
    zone = smallzone;
  } else {
    zone = mainzone;
  }

  i32         count = 0;
  memblock_t *block, *freed;
  for (block = zone->blocklist.next;;) {
    if (block->tag == tag && block->id == ZONEID) {
      if (block->prev->tag == TAG_FREE) freed = block->prev;  // current block will be merged with previous
      else freed = block;                                     // will leave in place
      Z_Free((void*)(block + 1));
      block = freed;
      count++;
    }
    if (block->next == &zone->blocklist) { break; }  // all blocks have been hit
    block = block->next;
  }

  return count;
}


//..............................
// Z_TagMalloc
//..............................
#ifdef ZONE_DEBUG
void* Z_TagMallocDebug(i32 size, memtag_t tag, char* label, char* file, i32 line) {
  i32 allocSize;
#else
void* Z_TagMalloc(i32 size, memtag_t tag) {
#endif
  i32 extra;
#ifndef USE_MULTI_SEGMENT
  memblock_t *start, *rover;
#endif
  memblock_t* base;
  memzone_t*  zone;

  if (tag == TAG_FREE) { err(ERR_EXIT, "Z_TagMalloc: tried to use with TAG_FREE"); }

  if (tag == TAG_SMALL) {
    zone = smallzone;
  } else {
    zone = mainzone;
  }

#ifdef ZONE_DEBUG
  allocSize = size;
#endif

#ifdef USE_MULTI_SEGMENT
  if (size < (sizeof(freeblock_t))) { size = (sizeof(freeblock_t)); }
#endif

  //
  // scan through the block list looking for the first free block
  // of sufficient size
  size += sizeof(*base);  // account for size of block header
#ifdef USE_TRASH_TEST
  size += 4;  // space for memory trash tester
#endif

  size = PAD(size, sizeof(intptr_t));  // align to 32/64 bit boundary

#ifdef USE_MULTI_SEGMENT
  base = SearchFree(zone, size);
  RemoveFree(base);
#else

  base = rover = zone->rover;
  start = base->prev;

  do {
    if (rover == start) {
      // scaned all the way around the list
#  ifdef ZONE_DEBUG
      Z_LogHeap();
      err(ERR_EXIT,
          "Z_Malloc: failed on allocation of %i bytes from the %s zone: %s, line: %d (%s)",
          size,
          zone == smallzone ? "small" : "main",
          file,
          line,
          label);
#  else
      err(ERR_EXIT, "Z_Malloc: failed on allocation of %i bytes from the %s zone", size, zone == smallzone ? "small" : "main");
#  endif
      return NULL;
    }
    if (rover->tag != TAG_FREE) {
      base = rover = rover->next;
    } else {
      rover = rover->next;
    }
  } while (base->tag != TAG_FREE || base->size < size);
#endif

  //
  // found a block big enough
  //
  extra = base->size - size;
  if (extra >= minfragment) {
    memblock_t* fragment;
    // there will be a free fragment after the allocated block
    fragment             = (memblock_t*)((byte*)base + size);
    fragment->size       = extra;
    fragment->tag        = TAG_FREE;  // free block
    fragment->id         = ZONEID;
    fragment->prev       = base;
    fragment->next       = base->next;
    fragment->next->prev = fragment;
    base->next           = fragment;
    base->size           = size;
#ifdef USE_MULTI_SEGMENT
    InsertFree(zone, fragment);
#endif
  }

#ifndef USE_MULTI_SEGMENT
  zone->rover = base->next;  // next allocation will start looking here
#endif
  zone->used += base->size;

  base->tag = tag;  // no longer a free block
  base->id  = ZONEID;

#ifdef ZONE_DEBUG
  base->d.label     = label;
  base->d.file      = file;
  base->d.line      = line;
  base->d.allocSize = allocSize;
#endif

#ifdef USE_TRASH_TEST
  // marker for memory trash testing
  *(i32*)((byte*)base + base->size - 4) = ZONEID;
#endif

  return (void*)(base + 1);
}

//..............................
// Z_CheckHeap
//..............................
static void Z_CheckHeap(void) {
  const memblock_t* block;
  const memzone_t*  zone;

  zone = mainzone;
  for (block = zone->blocklist.next;;) {
    if (block->next == &zone->blocklist) {
      break;  // all blocks have been hit
    }
    if ((byte*)block + block->size != (byte*)block->next) {
#ifdef USE_MULTI_SEGMENT
      const memblock_t* next = block->next;
      if (next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL) {
        block = next;  // new zone segment
      } else
#endif
        err(ERR_EXIT, "Z_CheckHeap: block size does not touch the next block");
    }
    if (block->next->prev != block) { err(ERR_EXIT, "Z_CheckHeap: next block doesn't have proper back link"); }
    if (block->tag == TAG_FREE && block->next->tag == TAG_FREE) { err(ERR_EXIT, "Z_CheckHeap: two consecutive free blocks"); }
    block = block->next;
  }
}

//..............................
// Z_Malloc
//..............................
#ifdef ZONE_DEBUG
void* Z_MallocDebug(i32 size, char* label, char* file, i32 line) {
#else
void* Z_Malloc(i32 size) {
#endif
  void* buf;
  Z_CheckHeap();

#ifdef ZONE_DEBUG
  buf = Z_TagMallocDebug(size, TAG_GENERAL, label, file, line);
#else
  buf = Z_TagMalloc(size, TAG_GENERAL);
#endif
  Std_memset(buf, 0, size);

  return buf;
}

//..............................
// S_Malloc
//..............................
#ifdef ZONE_DEBUG
void* S_MallocDebug(i32 size, char* label, char* file, i32 line) { return Z_TagMallocDebug(size, TAG_SMALL, label, file, line); }
#else
void* S_Malloc(i32 size) { return Z_TagMalloc(size, TAG_SMALL); }
#endif

//..............................
// Com_TouchMemory
//   Touch all known used data to make sure it is paged in
//..............................
void Com_TouchMemory(void) {
  const memblock_t* block;
  const memzone_t*  zone;
  i32               start, end;
  i32               i, j;
  u32               sum;

  Z_CheckHeap();

  start = msec();

  sum   = 0;

  j     = hunk_low.permanent >> 2;
  for (i = 0; i < j; i += 64) {  // only need to touch each page
    sum += ((u32*)s_hunkData)[i];
  }

  i = (s_hunkTotal - hunk_high.permanent) >> 2;
  j = hunk_high.permanent >> 2;
  for (; i < j; i += 64) {  // only need to touch each page
    sum += ((u32*)s_hunkData)[i];
  }

  zone = mainzone;
  for (block = zone->blocklist.next;; block = block->next) {
    if (block->tag != TAG_FREE) {
      j = block->size >> 2;
      for (i = 0; i < j; i += 64) {  // only need to touch each page
        sum += ((u32*)block)[i];
      }
    }
    if (block->next == &zone->blocklist) {
      break;  // all blocks have been hit
    }
  }

  end = msec();

  echo("Com_TouchMemory: %i msec\n", end - start);
}


//..............................
// Com_InitSmallZoneMemory
//..............................
void Com_InitSmallZoneMemory(void) {
  static byte s_buf[512 * 1024];
  i32         smallZoneSize;

  smallZoneSize = sizeof(s_buf);
  Std_memset(s_buf, 0, smallZoneSize);
  smallzone = (memzone_t*)s_buf;
  Z_ClearZone(smallzone, smallzone, smallZoneSize, 1);
}
//..............................
// Com_InitZoneMemory
//..............................
void Com_InitZoneMemory(void) {
  // Please note: com_zoneMegs can only be set on the command line, and
  // not in q3config.cfg or Com_StartupVariable, as they haven't been
  // executed by this point. It's a chicken and egg problem. We need the
  // memory manager configured to handle those places where you would
  // configure the memory manager.

  // allocate the random block zone
  // cvar_t* cv = Cvar_Get("com_zoneMegs", XSTRING(DEF_COMZONEMEGS), CVAR_LATCH | CVAR_ARCHIVE);
  // Cvar_CheckRange(cv, "1", NULL, CV_INTEGER);
  if (mem.zoneMegs < 1) mem.zoneMegs = 1;  // sk.add -> cvar replacement

  i32 mainZoneSize;
#ifndef USE_MULTI_SEGMENT
  // if (cv->integer < DEF_COMZONEMEGS) mainZoneSize = 1024 * 1024 * DEF_COMZONEMEGS;
  if (mem.zoneMegs < DEF_COMZONEMEGS) mainZoneSize = 1024 * 1024 * DEF_COMZONEMEGS;
  else
#endif
    // mainZoneSize = cv->integer * 1024 * 1024;
    mainZoneSize = mem.zoneMegs * 1024 * 1024;

  mainzone = calloc(mainZoneSize, 1);
  if (!mainzone) { err(ERR_EXIT, "Zone data failed to allocate %i megs", mainZoneSize / (1024 * 1024)); }
  Z_ClearZone(mainzone, mainzone, mainZoneSize, 1);
}


//..............................
#ifdef MEM_LOG
//..............................
// Z_LogZoneHeap
//   BROKEN: Needs either refactoring for stdlib, or extraction of the engine's filesystem
//..............................
static void Z_LogZoneHeap(memzone_t* zone, const char* name) {
#  ifdef ZONE_DEBUG
  char dump[32], *ptr;
  i32  i, j;
#  endif
  memblock_t* block;
  char        buf[4096];
  i32         size, allocSize, numBlocks;
  i32         len;

  if (logfile == FS_INVALID_HANDLE || !FS_Initialized()) return;

  size = numBlocks = 0;
#  ifdef ZONE_DEBUG
  allocSize = 0;
#  endif
  len = sprintf(buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name);
  FS_Write(buf, len, logfile);
  for (block = zone->blocklist.next;;) {
    if (block->tag != TAG_FREE) {
#  ifdef ZONE_DEBUG
      ptr = ((char*)block) + sizeof(memblock_t);
      j   = 0;
      for (i = 0; i < 20 && i < block->d.allocSize; i++) {
        if (ptr[i] >= 32 && ptr[i] < 127) {
          dump[j++] = ptr[i];
        } else {
          dump[j++] = '_';
        }
      }
      dump[j] = '\0';
      len     = sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
      FS_Write(buf, len, logfile);
      allocSize += block->d.allocSize;
#  endif
      size += block->size;
      numBlocks++;
    }
    if (block->next == &zone->blocklist) {
      break;  // all blocks have been hit
    }
    block = block->next;
  }
#  ifdef ZONE_DEBUG
  // subtract debug memory
  size -= numBlocks * sizeof(zonedebug_t);
#  else
  allocSize = numBlocks * sizeof(memblock_t);  // + 32 bit alignment
#  endif
  len = sprintf(buf, sizeof(buf), "%d %s memory in %d blocks\r\n", size, name, numBlocks);
  FS_Write(buf, len, logfile);
  len = sprintf(buf, sizeof(buf), "%d %s memory overhead\r\n", size - allocSize, name);
  FS_Write(buf, len, logfile);
  FS_Flush(logfile);
}
//..............................
// Z_LogHeap
//   BROKEN: Needs either refactoring for stdlib, or extraction of the engine's filesystem
//..............................
void Z_LogHeap(void) {
  Z_LogZoneHeap(mainzone, "MAIN");
  Z_LogZoneHeap(smallzone, "SMALL");
}
//..............................
#endif  // MEM_LOG
//..............................


//..............................
#ifdef USE_STATIC_TAGS
// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
  memblock_t b;
  byte       mem[2];
} memstatic_t;
//..............................
#  define MEM_STATIC(chr)                                                            \
    {                                                                                \
      { NULL, NULL, PAD(sizeof(memstatic_t), 4), TAG_STATIC, ZONEID }, { chr, '\0' } \
    }
//..............................
static const memstatic_t emptystring    = MEM_STATIC('\0');
static const memstatic_t numberstring[] = { MEM_STATIC('0'), MEM_STATIC('1'), MEM_STATIC('2'), MEM_STATIC('3'), MEM_STATIC('4'),
                                            MEM_STATIC('5'), MEM_STATIC('6'), MEM_STATIC('7'), MEM_STATIC('8'), MEM_STATIC('9') };
#endif  // USE_STATIC_TAGS
//..............................


//..............................
// CopyString
//  NOTE: never write over the memory CopyString returns because
//        memory from a memstatic_t might be returned
//..............................
char* CopyString(const char* in) {
  char* out;
#ifdef USE_STATIC_TAGS
  if (!in[0]) {
    return ((char*)&emptystring) + sizeof(memblock_t);
  } else if (!in[1]) {
    if (in[0] >= '0' && in[0] <= '9') { return ((char*)&numberstring[in[0] - '0']) + sizeof(memblock_t); }
  }
#endif
  out = S_Malloc(strlen(in) + 1);
  strcpy(out, in);
  return out;
}

