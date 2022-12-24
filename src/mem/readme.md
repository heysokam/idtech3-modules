# Building
- The external interface lives in core.h. 
  The rest of the headers are just for internal use by the module
  To use the allocator, import core.h and add the `./c/` folder files to your buildsystem.
  It doesn't need any file filtering. You can glob the whole folder all at once.
- The stdlib functions used should be portable. Please send a bug report if they are not.
- Requires C99 minimum 
  Single line comments with `//`
  Arbitrary positioning of variable declarations inside a block
  for-loop sentry variable declarations

# Notes
- The code has being extracted from the engine roughly as it is, without cleaning, readability changes or extra documentation. Original comments are kept, but they are very sparse.
- Cvars replaced with `MemCfg mem;` stored in `state.c`
- `Com_Printf("msg\n")` replaced with `echo("msg")`. Auto adds a newline character
- Com_DPrintf replaced with `if (mem.developer) echo("thing")`
- Com_Error replaced with `err(CODE, "msg", ...)`
- Com_sprintf replaced with sdtlib's `sprintf`
- BSPC code and filters have been removed
- Endianess management has been removed
- Cmd_AddCommand has been disabled (commented out). It's not needed for standalone behavior
- `Com_Init[memtype]Memory` initializer functions renamed to `Mem_Init[memtype]`
- `Com_Meminfo` renamed to `Mem_Info`. Change to fit the echo function, which auto adds a newline

# Logging
All memory logging has been fully disabled.
Extracting the logging code requires access to the full engine filesystem code, which is out of scope, so it has not been done. Instead, every reference to its code has been locked behind a `#ifdef MEM_LOG` compile time flag
The logging functions have been marked with the keyword `BROKEN` in their description, for clarity and easier searching.

id-Tech3 filesystem works with handle ids, which are then used to seek referenced files inside PK3 files and/or the folder structure, based on predetermined conditions.
The code is kept for reference, and will need extra effort to be able to use the functionality.
The best way to do this would be to replace `FS_` functions with their stdlib relatives, and refactor the logging code to fit with their format.
