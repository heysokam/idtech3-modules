#ifndef TOOLS_H
#define TOOLS_H
//..............................

// stdlib dependencies
#include <stdarg.h>  // For varargs
#include <stdio.h>   // For fprintf, vsnprintf
#include <stdlib.h>  // For exit()
#include <time.h>    // For clock() and CLOCKS_PER_SEC
// Engine modules
#include "../types/core.h"

//..............................
// cfg.h
#define MAX_PRINTMSG 8192

//..............................
// Error types
typedef enum {
  ERR_EXIT,  // Exit the entire game, and prints the message to stderr
  ERR_DROP,  // print to console and disconnect from game
  // ERR_SVDISCONNECT,  // don't kill server
  // ERR_CLDISCONNECT,  // client disconnected from the server
} ErrorType;
//..............................
char* f(const char* format, ...);  // Used to be va()
void  err(ErrorType type, const char* msg, ...);
void  echo(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
int   msec(void);
//...............................
void strncpyz(char* dest, const char* src, int destsize);
i32  Q_stricmp(const char* s1, const char* s2);

//..............................
#endif  // TOOLS_H
