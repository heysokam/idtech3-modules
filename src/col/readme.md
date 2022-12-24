The math and output of this module is kept as intact as it has been humanly possible.
If there is any difference in how the code/math behaves, compared to the original code, they have have been caused by human error during extraction. Please report a the bug if you find any.

# Differences from Q3
Extraction from id-Tech3 engine required changing the following dependencies:
- id3 Filesystem: Translated to only depend on C's stdlib
- id3 Error management: Translated to only depend on C's stdlib
- Com_memset -> Std_memset : memset without vm/platform implementations and checks
- Com_memcpy -> Std_memcpy : memcpy without vm/platform implementations and checks
- Endianess management has been removed
- BSPC code and filters have been removed
- `Com_Printf("msg\n")` replaced with `echo("msg")`. Simpler and adds newline char

The code has been reorganized (in some spots also renamed/reworded) for readability, and it's slightly more commented out than the original. But documenting such a complex mathematical system is very difficult. 
If you have suggestions/feedback on how to better explain what the code is doing, please reach out.

id-Tech3 engine's file organization, as is the case with its predecesors, is very chaotic and not representative of what each individual piece of code is doing in essence.
Because of this, the folder/files structure has been reorganized a lot, to better represent what each part of the module is meant to do, in a more concrete and specific way.

As explained above, the goal of these changes has _exclusively_ being readability and engine extraction, and not behavioral modification. If you find any changes in the results of this module's output, when compared to the original code, please fill a bug report.
