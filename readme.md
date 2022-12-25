# id-Tech3 Modules
Modular elements of id-Tech3 Engine (Quake III Arena), without any dependencies other than stdlib.  

Each individual module contains as few dependencies as possible.  
This is so they can be integrated into a different project, or wrapped for usage in a different language.  
The modules depend on the custom-named types file for readability, but that can easily be searched-replaced to revert them to std-only types if desired.  

Done:
- [x] Manual Memory Management System (Zone+Hunk allocation)
- [x] BSP loading (full spec, including patches)
- [x] Collision resolution system
- [x] Simplified bbox-to-bsp solving code, with no dependencies and no loading

TODO:
- [ ] Disconnect the collision system from memory allocation. Should take a pointer to the data as input instead.
  - [x] bbox-to-bsp only
  - [ ] full bsp spec
- [ ] Disconnect BSP loading from memory allocation. Should return the data instead.
- [ ] Script to generate Single Header modules from the code, in stb_ style.
