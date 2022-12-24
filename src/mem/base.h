#ifndef MEM_BASE_H
#define MEM_BASE_H
//....................

// Std dependencies
#include <string.h>
// Engine dependencies
#include "../types.h"

//....................
// Base functionality for the mem modules
//....................

//....................
#define Std_memset memset
#define Std_memcpy memcpy
//....................
#define PAD(base, alignment) (((base) + (alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment) (PAD((base), (alignment)) - (base))
#define PADP(base, alignment) ((void*)PAD((intptr_t)(base), (alignment)))
// stringify macro
#define XSTRING(x) STRING(x)
#define STRING(x) #x

//....................
#endif  // MEM_BASE_H
