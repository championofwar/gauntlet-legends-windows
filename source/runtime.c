#include "cpu_state.h"
typedef uint8_t B;
static B ram[0x800000];
static const B *cart;

Q reg[32],hi,lo;
U cp0[32],pc,next,pi_ram,pi_cart;
TlbEntry tlb[32];
U instructions,dmas,dispatches,entered,module_crc,main_entered,stop_pc,stop_word,stop_address;

int status;
static B sp_mem[8192];
static U sp_addr,sp_dram,sp_status=1,sp_pc,sp_sem;
typedef struct {U address,bytes,duration,rate;} AudioDMA;
static AudioDMA ai_queue[2];
static U ai_count,ai[6],dp_end,ai_end,si_addr;static B pif[64];
static U pad_buttons;static int pad_x,pad_y;
static U vi[14],line,mi_pending,pi_domain[8];
U fp[32];
U reservation=~0u;
U fcsr;static U pif_control,mi_mask;

static U be32(const B*p){return (U)p[0]<<24|(U)p[1]<<16|(U)p[2]<<8|p[3];}
PageEntry pages[128];
static __attribute__((always_inline)) inline U translate_address(U addr){
    if(addr>=0x80000000 && addr<0xc0000000)return addr&0x1fffffff;
    U page=addr>>12,slot=page&127;
    if(pages[slot].tag==page+1)return pages[slot].base|(addr&4095);
    for(unsigned i=0;i<32;i++){
        U mask=tlb[i].mask|0x1fff,half=(mask+1)/2;
        if((addr&~mask)!=(tlb[i].hi&~mask))continue;
        U entry=tlb[i].lo[(addr&half)!=0];
        if(entry&2){
            U p=(((entry>>6)<<12)&~(half-1))|(addr&(half-1));
            pages[slot].tag=page+1;pages[slot].base=p&~4095u;return p;
        }
    }
    return ~0u;
}
U physical(U addr){return translate_address(addr);}
#include "pak.inc"
extern int QueryPerformanceCounter(int64_t*);
int profile_runtime;
int64_t display_ticks,worker_wait_ticks,audio_task_ticks;
static int64_t profile_clock(void){
    int64_t now=0;
    if(profile_runtime)QueryPerformanceCounter(&now);
    return now;
}
#include "display.inc"
static int timed_draw_list(U address){
    int64_t begin=profile_clock();
    int result=draw_list(address);
    display_ticks+=profile_clock()-begin;
    return result;
}
extern int native_audio_task(B*, const B*);
U native_audio_tasks,legacy_audio_tasks;
extern void gauntlet_audio(B*,U,U);
extern void audio_submit(const B*,U,U);
static void start_audio_dma(void){
    AudioDMA q=ai_queue[0];
    audio_submit(ram+q.address,q.bytes,q.rate);
    ai_end=instructions+q.duration;
}
static void queue_audio(U bytes){
    U address=ai[0]&0xfffff8;
    bytes&=0x3fff8;
    if(ai_count==2 || !bytes || address>sizeof(ram)-bytes)return;
    U rate=48681812/((ai[4]&0x3fff)+1);
    U duration=(U)(((Q)(bytes/4)*32317440+rate/2)/rate);
    ai_queue[ai_count++]=(AudioDMA){address,bytes,duration?duration:1,rate};
    if(ai_count==1)start_audio_dma();
}
U access_bus(U addr,unsigned size,U value,int write){
    U p=physical(addr);
    if(addr&(size-1)){status=ALIGNMENT;stop_address=addr;return 0;}
    if(p<=sizeof(ram)-size){
        if(size==4){
            if(!write)return be32(ram+p);
            U word=__builtin_bswap32(value);
            __builtin_memcpy(ram+p,&word,sizeof(word));
        }else if(size==2){
            if(!write)return (ram[p]<<8)|ram[p+1];
            ram[p]=value>>8;ram[p+1]=value;
        }else if(size==1){
            if(!write)return ram[p];
            ram[p]=value;
        }else{
            U v=0;
            for(unsigned j=0;j<size;j++){
                if(write)ram[p+j]=value>>((size-1-j)*8);
                else v=(v<<8)|ram[p+j];
            }
            return v;
        }
        return 0;
    }
    if(write && p>=0x10000000 && p<0x11000000)return 0; /* cartridge ROM is read-only */
    if(p>=0x10000000 && p<=0x11000000-size && !write){
        U v=0;for(unsigned j=0;j<size;j++)v=(v<<8)|cart[p-0x10000000+j];return v;
    }
    if(p>=0x04000000 && p<=0x04002000-size){
        U o=p-0x04000000,v=0;for(U j=0;j<size;j++){if(write)sp_mem[o+j]=value>>((size-1-j)*8);else v=v*256+sp_mem[o+j];}return v;
    }
    if(size==4 && p==0x0410000c){static U dp_status;if(write){for(int j=0;j<3;j++){if(value&(1u<<(2*j)))dp_status&=~(1u<<j);if(value&(2u<<(2*j)))dp_status|=1u<<j;}}return dp_status;}
    if(size==4 && p==0x04080000){if(write)sp_pc=value&4092;return sp_pc;}
    if(size==4 && p>=0x04040000 && p<=0x0404001c){
        U i=(p-0x04040000)/4;
        if(i==0){if(write)sp_addr=value&8191;return sp_addr;}
        if(i==1){if(write)sp_dram=value&0xffffff;return sp_dram;}
        if((i==2||i==3) && write){
            U len=((value&4095)|7)+1,blocks=((value>>12)&255)+1,skip=value>>20;
            sp_dram&=~7u;sp_addr&=~7u;
            for(U k=0;k<blocks;k++){
                if(sp_dram>sizeof(ram)-len){status=BUS;stop_address=addr;return 0;}
                for(U j=0;j<len;j++){U pos=(sp_addr&4096)|((sp_addr+j)&4095);if(i==2)sp_mem[pos]=ram[sp_dram+j];else ram[sp_dram+j]=sp_mem[pos];}
                sp_addr=(sp_addr&4096)|((sp_addr+len)&4095);sp_dram+=len+(k+1<blocks?skip:0);
            }
            return 0;
        }
        if(i==4){
            if(write){
                if(value&1)sp_status&=~1u;if(value&2)sp_status|=1;
                if(value&4)sp_status&=~2u;if(value&8)mi_pending&=~1u;if(value&16)mi_pending|=1;
                for(int j=0;j<10;j++){if(value&(32u<<(2*j)))sp_status&=~(32u<<j);if(value&(64u<<(2*j)))sp_status|=32u<<j;}
                if(!(sp_status&1)){if(be32(sp_mem+0xfc0)==1 && timed_draw_list(be32(sp_mem+0xff0))){if(suspended)return 0;sp_status|=0x203;if(sp_status&64)mi_pending|=1;dp_end=instructions+2048;}else if(be32(sp_mem+0xfc0)==2){/* The USA resident MusyX microcode is linked at physical 0xA4CA0. */
        if(be32(sp_mem+0xfd0)==0xa4ca0){
            int64_t begin=profile_clock();
            int success=native_audio_task(ram,sp_mem+0xfc0);
            audio_task_ticks+=profile_clock()-begin;
            if(!success){status=BUS;stop_address=sp_dram;return 0;}
            native_audio_tasks++;
        }else{
            gauntlet_audio(ram,be32(sp_mem+0xff0),be32(sp_mem+0xff4));
            legacy_audio_tasks++;
        }
        sp_status|=0x203;if(sp_status&64)mi_pending|=1;}else{status=BUS;stop_address=addr;}}
            }
            return sp_status;
        }
        if(i==7){U old=sp_sem;sp_sem=write?0:1;return old;}
        return 0;
    }
    if(size==4 && p>=0x04400000 && p<0x04400038){
        U idx=(p-0x04400000)/4;
        if(idx==4){if(write)mi_pending&=~8u;return line;}
        if(write)vi[idx]=value;
        return vi[idx];
    }
    if(size==4 && p==0x04300000 && write){if(value&2048)mi_pending&=~32u;return 0;}
    if(size==4 && p==0x04300008 && !write)return mi_pending;
    if(size==4 && p>=0x04600014 && p<0x04600034){U i=(p-0x04600014)/4;if(write)pi_domain[i]=value;return pi_domain[i];}
    if(size==4 && p==0x0430000c){
        if(write)for(int i=0;i<6;i++){if(value&(1u<<(2*i)))mi_mask&=~(1u<<i);if(value&(2u<<(2*i)))mi_mask|=1u<<i;}
        return mi_mask;
    }
    if(size==4 && p>=0x04500000 && p<0x04500018){
        U i=(p-0x04500000)/4;
        if(write){
            if(i==1)queue_audio(value);
            else if(i==3)mi_pending&=~4u;
            else ai[i]=value;
        }
        if(i==3)return (ai_count?0x40000000u:0)|(ai_count==2?0x80000000u:0);
        if(i==1)return ai_count?(U)((Q)(ai_end-instructions)*ai_queue[0].bytes/ai_queue[0].duration)&~7u:0;
        return ai[i];
    }
    if(size==4 && p==0x04800018){if(write)mi_pending&=~2u;return (mi_pending&2)?0x1000:0;}
    if(size==4 && write && p==0x04800000){si_addr=value&0xffffff;return 0;}
    if(size==4 && write && (p==0x04800004 || p==0x04800010) && si_addr<=sizeof(ram)-64){
        if(p==0x04800010){
            for(unsigned i=0;i<64;i++)pif[i]=ram[si_addr+i];
        }else{
            unsigned i=0,channel=0;
            while(i<63){U tx=pif[i++];if(tx==0xfe)break;if(tx==255)continue;if(!tx){channel++;continue;}
                if(i>=63)break;unsigned r=i++,rx=pif[r]&63;tx&=63;if(!tx || i+tx+rx>63)break;
                pif[r]&=63; B *reply=pif+i+tx;
                if(channel==0 && (pif[i]==0 || pif[i]==255) && rx>=3){reply[0]=5;reply[1]=0;reply[2]=pak_ready?1:0;}
                else if(channel==0 && pif[i]==1 && rx>=4){reply[0]=pad_buttons>>8;reply[1]=pad_buttons;reply[2]=(B)pad_x;reply[3]=(B)pad_y;}
                else if(channel==0 && pak_ready && ((pif[i]==2 && tx==3 && rx==33)||(pif[i]==3 && tx==35 && rx==1)))pak_block(pif+i,reply);
                else pif[r]|=0x80;
                i+=tx+rx;channel++;
            }
            pif[63]=0;
            for(unsigned j=0;j<64;j++)ram[si_addr+j]=pif[j];
        }
        mi_pending|=2;return 0;
    } // SI idle before controller initialization
    if(size==4 && p==0x1fc007fc){if(write)pif_control=value;return pif_control;}
    if(size==4 && p==0x04600010){if(write && (value&2))mi_pending&=~16u;return 0;} // synchronous PI DMA
    if(size==4 && write){
        if(p==0x04600000){pi_ram=value&0xffffff;return 0;}
        if(p==0x04600004){pi_cart=value;return 0;}
        if(p==0x0460000c){
            U count=(value&0xffffff)+1,off=pi_cart-0x10000000;
            if(off<=0x1000000 && count<=0x1000000-off && pi_ram<=sizeof(ram) && count<=sizeof(ram)-pi_ram){
                for(U i=0;i<count;i++)ram[pi_ram+i]=cart[off+i];
                dmas++;mi_pending|=16;return 0;
            }
        }
    }
    status=BUS;stop_address=addr;return 0;
}
#include "fpu.inc"
int delay_slot;
void exception(U cause,U extra){
    cp0[13]=(cp0[13]&0xff00)|(cause<<2)|extra|(delay_slot?0x80000000u:0);
    cp0[14]=pc-(delay_slot?4:0);cp0[12]|=2;delay_slot=0;
    pc=0x80000180;next=pc+4;
}
enum { VIDEO_PIXELS=416*220 };
static B scanout[VIDEO_PIXELS*2];
static U scan_rgb[65536];
static U scan_id;
U cart_scan_id(void){return scan_id;}
U displayed_origins;
/* The gameplay HUD has three repeated panel dividers at a 104-pixel pitch.
   Match their bevel on the captured image so menus and cutscenes keep their bars. */
static int has_bottom_panels(void){
    int matched=0,lit=0;
    for(int y=172;y<=215;y+=4){
        int i=2*(416*y+104);
        U a=(scanout[i]<<8|scanout[i+1])&0xfffe;
        U b=(scanout[i+208]<<8|scanout[i+209])&0xfffe;
        U c=(scanout[i+416]<<8|scanout[i+417])&0xfffe;
        if(a==b && b==c){matched++;if(a>0x1000)lit++;}
    }
    return matched>=8 && lit>=3;
}
static U hud_trim(int x,int y){
    static const unsigned char rail[]={82,123,57,29};
    int level;
    if(y<4)level=rail[y];
    else if(y>=46)level=rail[49-y]/2;
    else{
        int bevel=y<9?9-y:y>40?y-40:0;
        int grain=((x*13+y*7)^((x+y)>>1))&3;
        int dx=(x%36)-18,dy=y-25;
        int radius=(dx<0?-dx:dx)+(dy<0?-dy:dy);
        level=26+2*bevel+grain;
        if(radius==10 || radius==6)level+=dy<0?12:-5;
        if(radius<=2)level+=14;
    }
    return (U)level<<16 | (U)(level+2)<<8 | (U)(level+8);
}
static void video_retrace(void){
    U base=vi[1]&0xffffff,depth=vi[0]&3;
    if(depth==2 && base>=832){
        for(unsigned i=0;i<8;i++)if(colour_images[i] && base-colour_images[i]==832){
            base=colour_images[i];break;
        }
    }
    if(vi[2]!=416 || base>sizeof(ram)-sizeof(scanout))return;
    if(suspended && (active_colour(base) || (buffer<=base && base-buffer<416*4)))return;
    static int held_previous_retrace;
    if(scan_id && audio_pressure && !held_previous_retrace){
        held_previous_retrace=1;
        held_count++;
        return;
    }
    held_previous_retrace=0;
    if(depth==2){
        for(U i=0;i<sizeof(scanout);i++)scanout[i]=ram[base+i];
    }else if(depth==3 && base<=sizeof(ram)-4*VIDEO_PIXELS){
        for(U i=0;i<VIDEO_PIXELS;i++){
            U c=color16(be32(ram+base+4*i));scanout[2*i]=c>>8;scanout[2*i+1]=c;
        }
    }else if(!depth){for(U i=0;i<sizeof(scanout);i++)scanout[i]=0;}
    else return;
    static U last_origin=~0u;
    if(base!=last_origin){displayed_origins++;last_origin=base;}
    captured_output=requested_scale;captured_aa=edge_aa;captured_fast=fast_scale;captured_smooth=smooth_texture;
    SideImage *side=depth==2 && side_width?side_find(base):0;
    if(side && frames+1-side->stamp>3)side=0;
    capture_width=416+2*side_width;
    hd_screen_hud=depth==2 && has_bottom_panels();
    int extend_hud=side_width && hd_screen_hud;
    if(track_geometry() || side_width){
        for(int y=0;y<220;y++){
            int row=y*capture_width;
            for(int x=0;x<416;x++){
                int i=y*416+x,k=row+side_width+x;
                U id=depth==2 && track_geometry()?owners[base/2+i]:0;
                screen_owner[k]=id;screen_geometry[k]=id!=0;
                enhanced[k]=scan_rgb[scanout[2*i]*256+scanout[2*i+1]];
            }
            for(int x=0;x<side_width*2;x++){
                int i=y*SIDE_STRIDE+x,k=row+x+(x>=side_width?416:0);
                enhanced[k]=side?scan_rgb[side->color[i]]:0;
                screen_owner[k]=side?side->owner[i]:0;screen_geometry[k]=screen_owner[k]!=0;
                if(extend_hud && y>=170){
                    int distance=x<side_width?side_width-1-x:x-side_width;
                    enhanced[k]=hud_trim(distance,y-170);
                    screen_owner[k]=0;screen_geometry[k]=0;
                }
            }
        }
    }
    scan_id++;
}
#include "static_lookup.inc"
static U music_percent=100,effects_percent=100;
void cart_audio_levels(U music,U effects){
    music_percent=music>100?100:music;effects_percent=effects>100?100:effects;
}
/* Extra CPU throughput leaves VI, Count/Compare and audio DMA on the existing clock. */
static U extra_cpu=1;
U cart_toggle_extra_cpu(void){extra_cpu^=1;return extra_cpu;}
U cart_extra_cpu_enabled(void){return extra_cpu;}
static inline void execute_guest_instruction(void){
    if(mi_mask&mi_pending)cp0[13]|=0x400;else cp0[13]&=~0x400u;
    if((cp0[12]&7)==1 && (cp0[12]&cp0[13]&0xff00) && !delay_slot){
        reservation=~0u;exception(0,0);
    }
    U at=pc,p=translate_address(at);stop_pc=at;
    if(p>sizeof(ram)-4 || (at&3)){status=BUS;stop_address=at;return;}
    U w=be32(ram+p);stop_word=w;
    if(p==0x2850 && !entered){
        U crc=~0u;
        for(unsigned i=0;i<707312;i++){
            crc^=ram[0x400+i];
            for(int bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);
        }
        module_crc=~crc;entered=1;
    }
    if(p==0x2900)main_entered=1;
    /* USA snd_master_volume: each call prepares an independent 0..127
       fader target in a0. Leave its fade duration and group selection intact. */
    if((at&~0x20u)==0xe008e058u && w==0x0c024f45){
        U percent=at==0xe008e058u?music_percent:effects_percent;
        reg[4]=((U)reg[4]*percent+50)/100;
    }
    if (!static_execute(at, w)) {
        missing_steps++;
        status=OPCODE;
    }
}

static void step(void){
    U count_distance=cp0[11]-cp0[9];
    U ticks=1+(instructions&1);cp0[9]+=ticks;
    if(count_distance && count_distance<=ticks)cp0[13]|=0x8000;
    if(suspended && !(instructions&255) && (wordat(suspended)!=0xde010000 || address(wordat(suspended+4))!=suspended)){
        if(!timed_draw_list(suspended)){status=BUS;stop_address=suspended;return;}
        if(!suspended){sp_status|=0x203;if(sp_status&64)mi_pending|=1;dp_end=instructions+2048;}
    }
    if(dp_end && instructions==dp_end){mi_pending|=32;dp_end=0;}
    if(ai_count && ai_end==instructions){
        mi_pending|=4;
        if(--ai_count){ai_queue[0]=ai_queue[1];start_audio_dma();}
    }
    if(!(instructions%2048)){
        line+=2;
        if(line>=(vi[6]?vi[6]:525))line=0;
        if((line>>1)==(vi[3]>>1)){video_retrace();mi_pending|=8;}
    }
#ifdef AUTO_PLAY
    pad_buttons=(frames>230 && frames%60<8)?(frames%120<8?0x1000:0x8000):0;
#endif
    execute_guest_instruction();
}

void cart_start(const B*rom){
    for(U i=0;i<65536;i++)scan_rgb[i]=(rgba_table[i]=expand_rgba16(i))>>8;
    cart=rom;pc=be32(rom+8);next=pc+4;
    for(U i=0;i<0x100000;i++)ram[0x200000+i]=cart[0x1000+i];
    access_bus(0x80000308,4,0xb0000000,1);access_bus(0x80000300,4,1,1);
    access_bus(0x80000318,4,sizeof(ram),1);
}
void cart_pad(U buttons,int x,int y){pad_buttons=buttons;pad_x=x;pad_y=y;}
/* Stop before the next VI, DMA, DP or Count/Compare boundary. */
static U event_budget(U budget){
    U until=(0u-instructions)&2047u;
    if(suspended && ((0u-instructions)&255u)<until)until=(0u-instructions)&255u;
    if(dp_end && dp_end-instructions<until)until=dp_end-instructions;
    if(ai_count && ai_end-instructions<until)until=ai_end-instructions;
    U distance=cp0[11]-cp0[9];U timer=(U)(((Q)(distance-1)*2)/3);
    if(timer<until)until=timer;
    return until<budget?until:budget;
}
/* An unconditional jump to itself with a NOP delay slot has no effects
   between hardware events. Advance whole pairs so the CPU phase is preserved. */
static U idle_steps(U budget){
    if((mi_pending&mi_mask) || (cp0[12]&cp0[13]&0xff00))return 0;
    U branch=delay_slot?pc-4:pc;
    U p=physical(branch);
    if(p>sizeof(ram)-8 || wordat(p)!=(0x08000000u|((branch>>2)&0x03ffffff)) || wordat(p+4))return 0;
    if(delay_slot?next!=branch:next!=pc+4)return 0;
    return event_budget(budget)&~1u;
}
/* osWritebackDCacheAll walks coherent guest RAM in 16-byte lines. Collapse
   full taken iterations only. Leave interrupts and the final branch to step(). */
static U writeback_steps(U budget){
    U cursor=(U)reg[8],limit=0x80001ff0u;
    if(delay_slot || next!=pc+4 || reg[9]!=sx(limit) || reg[8]!=sx(cursor) ||
       cursor<0x80000000u || cursor>=limit || (cursor&15) || (mi_pending&mi_mask) ||
       (cp0[13]&0x400) || (cp0[12]&cp0[13]&0xff00))return 0;
    U p=physical(pc);
    if(p>sizeof(ram)-16 || be32(ram+p)!=0xbd010000u || be32(ram+p+4)!=0x0109082bu ||
       be32(ram+p+8)!=0x1420fffdu || be32(ram+p+12)!=0x25080010u)return 0;
    U loops=event_budget(budget)/4,remaining=(limit-cursor)/16;
    if(loops>remaining)loops=remaining;
    if(!loops)return 0;
    reg[8]=sx(cursor+16*loops);reg[1]=1;
    stop_pc=pc+12;stop_word=0x25080010u;native_steps+=4*loops;
    return 4*loops;
}
int cart_slice(U budget){
    U start_frame=frames;
    /* Preserve hardware ticks and the normal instruction on every tick.
       With a short audio queue, run the optional bonus on alternate ticks. */
    U bonus_mask=limit_bonus_cpu?1u:0u;
    while(!status && budget && frames==start_frame){
        if(pc==0xe0099500u){
            U skip=writeback_steps(budget);
            if(skip){instructions+=skip;cp0[9]+=skip/2*3;budget-=skip;continue;}
        }
        if((instructions&63)==2){
            U skip=idle_steps(budget);
            if(skip){instructions+=skip;cp0[9]+=skip/2*3;budget-=skip;continue;}
        }
        step();
        if(extra_cpu && !(instructions&bonus_mask) && !status && frames==start_frame)execute_guest_instruction();
        instructions++;budget--;
    }
    return status;
}
/* Directional luma-edge antialiasing works on the world layer only. */
static unsigned luminance(U c){return ((c>>16)*77+((c>>8)&255)*150+(c&255)*29)>>8;}
static U average_rgb(U a,U b){return ((a&0xfefefe)+(b&0xfefefe))/2+(a&b&0x010101);}
static U screen_mix(U a,U b,unsigned weight){
    U rb=(((a&0xff00ff)*(256-weight)+(b&0xff00ff)*weight)>>8)&0xff00ff;
    U g=(((a&0xff00)*(256-weight)+(b&0xff00)*weight)>>8)&0xff00;
    return rb|g;
}
static U edge_sample(float x,float y,int w,int h,U center){
    if(x<0)x=0;if(x>w-2)x=w-2;
    if(y<0)y=0;if(y>h-2)y=h-2;
    int ix=(int)x,iy=(int)y;
    U wx=(U)((x-ix)*256),wy=(U)((y-iy)*256);
    U c[4];
    for(int k=0;k<4;k++){
        int px=ix+(k&1),py=iy+(k>>1);
        c[k]=screen_geometry[(py/render_scale)*capture_width+px/render_scale]?enhanced[py*w+px]:center;
    }
    return screen_mix(screen_mix(c[0],c[1],wx),screen_mix(c[2],c[3],wx),wy);
}
static void filter_edges(U *dest){
    int w=capture_width*render_scale,h=220*render_scale;
    for(int y=1;y<h-1;y++)for(int x=1;x<w-1;x++){
        if(!screen_geometry[(y/render_scale)*capture_width+x/render_scale])continue;
        int i=y*w+x;
        U c=enhanced[i];int lc=luminance(c);
        int nw=luminance(enhanced[i-w-1]),ne=luminance(enhanced[i-w+1]);
        int sw=luminance(enhanced[i+w-1]),se=luminance(enhanced[i+w+1]);
        int lmin=lc,lmax=lc,ls[4]={nw,ne,sw,se};
        for(int k=0;k<4;k++){if(ls[k]<lmin)lmin=ls[k];if(ls[k]>lmax)lmax=ls[k];}
        if(lmax-lmin<12 || (lmax-lmin)*8<lmax)continue;
        float dx=(sw+se)-(nw+ne),dy=(nw+sw)-(ne+se);
        float ax=__builtin_fabsf(dx),ay=__builtin_fabsf(dy);
        float reduce=(nw+ne+sw+se)/32.f;if(reduce<2)reduce=2;
        float inv=1.f/((ax<ay?ax:ay)+reduce);
        dx*=inv;dy*=inv;
        if(dx>8)dx=8;if(dx< -8)dx=-8;if(dy>8)dy=8;if(dy< -8)dy=-8;
        U inner=average_rgb(edge_sample(x-dx/6,y-dy/6,w,h,c),edge_sample(x+dx/6,y+dy/6,w,h,c));
        U outer=average_rgb(edge_sample(x-dx/2,y-dy/2,w,h,c),edge_sample(x+dx/2,y+dy/2,w,h,c));
        U result=average_rgb(inner,outer);int l=luminance(result);
        dest[i]=l<lmin || l>lmax?inner:result;
    }
}
/* Expand a complete native frame. Neighbour comparisons round diagonal
   outlines while flat artwork and HUD pixels retain their exact colours. */
static U filtered_native[GPU_PIXELS];
static void present_fast(U *dest){
    U *src=enhanced;
    if(captured_aa){
        for(U i=0;i<(U)capture_width*220;i++)filtered_native[i]=enhanced[i];
        filter_edges(filtered_native);src=filtered_native;
    }
    int scale=captured_output,w=capture_width*scale;
    for(int y=0;y<220;y++)for(int x=0;x<capture_width;x++){
        int i=y*capture_width+x;U c=src[i],tl=c,tr=c,bl=c,br=c;
        if(screen_geometry[i] && x && y && x<capture_width-1 && y<219){
            U up=src[i-capture_width],down=src[i+capture_width],left=src[i-1],right=src[i+1];
            if(up!=down && left!=right){
                if(left==up)tl=up;if(up==right)tr=up;
                if(left==down)bl=down;if(right==down)br=down;
            }
        }
        U *out=dest+y*scale*w+x*scale;
        if(captured_smooth && screen_owner[i]){
            for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++){
                int dx=2*sx+1-scale,dy=2*sy+1-scale;
                int nx=x+(dx>0)-(dx<0),ny=y+(dy>0)-(dy<0);
                if(nx<0 || nx>=capture_width)nx=x;
                if(ny<0 || ny>=220)ny=y;
                int h=y*capture_width+nx,v=ny*capture_width+x,k=ny*capture_width+nx;
                U id=screen_owner[i];
                U wx=(dx<0?-dx:dx)*128/scale,wy=(dy<0?-dy:dy)*128/scale;
                U top=screen_mix(c,screen_owner[h]==id?src[h]:c,wx);
                U bottom=screen_mix(screen_owner[v]==id?src[v]:c,screen_owner[k]==id?src[k]:c,wx);
                out[sy*w+sx]=screen_mix(top,bottom,wy);
            }
            continue;
        }
        if(scale==2){out[0]=tl;out[1]=tr;out[w]=bl;out[w+1]=br;}
        else{
            out[0]=tl;out[1]=c;out[2]=tr;
            out[w]=c;out[w+1]=c;out[w+2]=c;
            out[2*w]=bl;out[2*w+1]=c;out[2*w+2]=br;
        }
    }
}
U picture_version;
static U original_picture(U *dest){
    static U previous;
    if(scan_id!=previous){
        if(captured_output>1 || captured_aa){
            if(!captured_fast && gpu_ready()){
                int result=gpu_draw(capture_width,captured_output,captured_aa,captured_smooth,enhanced,screen_owner,dest);
                if(result>=0){
                    if(result>0){picture_scale=(unsigned)result;picture_version++;}
                    else if(published_width!=capture_width || picture_scale!=captured_output || picture_scale==1){
                        for(U i=0;i<(U)capture_width*220;i++)dest[i]=enhanced[i];
                        picture_scale=1;picture_version++;
                    }
                    published_width=capture_width;previous=scan_id;return scan_id;
                }
            }
            picture_version++;picture_scale=captured_output;
            if(captured_output>1){
                fast_scale=1;present_fast(dest);
            }else{
                for(U i=0;i<(U)capture_width*220;i++)dest[i]=enhanced[i];
                if(captured_aa)filter_edges(dest);
            }
            published_width=capture_width;previous=scan_id;return scan_id;
        }
        picture_version++;picture_scale=1;
        if(capture_width!=416){for(int i=0;i<capture_width*220;i++)dest[i]=enhanced[i];}
        else for(U i=0;i<VIDEO_PIXELS;i++)dest[i]=scan_rgb[scanout[2*i]*256+scanout[2*i+1]];
        published_width=capture_width;previous=scan_id;
    }
    return scan_id;
}
extern int gpu_frame(U,U,U,U,const U*,const uint16_t*,U*,U,U,U,U,const HdTriangle*,const U*,const B*,const U*);
static U bitmap_w=416,bitmap_h=220;
U cart_bitmap_width(void){return bitmap_w;}
U cart_bitmap_height(void){return bitmap_h;}
static U presented_w=416,presented_h=220;
U cart_output_width(void){return presented_w;}
U cart_output_height(void){return presented_h;}
static void style_native(U *dest,U w,U h){
    const int custom_hud=hd_screen_hud && (hd_hud_size!=100 || hd_hud_anchor!=1);
    int panel_w=416*hd_hud_size/100, panel_h=50*hd_hud_size/100;
    int left=(capture_width-panel_w)*hd_hud_anchor/2,top=220-panel_h;
    int tone=hd_brightness!=100 || hd_gamma!=100;
    if(!tone && !custom_hud)return;
    for(U y=0;y<h;y++){
        int py=y*220/h;
        for(U x=0;x<w;x++){
            U c=dest[y*w+x];
            if(custom_hud && (py>=170 || py>=top)){
                int px=x*capture_width/w;
                if(px>=left && px<left+panel_w && py>=top){
                    int sx=(px-left)*416/panel_w,sy=170+(py-top)*50/panel_h;
                    int i=(sy*416+sx)*2;c=scan_rgb[scanout[i]*256+scanout[i+1]];
                }else if(py>=170)c=hud_trim(px,py-170);
            }
            if(tone)c=hd_tone[(c>>16)&255]<<16|hd_tone[(c>>8)&255]<<8|hd_tone[c&255];
            dest[y*w+x]=c;
        }
    }
}
U cart_picture(U *dest){
    static U previous_id;
    int fresh=scan_id!=previous_id;
    if(!scan_id || (!fresh && (!hd_height || !gpu_ready())))return scan_id;
    previous_id=scan_id;
    if(hd_height && gpu_ready()){
        U w=((uint64_t)hd_aspect_w*hd_height+hd_aspect_h/2)/hd_aspect_h;
        U hud=hd_screen_hud?(hd_hud_size|(hd_hud_anchor<<8)):0;
        extern int gpu_collect(U,U,U,U*);
        extern U gpu_submissions;
        static U submitted_scan;
        U before=gpu_submissions;
        int result=scan_id==submitted_scan?gpu_collect(1,w,hd_height,dest):gpu_frame(capture_width,1,captured_aa,0,enhanced,screen_owner,dest,w,hd_height,
                             hd_aniso,hud,hd_triangles,&hd_palettes[0][0],ram,hd_tone);
        if(before!=gpu_submissions)submitted_scan=scan_id;
        if(result>0){extern U gpu_read_width(void),gpu_read_height(void);
            bitmap_w=gpu_read_width();bitmap_h=gpu_read_height();picture_version++;published_width=capture_width;presented_w=w;presented_h=hd_height;picture_scale=1;return scan_id;}
        if(!fresh)return scan_id;
        if(result==0 && presented_h==hd_height && presented_w==w)return scan_id;
        if(result==0){
            for(int i=0;i<capture_width*220;i++)dest[i]=enhanced[i];
            presented_w=published_width=capture_width;presented_h=220;picture_scale=1;
            bitmap_w=presented_w;bitmap_h=presented_h;picture_version++;style_native(dest,presented_w,presented_h);return scan_id;
        }
    }
    static U neutral[9*GPU_PIXELS];
    int style=hd_brightness!=100 || hd_gamma!=100 || hd_hud_size!=100 || hd_hud_anchor!=1;
    original_picture(style?neutral:dest);
    presented_w=published_width*picture_scale;presented_h=220*picture_scale;
    bitmap_w=presented_w;bitmap_h=presented_h;
    if(style){for(U i=0;i<presented_w*presented_h;i++)dest[i]=neutral[i];style_native(dest,presented_w,presented_h);}
    return scan_id;
}
int run_cart(const B*rom){
    cart_start(rom);
    while(!status && instructions<400000000){cart_slice(1000000);}
    return status?status:LIMIT;
}
