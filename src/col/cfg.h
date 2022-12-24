#ifndef COL_CFG_H
#define COL_CFG_H
//..................

//..................
// Collision Configuration values
// Don't change unless you know what you are doing
//..................

//..................
// General
#define MAX_PATHLEN 64  // the maximum size of game relative pathnames
#define MAX_SUBMODELS 256
#define SURFACE_CLIP_EPSILON (0.125)  // keep 1/8 unit away to keep the position valid before network snapping and avoid various numeric issues
#define MAX_POSITION_LEAFS 1024

//..................
// Math
#define MAX_I32 0x7fffffff
#define MIN_QINT (-MAX_I32 - 1)

//..................
// AABB
#define BOUNDS_CLIP_EPSILON 0.25f  // assume single precision and slightly increase to compensate potential SIMD precison loss in 64-bit environment

//..................
// Spheres
#define RADIUS_EPSILON 1.0f

//..................
// Patches
#define MAX_FACETS 1024
#define MAX_PATCH_PLANES (2048 + 128)  // Old engine versions use 2048, and they crash on some q3-defrag maps
#define MAX_GRID_SIZE 129
#define SUBDIVIDE_DISTANCE 16  // 4 // never more than this units away from curve
#define PLANE_TRI_EPSILON 0.1
#define WRAP_POINT_EPSILON 0.1
#define GRID_POINT_EPSILON 0.1
//..................
// Patches: Debug
#define MAX_MAP_BOUNDS 65535
#define MAX_POINTS_ON_WINDING 64
#define NORMAL_EPSILON 0.0001
#define DIST_EPSILON 0.02


//..................
// Angle Indexes
// Note: Quake3 uses +Zup +Yforw for gamecode
//       But id-Tech3 engine uses OpenGL coordinate system in the backend, like seen here
//       These values have not been modified from their defaults (found in qcommon/q_shared.h)
#define PITCH 0  // rotation: up / down
#define YAW 1    // rotation: left / right
#define ROLL 2   // rotation: side rolling
//..............................
// State IDs
// ID numbers for the temporary bsp of the player AABB
#define BOX_MODEL_HANDLE 255      // Box clipModel handle ID number
#define CAPSULE_MODEL_HANDLE 254  // Capsule clipModel handle ID number

//..............................
// BSP Loader
#define VIS_HEADER 8
#define MAX_PATCH_VERTS 1024

//..................
// BSP Limits
// For reference only.
// Used by the engine, but not in this module.
//..................
// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities
#define MAX_MAP_MODELS 0x400
#define MAX_MAP_BRUSHES 0x8000
#define MAX_MAP_ENTITIES 0x800
#define MAX_MAP_ENTSTRING 0x40000
#define MAX_MAP_SHADERS 0x400
#define MAX_MAP_AREAS 0x100  // MAX_MAP_AREA_BYTES in q_shared must match!
#define MAX_MAP_FOGS 0x100
#define MAX_MAP_PLANES 0x20000
#define MAX_MAP_NODES 0x20000
#define MAX_MAP_BSideS 0x20000
#define MAX_MAP_LEAFS 0x20000
#define MAX_MAP_LEAFFACES 0x20000
#define MAX_MAP_LEAFBRUSHES 0x40000
#define MAX_MAP_PORTALS 0x20000
#define MAX_MAP_LIGHTING 0x800000
#define MAX_MAP_LIGHTGRID 0x800000
#define MAX_MAP_VISIBILITY 0x200000
#define MAX_MAP_DRAW_SURFS 0x20000
#define MAX_MAP_DRAW_VERTS 0x80000
#define MAX_MAP_DRAW_INDEXES 0x80000
// key / value pair sizes in the entities lump
#define MAX_KEY 32
#define MAX_VALUE 1024
// the editor uses these predefined yaw angles to orient entities up or down
#define ANGLE_UP -1
#define ANGLE_DOWN -2
#define LIGHTMAP_WIDTH 128
#define LIGHTMAP_HEIGHT 128
#define MAX_WORLD_COORD (128 * 1024)
#define MIN_WORLD_COORD (-128 * 1024)
#define WORLD_SIZE (MAX_WORLD_COORD - MIN_WORLD_COORD)
//..................
#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I')

#endif  // COL_CFG_H
