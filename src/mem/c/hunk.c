#include "../hunk.h"

//..............................
// Hunk_Clear
// The server calls this before shutting down or loading a new map
void Hunk_Clear(void) {
  // #ifndef DEDICATED
  //   CL_ShutdownCGame();
  //   CL_ShutdownUI();
  // #endif
  //   SV_ShutdownGameProgs();
  // #ifndef DEDICATED
  //   CIN_CloseAllVideos();
  // #endif
  hunk_low.mark           = 0;
  hunk_low.permanent      = 0;
  hunk_low.temp           = 0;
  hunk_low.tempHighwater  = 0;

  hunk_high.mark          = 0;
  hunk_high.permanent     = 0;
  hunk_high.temp          = 0;
  hunk_high.tempHighwater = 0;

  hunk_permanent          = &hunk_low;
  hunk_temp               = &hunk_high;

  echo("Hunk_Clear: reset the hunk ok");
  // VM_Clear();
#ifdef HUNK_DEBUG
  hunkblocks = NULL;
#endif
}


//..............................
static void Hunk_SwapBanks(void) {
  hunkUsed_t* swap;

  // can't swap banks if there is any temp already allocated
  if (hunk_temp->temp != hunk_temp->permanent) { return; }

  // if we have a larger highwater mark on this side, start making
  // our permanent allocations here and use the other side for temp
  if (hunk_temp->tempHighwater - hunk_temp->permanent > hunk_permanent->tempHighwater - hunk_permanent->permanent) {
    swap           = hunk_temp;
    hunk_temp      = hunk_permanent;
    hunk_permanent = swap;
  }
}

//..............................
// Hunk_Alloc
// Allocate permanent (until the hunk is cleared) memory
//..............................
#ifdef HUNK_DEBUG
void* Hunk_AllocDebug(i32 size, ha_pref preference, char* label, char* file, i32 line) {
#else
void* Hunk_Alloc(i32 size, ha_pref preference) {
#endif
  void* buf;

  if (s_hunkData == NULL) { err(ERR_EXIT, "%s: Hunk memory system not initialized", __func__); }

  // can't do preference if there is any temp allocated
  if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
    Hunk_SwapBanks();
  } else {
    if (preference == h_low && hunk_permanent != &hunk_low) {
      Hunk_SwapBanks();
    } else if (preference == h_high && hunk_permanent != &hunk_high) {
      Hunk_SwapBanks();
    }
  }

#ifdef HUNK_DEBUG
  size += sizeof(hunkblock_t);
#endif

  // round to cacheline
  size = PAD(size, 64);

  if (hunk_low.temp + hunk_high.temp + size > s_hunkTotal) {
#ifdef HUNK_DEBUG
#  ifdef MEM_LOG
    Hunk_Log();
    Hunk_SmallLog();
#  endif  // MEM_LOG
    err(ERR_DROP, "%s failed on %i: %s, line: %d (%s)", __func__, size, file, line, label);
#else
    err(ERR_DROP, "%s failed on %i", __func__, size);
#endif
  }

  if (hunk_permanent == &hunk_low) {
    buf = (void*)(s_hunkData + hunk_permanent->permanent);
    hunk_permanent->permanent += size;
  } else {
    hunk_permanent->permanent += size;
    buf = (void*)(s_hunkData + s_hunkTotal - hunk_permanent->permanent);
  }

  hunk_permanent->temp = hunk_permanent->permanent;

  Std_memset(buf, 0, size);

#ifdef HUNK_DEBUG
  {
    hunkblock_t* block;

    block        = (hunkblock_t*)buf;
    block->size  = size - sizeof(hunkblock_t);
    block->file  = file;
    block->label = label;
    block->line  = line;
    block->next  = hunkblocks;
    hunkblocks   = block;
    buf          = ((byte*)buf) + sizeof(hunkblock_t);
  }
#endif
  return buf;
}

//..............................
// Hunk_AllocateTempMemory
//   This is used by the file loading system.
//   Multiple files can be loaded in temporary memory.
//   When the files-in-use count reaches zero, all temp memory will be deleted
//..............................
void* Hunk_AllocateTempMemory(i32 size) {
  void*         buf;
  hunkHeader_t* hdr;

  // return a Z_Malloc'd block if the hunk has not been initialized
  // this allows the config and product id files ( journal files too ) to be loaded
  // by the file system without redunant routines in the file system utilizing different
  // memory systems
  if (s_hunkData == NULL) { return Z_Malloc(size); }

  Hunk_SwapBanks();

  size = PAD(size, sizeof(intptr_t)) + sizeof(hunkHeader_t);

  if (hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal) { err(ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", size); }

  if (hunk_temp == &hunk_low) {
    buf = (void*)(s_hunkData + hunk_temp->temp);
    hunk_temp->temp += size;
  } else {
    hunk_temp->temp += size;
    buf = (void*)(s_hunkData + s_hunkTotal - hunk_temp->temp);
  }

  if (hunk_temp->temp > hunk_temp->tempHighwater) { hunk_temp->tempHighwater = hunk_temp->temp; }

  hdr        = (hunkHeader_t*)buf;
  buf        = (void*)(hdr + 1);

  hdr->magic = HUNK_MAGIC;
  hdr->size  = size;

  // don't bother clearing, because we are going to load a file over it
  return buf;
}


//..............................
// Hunk_FreeTempMemory
//..............................
void Hunk_FreeTempMemory(void* buf) {
  hunkHeader_t* hdr;

  // free with Z_Free if the hunk has not been initialized
  // this allows the config and product id files ( journal files too ) to be loaded
  // by the file system without redunant routines in the file system utilizing different
  // memory systems
  if (s_hunkData == NULL) {
    Z_Free(buf);
    return;
  }

  hdr = ((hunkHeader_t*)buf) - 1;
  if (hdr->magic != HUNK_MAGIC) { err(ERR_EXIT, "Hunk_FreeTempMemory: bad magic"); }

  hdr->magic = HUNK_FREE_MAGIC;

  // this only works if the files are freed in stack order,
  // otherwise the memory will stay around until Hunk_ClearTempMemory
  if (hunk_temp == &hunk_low) {
    if (hdr == (void*)(s_hunkData + hunk_temp->temp - hdr->size)) {
      hunk_temp->temp -= hdr->size;
    } else {
      echo("%s: not the final block\n", __func__);
    }
  } else {
    if (hdr == (void*)(s_hunkData + s_hunkTotal - hunk_temp->temp)) {
      hunk_temp->temp -= hdr->size;
    } else {
      echo("%s: not the final block\n", __func__);
    }
  }
}


//..............................
// Hunk_ClearTempMemory
//   The temp space is no longer needed.  If we have left more
//   touched but unused memory on this side, have future
//   permanent allocs use this side.
//..............................
void Hunk_ClearTempMemory(void) {
  if (s_hunkData != NULL) { hunk_temp->temp = hunk_temp->permanent; }
}

//..............................
#ifdef MEM_LOG
//..............................
// Hunk_Log
//   BROKEN: Needs either refactoring for stdlib, or extraction of the engine's filesystem
//..............................
void Hunk_Log(void) {
  hunkblock_t* block;
  char         buf[4096];
  i32          size, numBlocks;

  if (logfile == FS_INVALID_HANDLE || !FS_Initialized()) return;

  size      = 0;
  numBlocks = 0;
  sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
  FS_Write(buf, strlen(buf), logfile);
  for (block = hunkblocks; block; block = block->next) {
#  ifdef HUNK_DEBUG
    sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
    FS_Write(buf, strlen(buf), logfile);
#  endif
    size += block->size;
    numBlocks++;
  }
  sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
  FS_Write(buf, strlen(buf), logfile);
  sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
  FS_Write(buf, strlen(buf), logfile);
}


//..............................
// Hunk_SmallLog
//   BROKEN: Needs either refactoring for stdlib, or extraction of the engine's filesystem
//..............................
#  ifdef HUNK_DEBUG
void Hunk_SmallLog(void) {
  hunkblock_t *block, *block2;
  char         buf[4096];
  i32          size, locsize, numBlocks;

  if (logfile == FS_INVALID_HANDLE || !FS_Initialized()) return;

  for (block = hunkblocks; block; block = block->next) { block->printed = false; }
  size      = 0;
  numBlocks = 0;
  sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
  FS_Write(buf, strlen(buf), logfile);
  for (block = hunkblocks; block; block = block->next) {
    if (block->printed) { continue; }
    locsize = block->size;
    for (block2 = block->next; block2; block2 = block2->next) {
      if (block->line != block2->line) { continue; }
      if (Q_stricmp(block->file, block2->file)) { continue; }
      size += block2->size;
      locsize += block2->size;
      block2->printed = true;
    }
    sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
    FS_Write(buf, strlen(buf), logfile);
    size += block->size;
    numBlocks++;
  }
  sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
  FS_Write(buf, strlen(buf), logfile);
  sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
  FS_Write(buf, strlen(buf), logfile);
}
#  endif  // HUNK_DEBUG
//..............................
#endif  // MEM_LOG
//..............................


//..............................
// Hunk_MemoryRemaining
//..............................
i32 Hunk_MemoryRemaining(void) {
  i32 low, high;

  low  = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
  high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

  return s_hunkTotal - (low + high);
}
//..............................
// Hunk_SetMark
// The server calls this after the level and game VM have been loaded
//..............................
void Hunk_SetMark(void) {
  hunk_low.mark  = hunk_low.permanent;
  hunk_high.mark = hunk_high.permanent;
}
//..............................
// Hunk_ClearToMark
// The client calls this before starting a vid_restart or snd_restart
//..............................
void Hunk_ClearToMark(void) {
  hunk_low.permanent = hunk_low.temp = hunk_low.mark;
  hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}
//..............................
// Hunk_CheckMark
//..............................
bool Hunk_CheckMark(void) {
  if (hunk_low.mark || hunk_high.mark) { return true; }
  return false;
}

//..............................
// Com_InitHunkMemory
void Com_InitHunkMemory(void) {
  // make sure the file system has allocated and "not" freed any temp blocks
  // this allows the config and product id files ( journal files too ) to be loaded
  // by the file system without redunant routines in the file system utilizing different
  // memory systems
  if (GetLoadStack() != 0) { err(ERR_EXIT, "Hunk initialization failed. File system load stack not zero"); }

  // allocate the stack based hunk allocator
  // cvar_t* cv = Cvar_Get("com_hunkMegs", XSTRING(DEF_COMHUNKMEGS), CVAR_LATCH | CVAR_ARCHIVE);
  // Cvar_CheckRange(cv, XSTRING(MIN_COMHUNKMEGS), NULL, CV_INTEGER);
  // Cvar_SetDescription(cv, "The size of the hunk memory segment");
  if (!mem.hunkMegs) mem.hunkMegs = DEF_COMHUNKMEGS;                   // sk.chg -> cvar replacement
  if (mem.hunkMegs < MIN_COMHUNKMEGS) mem.hunkMegs = MIN_COMHUNKMEGS;  // sk.chg -> cvar replacement

  s_hunkTotal = mem.hunkMegs * 1024 * 1024;
  s_hunkData  = calloc(s_hunkTotal + 63, 1);
  if (!s_hunkData) { err(ERR_EXIT, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024 * 1024)); }

  // cacheline align
  s_hunkData = PADP(s_hunkData, 64);
  Hunk_Clear();

  // Cmd_AddCommand("meminfo", Com_Meminfo_f);
#ifdef MEM_LOG
#  ifdef ZONE_DEBUG
  // Cmd_AddCommand("zonelog", Z_LogHeap);
#  endif  // ZONE_DEBUG
#  ifdef HUNK_DEBUG
  // Cmd_AddCommand("hunklog", Hunk_Log);
  // Cmd_AddCommand("hunksmalllog", Hunk_SmallLog);
#  endif  // HUNK_DEBUG
#endif    // MEM_LOG
}
