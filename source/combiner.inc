/* Decode SETCOMBINE once. Each N64 colour or alpha cycle computes (A-B)*C+D. */
enum { CC_PREV,CC_TEX,CC_TEX1,CC_PRIM,CC_SHADE,CC_ENV,CC_ONE,CC_ZERO,
       CC_PREV_A,CC_TEX_A,CC_TEX1_A,CC_PRIM_A,CC_SHADE_A,CC_ENV_A,CC_LOD,CC_KEY_SCALE,CC_KEY_CENTER };
typedef struct {B a,b,c,d;} Equation;
typedef struct {Equation rgb,alpha;} ColorCycle;
static ColorCycle cycles[2];
static U combine_low,lod_fraction,key_scale,key_center;
static int first_cycle_pair;
static unsigned tile_inputs(unsigned input){
    if(input==CC_TEX || input==CC_TEX_A)return 1;
    if(input==CC_TEX1 || input==CC_TEX1_A)return 2;
    return 0;
}
static unsigned equation_tiles(Equation e){
    unsigned mask=tile_inputs(e.d);
    if(e.c!=CC_ZERO)mask|=tile_inputs(e.a)|tile_inputs(e.b)|tile_inputs(e.c);
    return mask;
}
static int uses_tile_pair(unsigned tile){
    const Tile *second=&tiles[(tile+1)&7];
    return cycle_type==1 && first_cycle_pair && second->w>0 && second->h>0;
}
static const B rgb_c[32]={0,1,2,3,4,5,CC_KEY_SCALE,8,9,10,11,12,13,7,14,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
static Equation color_equation(U a,U b,U c,U d){
    Equation e={a<=6?a:CC_ZERO,b<6?b:b==6?CC_KEY_CENTER:CC_ZERO,rgb_c[c],d};
    if(e.a==e.b || e.c==CC_ZERO)e.c=CC_ZERO;
    return e;
}
static Equation alpha_equation(U a,U b,U c,U d){
    Equation e={a,b,c==0?CC_ZERO:c==6?CC_LOD:c,d};
    if(a==b || c==7)e.c=CC_ZERO;
    return e;
}
static void set_combine(U a,U b){
    combine=a;combine_low=b;
    cycles[0].rgb=color_equation(a>>20&15,b>>28,a>>15&31,b>>15&7);
    cycles[0].alpha=alpha_equation(a>>12&7,b>>12&7,a>>9&7,b>>9&7);
    cycles[1].rgb=color_equation(a>>5&15,b>>24&15,a&31,b>>6&7);
    cycles[1].alpha=alpha_equation(b>>21&7,b>>3&7,b>>18&7,b&7);
    /* Handle explicit two-tile materials whose second cycle needs no new sample. */
    first_cycle_pair=(equation_tiles(cycles[0].rgb)|equation_tiles(cycles[0].alpha))==3 &&
        !equation_tiles(cycles[1].rgb) && !equation_tiles(cycles[1].alpha);
}
static U mul_channel(U x,U y){return (x*y+127)/255;}
static U modulate(U a,U b){
    typedef unsigned char Bytes __attribute__((vector_size(4)));
    typedef unsigned short Words __attribute__((vector_size(8)));
    Words x=__builtin_convertvector((Bytes)a,Words);
    Words y=__builtin_convertvector((Bytes)b,Words);
    Words product=x*y+128;
    return (U)__builtin_convertvector((product+(product>>8))>>8,Bytes);
}
static U equation_channel(int a,int b,int c,int d){
    int v=((a-b)*c+(a>=b?127:-127))/255+d;
    return v<0?0:v>255?255:(U)v;
}
static U combined_pixel(U tex,U tex1,U shade){
    if(cycle_type==2)return tex;
    /* Hot paths used to copy a framebuffer and to shade world textures. */
    if(combine==0xfcffffff && combine_low==0xfffcf279)return tex;
    if(cycle_type==1){
        if(combine==0xfcffffff && combine_low==0xfffcf238)return tex;
        if(combine==0xfcffffff && combine_low==0xff0cf23f)
            return (tex&~255u)|mul_channel(tex&255,prim&255);
        if((combine==0xfc137fff && combine_low==0xfffff238) ||
           (combine==0xfcfffe06 && combine_low==0xfffcf3f8))
            return key_scale==0xffffff00u?tex:(modulate(tex,key_scale)&~255u)|(tex&255);
        if(combine==0xfcff9bff && combine_low==0xfffdfe38)
            return (prim&~255u)|mul_channel(tex&255,env&255);
        if(combine==0xfc26a004 && combine_low==0x1ffc93f8)
            return (modulate(tex,shade)&~255u)|(tex&255);
        if(combine==0xfc30ffff && combine_low==0xfffffc38)return modulate(prim,tex)|255u;
        if(key_scale==0xffffff00u && combine==0xfc127e06 && combine_low==0xfffff3f8)return (modulate(tex,shade)&~255u)|(tex&255);
        if(combine==0xfcfffe04 && combine_low==0xfffcf3f8)return (modulate(tex,shade)&~255u)|(tex&255);
    }
    U val[17]={0,tex,tex1,prim,shade,env,~0u,0,0,(tex&255)*0x01010101u,
               (tex1&255)*0x01010101u,(prim&255)*0x01010101u,(shade&255)*0x01010101u,
               (env&255)*0x01010101u,lod_fraction*0x01010101u,key_scale,key_center};
    for(int i=(cycle_type==1?0:1);i<2;i++){
        Equation e=cycles[i].rgb,f=cycles[i].alpha;
        U result=val[e.d]&~255u;
        if(e.c!=CC_ZERO){
            result=0;
            for(int j=8;j<=24;j+=8)result|=equation_channel(val[e.a]>>j&255,val[e.b]>>j&255,val[e.c]>>j&255,val[e.d]>>j&255)<<j;
        }
        U alpha=val[f.d]&255;
        if(f.c!=CC_ZERO)alpha=equation_channel(val[f.a]&255,val[f.b]&255,val[f.c]&255,alpha);
        val[0]=result|alpha;val[CC_PREV_A]=alpha*0x01010101u;
    }
    return val[0];
}
