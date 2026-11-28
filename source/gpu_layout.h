#include <stdint.h>
#include "hd_layout.h"
enum { GPU_WIDTH=832, GPU_HEIGHT=220, GPU_PIXELS=GPU_WIDTH*GPU_HEIGHT, GPU_TRIS=65536 };
typedef struct {
    uint32_t native[GPU_PIXELS],owner[GPU_PIXELS/2];
    uint32_t scene[HD_MAX_PIXELS],output[HD_MAX_PIXELS];
    HdTriangle triangles[HD_TRIS];
    uint32_t palette[HD_PALETTES*256],memory[0x800000/4],tone[256],valid[GPU_PIXELS];
} GpuStorage;
