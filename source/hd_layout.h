#ifndef TOWER_HD_LAYOUT
#define TOWER_HD_LAYOUT
#include <stdint.h>
enum { HD_TRIS=65536, HD_PALETTES=4096, HD_MAX_PIXELS=5760*2160 };
typedef struct {
    float x,y,z,q,u,v;
    uint32_t shade,pad;
} HdVertex;
typedef struct {
    HdVertex vertex[3];
    uint32_t tag,format,source,stride;
    int32_t origin_s,origin_t,width,height;
    uint32_t axes,palette,prim,env,key_scale,key_center,lod,cycle;
    uint32_t equations[4],clip_min,clip_max,mode,palette_bank;
} HdTriangle;
_Static_assert(sizeof(HdTriangle)==192,"Shader triangle layout");
#endif
