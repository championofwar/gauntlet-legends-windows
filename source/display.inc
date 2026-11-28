#include "visuals.inc"
static int clip_left,clip_top,clip_right=416,clip_bottom=220;
static U cycle_type=3;
static U frames,buffer,width=416,seg[16],draw_errors;
/* Track colour images so VI's one-line origin bias cannot cross into the next image. */
static U colour_images[8],colour_cursor;
static void remember_colour_image(U base){
    for(unsigned i=0;i<8;i++)if(colour_images[i]==base)return;
    colour_images[colour_cursor++&7]=base;
}
static U address(U v){return ((v&0xffffff)+seg[(v>>24)&15])&0x7fffff;}
static U wordat(U a){return a<=sizeof(ram)-4?be32(ram+a):0;}
static void pixel(int x,int y,U color){
    if(x<clip_left || x>=clip_right || y<clip_top || y>=clip_bottom || (U)x>=width || (U)y>=480)return;
    U pos=buffer+2*((U)y*width+(U)x);
    if(pos<=sizeof(ram)-2){ram[pos]=color>>8;ram[pos+1]=color;overlay_written(pos,2);}
}
static unsigned held_count;
static int audio_pressure;
static int limit_bonus_cpu;
void cart_audio_priority(int remaining,int target){
    /* Hysteresis lets a short queue recover before restoring full boost. */
    if(remaining<0 || remaining>=96)limit_bonus_cpu=0;
    else if(remaining<56)limit_bonus_cpu=1;
    int low=target-32;
    if(low>112)low=112;
    if(remaining<0 || remaining>=target-12)audio_pressure=0;
    else if(remaining<low)audio_pressure=1;
}
unsigned cart_held_frames(void){return held_count;}
#include "raster.inc"
static U active_targets[8],active_count;
static int active_colour(U base){
    for(U i=0;i<active_count;i++)if(active_targets[i]==base)return 1;
    return 0;
}
static U suspended,stack[32],list_depth,fill;
static int draw_list(U p){
    if(!suspended){
        list_depth=0;active_count=0;
    }
    suspended=0;
    for(U i=0;i<100000 && p<=sizeof(ram)-8;i++){
        if(batch_count && draw_target_overlap(p,8))flush_rows();
        U a=wordat(p),b=wordat(p+4),op=a>>24;p+=8;
#ifdef TRACE_DL
        if(frames==100 || frames==240 || frames==420 || frames==820 || frames==1200)fprintf(stderr,"%u %08x %08x %08x\n",frames,p-8,a,b);
#endif
        /* Vertex loads copy into a separate cache. Their source, lights and
           matrices must see any earlier writes to the guest framebuffer. */
        if(batch_count && op!=5 && op!=6){
            int vertex_safe=op==1 && !draw_target_overlap(address(b),16*((a>>12)&255)) &&
                !draw_target_overlap(light_address,16) && !draw_target_overlap(ambient_address,16);
            if(!vertex_safe)flush_rows();
        }
        graphics_cmd(a,b);
        if(op==0xdb && (a>>16&255)==6)seg[(a>>2)&15]=b;
        if(op==0xde){if(!(a&0x10000)){if(list_depth==32)return 0;stack[list_depth++]=p;}U target=address(b);if(target==p-8){suspended=target;return 2;}p=target;}
        if(op==0xdf){if(!list_depth){frames++;return 1;}p=stack[--list_depth];}
        if(op==0xff){width=(a&4095)+1;buffer=address(b);if(width==416 && ((a>>19)&3)==2 && buffer!=zbuffer){remember_colour_image(buffer);if(side_width)side_begin(buffer,frames+1);if(active_count<8 && !active_colour(buffer))active_targets[active_count++]=buffer;}}
        if(op==0xf7)fill=b;
        if(op==0xf6){
            int x0=(b>>14)&1023,y0=(b>>2)&1023,x1=(a>>14)&1023,y1=(a>>2)&1023;
            if(cycle_type<2){x1--;y1--;}
            if(cycle_type==3){
                if(x0<clip_left)x0=clip_left;
                if(y0<clip_top)y0=clip_top;
                if(x1>=clip_right)x1=clip_right-1;
                if(y1>=clip_bottom)y1=clip_bottom-1;
                if(x1>=(int)width)x1=(int)width-1;
                if(y1>=480)y1=479;
                if(x0>x1 || y0>y1)continue;
                for(int y=y0;y<=y1;y++){
                    U first=buffer+2*((U)y*width+x0);
                    U count=x1-x0+1;
                    if(first>=sizeof(ram))continue;
                    if(count>(sizeof(ram)-first)/2)count=(sizeof(ram)-first)/2;
                    U bytes=count*2;
                    B *dest=ram+first;
                    U pair=(x0&1)?(fill<<16)|(fill>>16):fill;
                    U pattern=__builtin_bswap32(pair);
                    for(;count>=2;count-=2,dest+=4)__builtin_memcpy(dest,&pattern,4);
                    if(count){dest[0]=pair>>24;dest[1]=pair>>16;}
                    overlay_written(first,bytes);
                }
                continue;
            }
            for(int y=y0;y<=y1 && y<480;y++)for(int x=x0;x<=x1 && (U)x<width;x++){
                composite(x,y,prim);
            }
        }
    }
    flush_rows();
    return 0;
}
