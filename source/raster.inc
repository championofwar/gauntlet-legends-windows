typedef struct {U fmt,size,line,bank,pal,ms,mt; int s,t,w,h,mode_s,mode_t,shift_s,shift_t;} Tile;
static Tile tiles[8];
static U triangle_tile,rect_tile;
static U source,src_width,src_size,tex_base[512],palette[256],prim=~0u,texture_on,zbuffer;
static int rect[4],rs,rt;
static U combine,env,geom,light_address,ambient_address,render_mode;
static U load_stride[512];
static unsigned halfword(U p){return p<sizeof(ram)-1?(ram[p]<<8)|ram[p+1]:0;}
static int signed12(int v){return (v&2048)?v-4096:v;}
static U rgba_table[65536];
static U expand_rgba16(U v){return ((v>>11)*255/31<<24)|(((v>>6)&31)*255/31<<16)|(((v>>1)&31)*255/31<<8)|((v&1)*255);}
static U rgba16(U v){return rgba_table[v&65535];}
static U color16(U c){return ((c>>16)&0xf800)|((c>>13)&0x7c0)|((c>>10)&62)|1;}
static int tile_axis(int coord,int origin,int extent,int mask,int mode,int shift){
    if(shift<=10)coord>>=shift;else coord*=1<<(16-shift);
    coord-=origin;
    if((mode&2) || !mask){
        if(coord<0)coord=0;
        if(coord>=extent)coord=extent>0?extent-1:0;
    }
    if(mask){
        int period=1<<mask;
        if((mode&1) && (coord&period))coord=~coord;
        coord&=period-1;
    }
    return coord;
}
static U texel(U tile,int u,int v){
    Tile *t=&tiles[tile];
    u=tile_axis(u,t->s,t->w,t->ms,t->mode_s,t->shift_s);
    v=tile_axis(v,t->t,t->h,t->mt,t->mode_t,t->shift_t);
    U stride=load_stride[t->bank]?load_stride[t->bank]:t->line*(t->size==3?2:1);
    U p=tex_base[t->bank]+v*stride+(((U)u<<t->size)>>1);
    if(p>sizeof(ram)-4)return 0;
    U c=ram[p], intensity=c,alpha=255;
    if(t->size==0)c=(c>>((u&1)?0:4))&15;
    if(t->fmt==2)return palette[t->size?c:c+t->pal*16];
    if(t->fmt==0)return t->size==3?wordat(p):rgba16(halfword(p));
    if(t->fmt==3){
        if(t->size==0){intensity=(c>>1)*255/7;alpha=(c&1)*255;}
        else if(t->size==1){intensity=(c>>4)*17;alpha=(c&15)*17;}
        else alpha=ram[p+1];
    }else if(t->fmt==4){intensity=t->size?c:c*17;alpha=intensity;}
    return intensity*0x1010100u+alpha;
}

static U blend_rgb(U a,U b,U weight){
    U result=0;
    for(int shift=8;shift<=24;shift+=8){
        U x=(a>>shift)&255,y=(b>>shift)&255;
        result|=((x*weight+y*(255-weight)+127)/255)<<shift;
    }
    return result;
}
static void composite(int x,int y,U c){
    U alpha=c&255;
    if(!alpha)return;
    if(alpha<255){
        U p=buffer+2*((U)y*width+x);
        c=blend_rgb(c,rgba16(halfword(p)),alpha);
    }
    pixel(x,y,color16(c));
}
#include "combiner.inc"
static void textured_rect(U delta){
    int pair=uses_tile_pair(rect_tile);
    float dx=(int16_t)(delta>>16)/1024.f,dy=(int16_t)delta/1024.f;
    for(int y=rect[1];y<rect[3]&&y<480;y++)for(int x=rect[0];x<rect[2]&&(U)x<width;x++){
        float u=rs/32.f+(x-rect[0])*dx,v=rt/32.f+(y-rect[1])*dy;
        U c=texel(rect_tile,(int)u,(int)v);
        U second=pair?texel((rect_tile+1)&7,(int)u,(int)v):c;
        c=combined_pixel(c,second,~0u);
        composite(x,y,c);
    }
}
typedef struct {float m[4][4];} Matrix;
static Matrix mv[32],projection;static U level;
static float vx=208,vy=110,vsx=208,vsy=110;
typedef struct {float x,y,z,q,s,t,clip[4];U shade;} Vertex;
static Vertex vertices[32];
static void matrix(U p,U flags){
    if(p>sizeof(ram)-64)return;
    Matrix n,old,out;
    for(int i=0;i<16;i++)n.m[i/4][i%4]=(int16_t)halfword(p+i*2)+halfword(p+32+i*2)/65536.f;
    if(!(flags&4)&&!(flags&1)&&level<31){mv[level+1]=mv[level];level++;}
    Matrix *m=(flags&4)?&projection:&mv[level];
    if(flags&2){*m=n;return;}
    old=*m;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){float f=0;for(int k=0;k<4;k++)f+=n.m[i][k]*old.m[k][j];out.m[i][j]=f;}
    *m=out;
}
static void transform(float *dst,const float *src,const Matrix *m){
    for(int j=0;j<4;j++){dst[j]=0;for(int k=0;k<4;k++)dst[j]+=src[k]*m->m[k][j];}
}
static U vertex_color(U p){
    if(!(geom&0x20000) || !ambient_address)return wordat(p);
    float n[3],len=0;
    for(int i=0;i<3;i++){
        n[i]=0;
        for(int j=0;j<3;j++)n[i]+=(int8_t)ram[p+j]*mv[level].m[j][i];
        len+=n[i]*n[i];
    }
    float diffuse=0;
    if(len>0 && light_address){
        for(int i=0;i<3;i++)diffuse+=n[i]*(int8_t)ram[light_address+8+i];
        diffuse/=127.f*__builtin_sqrtf(len);
        if(diffuse<0)diffuse=0;
    }
    U c=ram[p+3];
    for(int i=0;i<3;i++){U v=ram[ambient_address+i]+(U)(diffuse*ram[light_address+i]);c|=(v>255?255:v)<<(24-8*i);}
    return c;
}
static void vertex_load(U a,U p){
    int n=(a>>12)&255,end=(a&255)>>1;
    if(n>end||end>32||p>sizeof(ram)-n*16)return;
    for(int i=end-n;i<end;i++,p+=16){
        float xyz[4]={(int16_t)halfword(p),(int16_t)halfword(p+2),(int16_t)halfword(p+4),1},eye[4],clip[4];
        transform(eye,xyz,&mv[level]);transform(clip,eye,&projection);
        Vertex *v=&vertices[i];for(int j=0;j<4;j++)v->clip[j]=clip[j];v->q=clip[3]>0.001f?1.f/clip[3]:0;
        v->x=vx+clip[0]*v->q*vsx;v->y=vy-clip[1]*v->q*vsy;v->z=clip[2]*v->q;
        v->s=(int16_t)halfword(p+8)/32.f;v->t=(int16_t)halfword(p+10)/32.f;v->shade=vertex_color(p+12);
    }
}
typedef float Channels __attribute__((vector_size(16)));
typedef int ChannelInts __attribute__((vector_size(16)));
static Channels shade_channels(U c){
    return (Channels){c>>24,c>>16&255,c>>8&255,c&255};
}
typedef struct {
    Vertex a,b,c;
    SideImage *side;
    int x0,x1,y0,y1;
    float area;
    U owner;
} RasterRows;
typedef unsigned short PixelWords __attribute__((vector_size(8)));
static PixelWords load_pixel_words(const void *p){
    PixelWords v;__builtin_memcpy(&v,p,sizeof(v));return v;
}
static PixelWords reverse_pixel_bytes(PixelWords v){return (v>>8)|(v<<8);}
static void store_masked_words(void *dst,PixelWords v,PixelWords mask){
    v=(v&mask)|(load_pixel_words(dst)&~mask);
    __builtin_memcpy(dst,&v,sizeof(v));
}
static ChannelInts shade_four(ChannelInts component,Channels l,Channels m,Channels n,
                              U first,U second,U third,int flat){
    ChannelInts shade;
    if(flat)shade=(ChannelInts){first,first,first,first};
    else shade=__builtin_convertvector(l*(float)first+m*(float)second+n*(float)third+0.5f,ChannelInts);
    ChannelInts product=shade*component+128;
    return (product+(product>>8))>>8;
}
static int independent_rows(void);
static void raster_rows(const RasterRows *job,int first,int end,int row_step){
    Vertex a=job->a,b=job->b,c=job->c;
    SideImage *side=job->side;
    int x0=job->x0,x1=job->x1,y0=first,y1=end;
    float area=job->area,ox=0.5f,oy=0.5f;
    B *dst=ram;
    const int flat=a.shade==b.shade && a.shade==c.shade;
    Channels ca=shade_channels(a.shade),cb=shade_channels(b.shade),cc=shade_channels(c.shade);
    const int pair=texture_on && uses_tile_pair(triangle_tile);
    const int keyed=cycle_type==1 && combine==0xfc127e06 && combine_low==0xfffff3f8;
    const int shaded=cycle_type==1 && ((combine==0xfcfffe04 && combine_low==0xfffcf3f8)
        || (combine==0xfc26a004 && combine_low==0x1ffc93f8));
    const int direct=cycle_type==2 || (combine==0xfcffffff &&
        (combine_low==0xfffcf279 || (cycle_type==1 && combine_low==0xfffcf238)));
    const Tile *tile=&tiles[triangle_tile];
    /* Wrapped, unshifted CI4 tiles dominate the summoned creature scene.
       Compute their four palette lookups' byte addresses together. */
    const int wrapped_ci4=texture_on && tile->fmt==2 && tile->size==0 &&
        tile->ms && tile->mt && !tile->mode_s && !tile->mode_t &&
        !tile->shift_s && !tile->shift_t;
    /* Four disjoint depth samples can reject an entire group before its
       perspective texture divisions. Keep overlapping targets in draw order. */
    uint64_t span=2ull*width*220;
    const int early_depth=(geom&1) && (render_mode&16) && width==416 &&
        zbuffer+span<=sizeof(ram) && (buffer+span<=zbuffer || zbuffer+span<=buffer);
    const int four_pixels=wrapped_ci4 && (direct || shaded || (keyed && key_scale==0xffffff00u))
        && independent_rows();
    const U *ci_palette=palette+16*tile->pal;
    U tex_start=tex_base[tile->bank];
    int stride=load_stride[tile->bank]?load_stride[tile->bank]:tile->line;
    int mask_s=(1<<tile->ms)-1,mask_t=(1<<tile->mt)-1;
    /* Evaluate adjacent pixel centres with the same arithmetic and rounding as
       the scalar rasterizer. Shared edges and guest depth values stay exact. */
    for(int y=y0;y<y1;y+=row_step)for(int block=x0;block<x1;block+=4){
        Channels dx=(Channels){block,block+1,block+2,block+3}+ox-a.x;
        float dy=y+oy-a.y;
        Channels ms=(dx*(c.y-a.y)-dy*(c.x-a.x))/area;
        Channels ns=((b.x-a.x)*dy-(b.y-a.y)*dx)/area;
        Channels ls=1.f-ms-ns;
        ChannelInts covered=(ls>=0.f)&(ms>=0.f)&(ns>=0.f);
        if(!__builtin_ia32_movmskps((Channels)covered))continue;
        Channels zs=(ls*a.z+ms*b.z+ns*c.z+1.f)*32760.f;
        int tested_depth=early_depth && block>=0 && block+4<=x1 && block+4<=416;
        if(tested_depth){
            U offset=zbuffer+2*((U)y*width+block);
            PixelWords depths=reverse_pixel_bytes(load_pixel_words(dst+offset));
            covered&=(zs>=0.f)&(zs<=__builtin_convertvector(depths,Channels));
            if(!__builtin_ia32_movmskps((Channels)covered))continue;
        }
        Channels us={0},ts={0};
        if(texture_on){
            Channels qs=ls*a.q+ms*b.q+ns*c.q;
            us=(ls*a.s*a.q+ms*b.s*b.q+ns*c.s*c.q)/qs;
            ts=(ls*a.t*a.q+ms*b.t*b.q+ns*c.t*c.q)/qs;
        }
        ChannelInts ci_addr={0},ci_shift={0};
        if(wrapped_ci4){
            ChannelInts u=(__builtin_convertvector(us,ChannelInts)-tile->s)&mask_s;
            ChannelInts v=(__builtin_convertvector(ts,ChannelInts)-tile->t)&mask_t;
            ci_addr=(int)tex_start+v*stride+(u>>1);
            ci_shift=((u&1)^1)<<2;
        }
        /* CI4 palette entries are either transparent or fully opaque. Write
           four native samples at once when colour, depth and texture storage
           are disjoint. Side strips and final partial groups use scalar stores. */
        if(four_pixels && block>=0 && block+4<=x1 && block+4<=416){
            U zp=zbuffer+2*((U)y*width+block);
            covered&=(zs>=0.f)&(zs<=65535.f);
            if(!tested_depth && (geom&1) && (render_mode&16)){
                PixelWords depths=reverse_pixel_bytes(load_pixel_words(dst+zp));
                covered&=zs<=__builtin_convertvector(depths,Channels);
            }
            if(!__builtin_ia32_movmskps((Channels)covered))continue;
            ChannelInts colours;
            for(int k=0;k<4;k++){
                U pos=(U)ci_addr[k];
                colours[k]=pos<=sizeof(ram)-4?ci_palette[(ram[pos]>>ci_shift[k])&15]:0;
            }
            covered&=(colours&255)!=0;
            if(!__builtin_ia32_movmskps((Channels)covered))continue;
            ChannelInts red=(colours>>24)&255,green=(colours>>16)&255,blue=(colours>>8)&255;
            if(!direct){
                red=shade_four(red,ls,ms,ns,a.shade>>24,b.shade>>24,c.shade>>24,flat);
                green=shade_four(green,ls,ms,ns,(a.shade>>16)&255,(b.shade>>16)&255,(c.shade>>16)&255,flat);
                blue=shade_four(blue,ls,ms,ns,(a.shade>>8)&255,(b.shade>>8)&255,(c.shade>>8)&255,flat);
            }
            ChannelInts packed=((red&248)<<8)|((green&248)<<3)|((blue&248)>>2)|1;
            PixelWords mask=__builtin_convertvector(covered,PixelWords);
            if((geom&1) && (render_mode&32)){
                Channels valid_z=(Channels)((ChannelInts)zs&covered);
                ChannelInts depth=__builtin_convertvector(valid_z,ChannelInts);
                store_masked_words(dst+zp,reverse_pixel_bytes(__builtin_convertvector(depth,PixelWords)),mask);
            }
            U addr=buffer+2*((U)y*width+block);
            store_masked_words(dst+addr,reverse_pixel_bytes(__builtin_convertvector(packed,PixelWords)),mask);
            if(track_geometry()){
                uint16_t id=job->owner;
                store_masked_words(owners+addr/2,(PixelWords){id,id,id,id},mask);
            }
            continue;
        }
        for(int lane=0;lane<4 && block+lane<x1;lane++){
            if(!covered[lane])continue;
            int x=block+lane;
            float l=ls[lane],m=ms[lane],n=ns[lane],z=zs[lane];
            int outside=x<0 || x>=(int)width;
            int si=y*SIDE_STRIDE+(x<0?x+side_width:x-416+side_width);
            U zp=zbuffer+2*((U)y*width+x);
            if(z<0 || z>65535 || (!outside && zp>sizeof(ram)-2))continue;
            if(outside && !side)continue;
            if(!tested_depth && (render_mode&16)&&(geom&1) && z>(outside?side->depth[si]:dst[zp]*256u+dst[zp+1]))continue;
            U shade=a.shade;
            if(!flat){
                Channels value=ca*l+cb*m+cc*n+0.5f;
                ChannelInts parts=__builtin_convertvector(value,ChannelInts);
                shade=((U)parts[0]<<24)|((U)parts[1]<<16)|((U)parts[2]<<8)|(U)parts[3];
            }
            U col=shade;
            if(wrapped_ci4){
                U pos=(U)ci_addr[lane];
                col=pos<=sizeof(ram)-4?ci_palette[(ram[pos]>>ci_shift[lane])&15]:0;
            }else if(texture_on)col=texel(triangle_tile,(int)us[lane],(int)ts[lane]);
            if(keyed || shaded){
                U original_alpha=col&255;
                col=modulate(col,shade);
                if(keyed && key_scale!=0xffffff00u)col=modulate(col,key_scale);
                col=(col&~255u)|original_alpha;
            }else if(!direct){
                U second=pair?texel((triangle_tile+1)&7,(int)us[lane],(int)ts[lane]):col;
                col=combined_pixel(col,second,shade);
            }
            if(!(col&255))continue;
            if(outside){
                if((geom&1)&&(render_mode&32))side->depth[si]=(U)z;
                U alpha=col&255;
                if(alpha<255)col=blend_rgb(col,rgba16(side->color[si]),alpha);
                side->color[si]=color16(col);
                side->owner[si]=alpha==255?job->owner:0;
                continue;
            }
            if((geom&1)&&(render_mode&32)){dst[zp]=(U)z>>8;dst[zp+1]=(U)z;}
            U addr=buffer+2*((U)y*width+x);
            if(addr>sizeof(ram)-2)continue;
            U alpha=col&255;
            if(alpha<255)col=blend_rgb(col,rgba16(dst[addr]*256u+dst[addr+1]),alpha);
            U packed=color16(col);
            dst[addr]=packed>>8;dst[addr+1]=packed;
            if(track_geometry())owners[addr/2]=alpha==255?job->owner:0;
        }
    }
}
#include "raster_worker.inc"
static void raster_sample(Vertex a,Vertex b,Vertex c){
    if(!a.q||!b.q||!c.q)return;
    float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    if(__builtin_fabsf(area)<0.01f)return;
    int expand=side_width && width==416 && buffer!=zbuffer && vsx>=195 && vsx<=211 &&
        (a.clip[3]!=1.f || b.clip[3]!=1.f || c.clip[3]!=1.f);
    int left=expand && clip_left<=2 ? -side_width:clip_left;
    int right=expand && clip_right>=414 ? 416+side_width:clip_right;
    if(!expand && right>(int)width)right=width;
    int x0=right,x1=left,y0=220,y1=0;
    Vertex pts[3]={a,b,c};
    for(int i=0;i<3;i++){
        float x=pts[i].x,y=pts[i].y;
        if(x<x0)x0=x<=left?left:(int)x;
        if(x>x1)x1=x>=right?right:(int)x+1;
        if(y<y0)y0=y<0?0:(int)y;
        if(y>y1)y1=y>220?220:(int)y+1;
    }
    SideImage *side=expand && (x0<0 || x1>416)?side_begin(buffer,frames+1):0;
    if(y0<clip_top)y0=clip_top;if(y1>clip_bottom)y1=clip_bottom;
    if(x0>=x1 || y0>=y1)return;
    RasterRows job={a,b,c,side,x0,x1,y0,y1,area,triangle_id};
    if(worker_thread && (batch_count || independent_rows())){
        row_batch[batch_count++]=job;
        batch_pixels+=(x1-x0)*(y1-y0);
        if(batch_count==BATCH_LIMIT)flush_rows();
    }else raster_rows(&job,y0,y1,1);
}
static U pack_equation(Equation e){return e.a|(e.b<<8)|(e.c<<16)|((U)e.d<<24);}
static void hd_capture(Vertex a,Vertex b,Vertex c){
    HdTriangle *h=&hd_triangles[triangle_id%HD_TRIS];
    h->tag=0;
    /* The HD shader stores one tile. Retain the native composite for this material. */
    if(texture_on && uses_tile_pair(triangle_tile))return;
    if(width!=416 || buffer==zbuffer || (a.clip[3]==1 && b.clip[3]==1 && c.clip[3]==1))return;
    Vertex pts[3]={a,b,c};
    for(int i=0;i<3;i++){
        Vertex *v=&pts[i];h->vertex[i]=(HdVertex){v->x,v->y,v->z,v->q,v->s,v->t,v->shade,0};
    }
    Tile *t=&tiles[triangle_tile];
    h->tag=triangle_id;h->format=texture_on?(1u|(t->fmt<<4)|(t->size<<8)):0;
    h->source=tex_base[t->bank];h->stride=load_stride[t->bank]?load_stride[t->bank]:t->line*(t->size==3?2:1);
    h->origin_s=t->s;h->origin_t=t->t;h->width=t->w;h->height=t->h;
    h->axes=t->ms|(t->mt<<4)|(t->mode_s<<8)|(t->mode_t<<10)|(t->shift_s<<12)|(t->shift_t<<16);
    if(texture_on && t->fmt==2 && hd_palette_dirty){
        ++hd_palette_id;__builtin_memcpy(hd_palettes[hd_palette_id%HD_PALETTES],palette,sizeof(palette));hd_palette_dirty=0;
    }
    h->palette=hd_palette_id;h->palette_bank=t->pal;
    h->prim=prim;h->env=env;h->key_scale=key_scale;h->key_center=key_center;h->lod=lod_fraction;
    h->cycle=cycle_type;h->equations[0]=pack_equation(cycles[0].rgb);h->equations[1]=pack_equation(cycles[0].alpha);
    h->equations[2]=pack_equation(cycles[1].rgb);h->equations[3]=pack_equation(cycles[1].alpha);
    h->clip_min=clip_left|((U)clip_top<<16);h->clip_max=clip_right|((U)clip_bottom<<16);
    h->mode=0;
    if(cycle_type==2 || (combine==0xfcffffff && (combine_low==0xfffcf279 || (cycle_type==1 && combine_low==0xfffcf238))))h->mode=1;
    else if(cycle_type==1){
        if(combine==0xfcffffff && combine_low==0xff0cf23f)h->mode=2;
        if((combine==0xfc137fff && combine_low==0xfffff238)||(combine==0xfcfffe06 && combine_low==0xfffcf3f8))h->mode=3;
        if(combine==0xfcff9bff && combine_low==0xfffdfe38)h->mode=4;
        if((combine==0xfc26a004 && combine_low==0x1ffc93f8)||(combine==0xfcfffe04 && combine_low==0xfffcf3f8)||
           (combine==0xfc127e06 && combine_low==0xfffff3f8 && key_scale==0xffffff00u))h->mode=5;
        if(combine==0xfc30ffff && combine_low==0xfffffc38)h->mode=6;
    }
}
static void raster_triangle(Vertex a,Vertex b,Vertex c){
    if(track_geometry()){
        ++serial;if(!(serial&65535u))++serial;
        triangle_id=serial&65535u;
        if(hd_height)hd_capture(a,b,c);

    }
    raster_sample(a,b,c);
}
static Vertex near_intersection(Vertex a,Vertex b,float da,float db){
    float t=da/(da-db);
    Vertex v=a;
    for(int j=0;j<4;j++)v.clip[j]+=t*(b.clip[j]-a.clip[j]);
    v.q=v.clip[3]>0.00001f?1.f/v.clip[3]:0;
    v.x=vx+v.clip[0]*v.q*vsx;v.y=vy-v.clip[1]*v.q*vsy;v.z=-1;
    v.s+=t*(b.s-a.s);v.t+=t*(b.t-a.t);v.shade=0;
    for(int j=0;j<32;j+=8){
        int first=(a.shade>>j)&255,last=(b.shade>>j)&255;
        v.shade|=(U)(first+t*(last-first)+0.5f)<<j;
    }
    return v;
}
static void triangle(U idx){
    Vertex *v[3]={&vertices[(idx>>17)&31],&vertices[(idx>>9)&31],&vertices[(idx>>1)&31]};
    float d[3];int inside=0;
    for(int j=0;j<3;j++){d[j]=v[j]->clip[2]+v[j]->clip[3];inside+=d[j]>=0;}
    if(!inside)return;
    if(inside==3){raster_triangle(*v[0],*v[1],*v[2]);return;}
    Vertex poly[4];int count=0;
    for(int j=0;j<3;j++){
        int prev=(j+2)%3;
        if((d[prev]>=0)!=(d[j]>=0))poly[count++]=near_intersection(*v[prev],*v[j],d[prev],d[j]);
        if(d[j]>=0)poly[count++]=*v[j];
    }
    for(int j=2;j<count;j++)raster_triangle(poly[0],poly[j-1],poly[j]);
}
static void graphics_cmd(U a,U b){
    U op=a>>24,i=(b>>24)&7,p=address(b);Tile *t=&tiles[i];
    switch(op){
    case 0xed:
        clip_left=(a>>14)&1023;clip_top=(a>>2)&1023;
        clip_right=(b>>14)&1023;clip_bottom=(b>>2)&1023;break;
    case 0xe3:if((a&65535)==0xa01)cycle_type=(b>>20)&3;break;
    case 0xea:
        key_scale=(key_scale&0xff000000u)|((b&0xff0000u))|((b&255u)<<8);
        key_center=(key_center&0xff000000u)|((b>>8)&0xff0000u)|(b&0xff00u);break;
    case 0xeb:
        key_scale=(key_scale&0xffffffu)|(b<<24);
        key_center=(key_center&0xffffffu)|((b&0xff00u)<<16);break;
    case 0xfc:set_combine(a,b);break;
    case 0xfb:env=b;break;
    case 0xe2:if((a&0xffff)==0x1c)render_mode=b;break;
    case 0xd9:geom=(geom&(a&0xffffff))|b;break;
    case 0xda:matrix(p,a);break;
    case 0xd8:level=b/64>level?0:level-b/64;break;
    case 0xdc:if((a&255)==10 && p+15<sizeof(ram)){
        int index=(a>>8)&255;
        if(index==6)light_address=p;
        if(index==9)ambient_address=p;
    }
    if((a&255)==8 && p<sizeof(ram)-16){vsx=(int16_t)halfword(p)/4.f;vsy=(int16_t)halfword(p+2)/4.f;vx=(int16_t)halfword(p+8)/4.f;vy=(int16_t)halfword(p+10)/4.f;}break;
    case 1:vertex_load(a,p);break;
    case 5:triangle(a);break;
    case 6:triangle(a);triangle(b);break;
    case 0xd7:texture_on=a&2;triangle_tile=(a>>8)&7;break;
    case 0xfe:zbuffer=p;break;
    case 0xfd:source=p;src_width=(a&4095)+1;src_size=(a>>19)&3;break;
    case 0xfa:prim=b;lod_fraction=a&255;break;
    case 0xf5:t->fmt=(a>>21)&7;t->size=(a>>19)&3;t->line=((a>>9)&511)*8;t->bank=a&511;t->pal=(b>>20)&15;t->ms=(b>>4)&15;t->mt=(b>>14)&15;t->mode_s=(b>>8)&3;t->mode_t=(b>>18)&3;t->shift_s=b&15;t->shift_t=(b>>10)&15;break;
    case 0xf4:case 0xf3:load_stride[t->bank]=op==0xf4?(src_width<<src_size)/2:0;tex_base[t->bank]=source+(((((a&4095)>>2)*src_width+((a>>14)&1023))<<src_size)>>1);break;
    case 0xf2:t->s=signed12((a>>12)&4095)>>2;t->t=signed12(a&4095)>>2;t->w=1+((((b>>12)-(a>>12))&4095)>>2);t->h=1+(((b-a)&4095)>>2);break;
    case 0xf0:{hd_palette_dirty=1;U first=t->bank>=256?t->bank-256:0,count=((b>>14)&1023)+1;for(U j=0;j<count&&first+j<256;j++)palette[first+j]=rgba16(halfword(source+j*2));break;}
    case 0xe4:rect_tile=i;rect[0]=(b>>14)&1023;rect[1]=(b>>2)&1023;rect[2]=(a>>14)&1023;rect[3]=(a>>2)&1023;break;
    case 0xe1:rs=(int16_t)(b>>16);rt=(int16_t)b;break;
    case 0xf1:textured_rect(b);break;
    }
}
