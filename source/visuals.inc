enum { TRI_SLOTS=65536, NATIVE_PIXELS=416*220 };
#include "gpu_layout.h"
static unsigned serial,triangle_id,requested_scale=1,render_scale=1,smooth_texture,edge_aa,fast_scale;
static uint16_t owners[sizeof(ram)/2];
static U captured_output=1,captured_aa,captured_fast,picture_scale=1;
static unsigned captured_smooth;
static U enhanced[GPU_PIXELS];
static uint16_t screen_owner[GPU_PIXELS];
static B screen_geometry[GPU_PIXELS];
extern int gpu_ready(void);
extern void gpu_invalidate(void);
extern int gpu_draw(unsigned,unsigned,unsigned,unsigned,const U*,const uint16_t*,U*);
void cart_visuals(unsigned scale,unsigned smooth,unsigned aa){
    if(scale<1 || scale>3)return;
    if(requested_scale!=scale || (aa && !edge_aa))
        for(unsigned i=0;i<sizeof(owners)/sizeof(owners[0]);i++)owners[i]=0;
    if(scale!=requested_scale || smooth!=smooth_texture || aa!=edge_aa)gpu_invalidate();
    requested_scale=scale;smooth_texture=smooth;edge_aa=aa;
    fast_scale=scale>1 && !gpu_ready();
}
unsigned cart_picture_scale(void){return picture_scale;}
unsigned cart_fast_scale(void){return fast_scale;}
static U hd_height,hd_aniso,hd_hud_size=100,hd_hud_anchor=1,hd_brightness=100,hd_gamma=100;
static U hd_tone[256],hd_screen_hud;
static HdTriangle hd_triangles[HD_TRIS];
static U hd_palettes[HD_PALETTES][256],hd_palette_id,hd_palette_dirty=1;
void cart_modern(U height,U samples,U hud_size,U anchor,U bright,U gamma){
    if(height && !hd_height){
        for(U i=0;i<HD_TRIS;i++)hd_triangles[i].tag=0;
        hd_palette_dirty=1;
    }
    hd_height=height;hd_aniso=samples;hd_hud_size=hud_size;hd_hud_anchor=anchor;
    hd_brightness=bright;hd_gamma=gamma;
    // The freestanding Windows build has no C math runtime. Evaluate x^(100/gamma)
    // with x87 logarithm and exponent instructions once per LUT entry.
    hd_tone[0]=0;
    for(int i=1;i<256;i++){
        double x=i/255.0, exponent=100.0/gamma, v;
        __asm__("fyl2x; fld %%st(0); frndint; fxch; fsub %%st(1),%%st; f2xm1; fld1; faddp; fscale; fstp %%st(1)"
                : "=t"(v):"0"(x),"u"(exponent):"st(1)");
        int k=(int)(v*255*bright/100+.5);hd_tone[i]=k>255?255:k<0?0:(U)k;
    }
    gpu_invalidate();
}
static int track_geometry(void){return hd_height || requested_scale>1 || edge_aa;}
static void overlay_written(U addr,U bytes){
    if(track_geometry())for(U i=0;i<bytes/2;i++)owners[addr/2+i]=0;
}

enum { SIDE_MAX=(GPU_WIDTH-416)/2, SIDE_STRIDE=2*SIDE_MAX };
typedef struct {
    U target,stamp;
    uint16_t color[220*SIDE_STRIDE],depth[220*SIDE_STRIDE],owner[220*SIDE_STRIDE];
} SideImage;
static SideImage sides[4];
static U side_cursor;
static int side_width,capture_width=416,published_width=416;
unsigned cart_picture_width(void){return published_width;}
static int hd_aspect_w=4,hd_aspect_h=3;
void cart_aspect(int w,int h){
    if(w<=0 || h<=0)return;
    hd_aspect_w=w;hd_aspect_h=h;
    if(3LL*w<4LL*h){hd_aspect_w=4;hd_aspect_h=3;}
    if(3LL*w>8LL*h){hd_aspect_w=8;hd_aspect_h=3;}
    int extra=(int)(((long long)w*312+h/2)/h)-416;
    int side=extra<=0?0:(extra+1)/2;
    if(side>SIDE_MAX)side=SIDE_MAX;
    if(side!=side_width){
        side_width=side;
        for(unsigned i=0;i<4;i++)sides[i].stamp=0;
        gpu_invalidate();
    }
}
static SideImage *side_find(U target){
    for(int i=0;i<4;i++)if(sides[i].target==target && sides[i].stamp)return &sides[i];
    return 0;
}
static SideImage *side_begin(U target,U stamp){
    SideImage *s=side_find(target);
    if(!s)s=&sides[side_cursor++%4];
    if(s->target!=target || s->stamp!=stamp){
        s->target=target;s->stamp=stamp;
        for(int y=0;y<220;y++)for(int x=0;x<side_width*2;x++){
            int i=y*SIDE_STRIDE+x;
            s->color[i]=s->owner[i]=0;s->depth[i]=65535;
        }
    }
    return s;
}
