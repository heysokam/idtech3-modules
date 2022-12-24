#include "../state.h"

//..............................
// Cvar replacement
//..............................
MemCfg mem;
//..............................
// Mem_InitCfg
//   Initializes the memory runtime config variables to their default values.
//..............................
void Mem_InitCfg(void) {
  mem.zoneMegs  = DEF_COMZONEMEGS;  // was com_zoneMegs "XSTRING(DEF_COMZONEMEGS)"
  mem.hunkMegs  = DEF_COMHUNKMEGS;  // The size of the hunk memory segment. was com_hunkMegs "XSTRING(DEF_COMHUNKMEGS)"
  mem.developer = 1;                // was Com_DPrintf alternate function
}

//..............................
// Runtime Config
//..............................
i32 minfragment = MINFRAGMENT;  // may be adjusted at runtime

//..............................
// Zone State
//..............................
// main zone for all "dynamic" memory allocation
memzone_t* mainzone;
//..............................
// Small zone for small allocations that would only fragment the main zone
// (think of cvar and cmd strings)
memzone_t* smallzone;

//..............................
// Hunk State
//..............................
byte*        s_hunkData = NULL;
i32          s_hunkTotal;
hunkblock_t* hunkblocks;
hunkUsed_t   hunk_low;
hunkUsed_t   hunk_high;
hunkUsed_t*  hunk_permanent;
hunkUsed_t*  hunk_temp;
//..............................
i32 fs_loadStack;  // total files in memory
//..............................
i32 GetLoadStack(void) { return fs_loadStack; }

//..............................
static const char* tagName[TAG_COUNT] = { "FREE",   "GENERAL",  "PACK",    "SEARCH-PATH", "SEARCH-PACK", "SEARCH-DIR",
                                          "BOTLIB", "RENDERER", "CLIENTS", "SMALL",       "STATIC" };


//..............................
// Debugging / Statistics
//..............................

//..............................
// Zone_Stats
//..............................
static void Zone_Stats(const char* name, const memzone_t* z, bool printDetails, zone_stats_t* stats) {
  const memblock_t* block;
  const memzone_t*  zone;
  zone_stats_t      st;

  memset(&st, 0, sizeof(st));
  zone            = z;
  st.zoneSegments = 1;
  st.freeSmallest = 0x7FFFFFFF;

  // if ( printDetails ) {
  //  echo( "---------- %s zone segment #%i ----------\n", name, zone->segnum );
  // }

  for (block = zone->blocklist.next;;) {
    if (printDetails) {
      i32 tag = block->tag;
      echo("block:%p  size:%8i  tag: %s\n", (void*)block, block->size, (unsigned)tag < TAG_COUNT ? tagName[tag] : f("%i", tag));
    }
    if (block->tag != TAG_FREE) {
      st.zoneBytes += block->size;
      st.zoneBlocks++;
      if (block->tag == TAG_BOTLIB) {
        st.botlibBytes += block->size;
      } else if (block->tag == TAG_RENDERER) {
        st.rendererBytes += block->size;
      }
    } else {
      st.freeBytes += block->size;
      st.freeBlocks++;
      if (block->size > st.freeLargest) st.freeLargest = block->size;
      if (block->size < st.freeSmallest) st.freeSmallest = block->size;
    }
    if (block->next == &zone->blocklist) {
      break;  // all blocks have been hit
    }
    if ((byte*)block + block->size != (byte*)block->next) {
#ifdef USE_MULTI_SEGMENT
      const memblock_t* next = block->next;
      if (next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL) {
        st.zoneSegments++;
        if (printDetails) { echo("---------- %s zone segment #%i ----------\n", name, st.zoneSegments); }
        block = next->next;
        continue;
      } else
#endif
        echo("ERROR: block size does not touch the next block\n");
    }
    if (block->next->prev != block) { echo("ERROR: next block doesn't have proper back link\n"); }
    if (block->tag == TAG_FREE && block->next->tag == TAG_FREE) { echo("ERROR: two consecutive free blocks\n"); }
    block = block->next;
  }
  // export stats
  if (stats) { memcpy(stats, &st, sizeof(*stats)); }
}

//..............................
// Com_Meminfo_f :
//   Prints memory information.
//   arg1 accepts the next zone-tags, and will print extra information for them if given
//     "main", "small", "" or "all"
//   Mods:
//   Renamed to Com_Meminfo, and now takes one char* argument.
//   Before it was called through a Cmd, and took its Cmd_Argv(1)
//   Uses echo("msg") to print its information. (was Com_Printf)
//..............................
void Com_Meminfo(const char* arg1) {
  zone_stats_t st;
  i32          unused;

  echo("%8i bytes total hunk\n", s_hunkTotal);
  echo("\n");
  echo("%8i low mark\n", hunk_low.mark);
  echo("%8i low permanent\n", hunk_low.permanent);
  if (hunk_low.temp != hunk_low.permanent) { echo("%8i low temp\n", hunk_low.temp); }
  echo("%8i low tempHighwater\n", hunk_low.tempHighwater);
  echo("\n");
  echo("%8i high mark\n", hunk_high.mark);
  echo("%8i high permanent\n", hunk_high.permanent);
  if (hunk_high.temp != hunk_high.permanent) { echo("%8i high temp\n", hunk_high.temp); }
  echo("%8i high tempHighwater\n", hunk_high.tempHighwater);
  echo("\n");
  echo("%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent);
  unused = 0;
  if (hunk_low.tempHighwater > hunk_low.permanent) { unused += hunk_low.tempHighwater - hunk_low.permanent; }
  if (hunk_high.tempHighwater > hunk_high.permanent) { unused += hunk_high.tempHighwater - hunk_high.permanent; }
  echo("%8i unused highwater\n", unused);
  echo("\n");

  Zone_Stats("main", mainzone, !Q_stricmp(arg1, "main") || !Q_stricmp(arg1, "all"), &st);
  echo("%8i bytes total main zone\n\n", mainzone->size);
  echo("%8i bytes in %i main zone blocks%s\n", st.zoneBytes, st.zoneBlocks, st.zoneSegments > 1 ? f(" and %i segments", st.zoneSegments) : "");
  echo("        %8i bytes in botlib\n", st.botlibBytes);
  echo("        %8i bytes in renderer\n", st.rendererBytes);
  echo("        %8i bytes in other\n", st.zoneBytes - (st.botlibBytes + st.rendererBytes));
  echo("        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks);
  if (st.freeBlocks > 1) { echo("        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest); }

  Zone_Stats("small", smallzone, !Q_stricmp(arg1, "small") || !Q_stricmp(arg1, "all"), &st);
  echo("%8i bytes total small zone\n\n", smallzone->size);
  echo("%8i bytes in %i small zone blocks%s\n", st.zoneBytes, st.zoneBlocks, st.zoneSegments > 1 ? f(" and %i segments", st.zoneSegments) : "");
  echo("        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks);
  if (st.freeBlocks > 1) { echo("        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest); }
}
