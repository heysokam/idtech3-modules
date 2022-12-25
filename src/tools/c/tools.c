#include "../types/base.h"


//..............................
// id-Tech3 tool functions
// Dropped all of their engine dependencies
// echo, as an example, pulled +60% of the engine just by itself because of the dependencies
// reaching into the server, the client, the cvars, the filesystem, the console, and many many more
//..............................

//..............................
// msec
//   Taken from botlib, because its a simpler solution.
//   The engine itself does not calc time this way.
//   This version avoids storing state for keeping track of time.
//   This depends on <time.h>. The original code used different code for windows/unix
//..............................
i32 msec(void) { return clock() * 1000 / CLOCKS_PER_SEC; }

//...............................
// va :
// varargs printf into a temp buffer, so varargs versions of all text functions are not needed
char* f(const char* format, ...) {
  static char string[2][32000];  // in case va is called by nested functions
  static int  index = 0;
  char*       buf   = string[index];
  index ^= 1;
  va_list args;
  va_start(args, format);
  vsprintf(buf, format, args);
  va_end(args);
  return buf;
}

//..............................
// echo
//   Prints a newline message to stdout
//   Replaces both  echo  and  Com_DPrintf
//   Mod version just outputs to stdout instead
//..............................
void echo(const char* fmt, ...) {
  va_list argptr;
  char    msg[MAX_PRINTMSG];
  va_start(argptr, fmt);
  vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);
  fprintf(stdout, "%s\n", msg);
}

//..............................
// Com_Error
//   Mod version just outputs to stderr.
//..............................
void err(ErrorType type, const char* msg, ...) {
  va_list argptr;
  char    text[1024];
  va_start(argptr, msg);
  vsnprintf(text, sizeof(text), msg, argptr);
  va_end(argptr);

  // Handle each error type
  switch (type) {
    case ERR_EXIT: fprintf(stderr, "%s\n", text); exit(-1);
    default: fprintf(stderr, "Unknown error:\t %s\n", text); exit(-1);
  }
}

//..............................
// Q_strncpyz
//   Safe strncpy that ensures a trailing zero
//..............................
void strncpyz(char* dest, const char* src, int destsize) {
  if (!dest) { err(ERR_EXIT, "Q_strncpyz: NULL dest"); }
  if (!src) { err(ERR_EXIT, "Q_strncpyz: NULL src"); }
  if (destsize < 1) { err(ERR_EXIT, "Q_strncpyz: destsize < 1"); }
#if 1
  // do not fill whole remaining buffer with zeros
  // this is obvious behavior change but actually it may affect only buggy QVMs
  // which passes overlapping or short buffers to cvar reading routines
  // what is rather good than bad because it will no longer cause overwrites, maybe
  while (--destsize > 0 && (*dest++ = *src++) != '\0')
    ;
  *dest = '\0';
#else
  strncpy(dest, src, destsize - 1);
  dest[destsize - 1] = '\0';
#endif
}

//..............................
// Q_stricmp :
//   Insensitive compare strings s1 and s2.
//   TODO: Is this different than stdlib's   stricmp  ?
//..............................
i32 Q_stricmp(const char* s1, const char* s2) {
  if (s1 == NULL) {
    if (s2 == NULL) return 0;
    else return -1;
  } else if (s2 == NULL) return 1;
  byte c1, c2;
  do {
    c1 = *s1++;
    c2 = *s2++;
    if (c1 != c2) {
      if (c1 <= 'Z' && c1 >= 'A') c1 += ('a' - 'A');
      if (c2 <= 'Z' && c2 >= 'A') c2 += ('a' - 'A');
      if (c1 != c2) return c1 < c2 ? -1 : 1;
    }
  } while (c1 != '\0');
  return 0;
}
