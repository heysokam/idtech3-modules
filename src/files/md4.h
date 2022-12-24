#ifndef FS_MD4_H
#define FS_MD4_H
//............................

#include "../types.h"
#include "../mem/core.h"

//............................
struct mdfour {
  u32 A, B, C, D;
  u32 totalN;
};
//............................
unsigned Com_BlockChecksum(const void* buffer, i32 length);
//............................


//............................
#endif  // FS_MD4_H
