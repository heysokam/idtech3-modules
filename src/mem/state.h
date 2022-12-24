#ifndef MEM_STATE_H
#define MEM_STATE_H
//............................

// Engine dependencies
#include "../tools/core.h"
// Module dependencies
#include "./cfg.h"
#include "./types.h"
#include "./base.h"

//............................
// Cvar replacement
extern MemCfg mem;
//............................
void Mem_InitCfg(void);

//............................
extern i32 minfragment;
//............................
// Zone State  :  Initialized from zone.h
extern memzone_t* mainzone;   // main zone for all "dynamic" memory allocation
extern memzone_t* smallzone;  // For small allocations that would only fragment the main zone
//............................
// Hunk State  :  Initialized from hunk.h
extern byte*        s_hunkData;
extern i32          s_hunkTotal;
extern hunkblock_t* hunkblocks;
extern hunkUsed_t   hunk_low;
extern hunkUsed_t   hunk_high;
extern hunkUsed_t*  hunk_permanent;
extern hunkUsed_t*  hunk_temp;
//..............................
extern i32 fs_loadStack;  // total files in memory
//..............................
i32 GetLoadStack(void);

//..............................
void Com_Meminfo(const char* arg1);


//............................
#endif  // MEM_STATE_H
