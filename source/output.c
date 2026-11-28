#include <stdint.h>
#include <stddef.h>
typedef struct {uint16_t format,channels;uint32_t rate,bytes_per_second;uint16_t align,bits,extra;} WaveFormat;
typedef struct Header {void *data;uint32_t length,recorded;uintptr_t user;volatile uint32_t flags;uint32_t loops;struct Header *next;uintptr_t reserved;} Header;
_Static_assert(sizeof(Header)==48,"Windows x64 WAVEHDR");
extern unsigned waveOutOpen(void**,unsigned,const WaveFormat*,uintptr_t,uintptr_t,unsigned);
extern unsigned waveOutPrepareHeader(void*,Header*,unsigned),waveOutWrite(void*,Header*,unsigned);
extern unsigned waveOutPause(void*),waveOutRestart(void*);
extern unsigned waveOutReset(void*),waveOutUnprepareHeader(void*,Header*,unsigned),waveOutClose(void*);
extern unsigned cart_held_frames(void);
extern void audio_log(const char*);
enum {BLOCKS=64, CAPACITY=0x4000};
static void *device;
static Header headers[BLOCKS];
static int16_t samples[BLOCKS][CAPACITY/2];
static unsigned slot,device_rate,failed_rate,priming,underruns;
enum { NORMAL_MS=104, INITIAL_MS=264, MAX_MS=360 };
static unsigned target_ms=INITIAL_MS,stable_frames,fade_in,skipped_buffers;
static unsigned recovery_floor_ms=NORMAL_MS;
static int previous_frame[2],previous_slope[2],have_previous;
static unsigned repaired_joins,guard_frames;
static int magnitude(int x){return x<0?-x:x;}

/* Check only DMA boundaries. Leave normal slopes and nearby transients intact. */
static void smooth_join(int16_t *pcm,unsigned frames,unsigned rate){
    if(!frames)return;
    int repaired=0;
    for(unsigned ch=0;ch<2;ch++){
        if(have_previous && frames>=4){
            int first=pcm[ch],slope=pcm[2+ch]-first;
            int jump=first-previous_frame[ch];
            int expected=(slope+previous_slope[ch])/2;
            int limit=magnitude(previous_slope[ch])+magnitude(slope)+
                      magnitude(pcm[4+ch]-pcm[2+ch])+magnitude(pcm[6+ch]-pcm[4+ch]);
            int offset=expected-jump;
            /* Four neighbouring slopes estimate the local signal level. */
            if(magnitude(offset)>128+limit/2){
                unsigned ramp=rate/1000;
                if(ramp>frames)ramp=frames;
                for(unsigned i=0;i<ramp;i++){
                    int sample=pcm[2*i+ch]+offset*(int)(ramp-i)/(int)ramp;
                    pcm[2*i+ch]=(int16_t)(sample>32767?32767:sample< -32768?-32768:sample);
                }
                repaired=1;
            }
        }
        int last=pcm[2*(frames-1)+ch];
        previous_slope[ch]=last-(frames>1?pcm[2*(frames-2)+ch]:previous_frame[ch]);
        previous_frame[ch]=last;
    }
    have_previous=1;
    repaired_joins+=repaired;
}

unsigned audio_join_count(void){return repaired_joins;}
unsigned audio_dropouts(void){return underruns;}
static unsigned video_headroom_ms;
void audio_video_headroom(int high_resolution){video_headroom_ms=high_resolution?160:0;}
unsigned audio_target_ms(void){return target_ms<video_headroom_ms?video_headroom_ms:target_ms;}
static void report_count(const char *label,unsigned n){
    char line[96];unsigned i=0;
    while(*label && i<76)line[i++]=*label++;
    char digits[10];unsigned k=0;
    do{digits[k++]=(char)('0'+n%10);n/=10;}while(n);
    while(k)line[i++]=digits[--k];
    line[i++]=13;line[i++]=10;line[i]=0;audio_log(line);
}
int audio_buffer_ms(void){
    if(!device)return -1;
    unsigned bytes=0;
    for(unsigned i=0;i<BLOCKS;i++)if(headers[i].flags&16)bytes+=headers[i].length;
    return (int)(bytes*250/device_rate);
}
void audio_close(void){
    if(!device)return;
    report_count("Audio underruns: ",underruns);report_count("Audio buffers skipped: ",skipped_buffers);
    report_count("Audio joins smoothed: ",repaired_joins);
    report_count("Video frames held for audio: ",cart_held_frames());
    waveOutReset(device);
    for(unsigned i=0;i<BLOCKS;i++)if(headers[i].flags&2)waveOutUnprepareHeader(device,&headers[i],sizeof(Header));
    waveOutClose(device);device=0;slot=0;
}
void audio_submit(const unsigned char *p,unsigned bytes,unsigned rate){
    if(!bytes || bytes>CAPACITY || bytes%4 || rate<4000 || rate>96000 || rate==failed_rate)return;
    if(device && rate!=device_rate)audio_close();
    if(!device){
        WaveFormat format={1,2,rate,rate*4,4,16,0};
        if(waveOutOpen(&device,~0u,&format,0,0,0)){device=0;failed_rate=rate;audio_log("Stereo device open failed\r\n");return;}
        device_rate=rate;target_ms=INITIAL_MS;recovery_floor_ms=NORMAL_MS;guard_frames=rate*12;stable_frames=0;have_previous=0;fade_in=rate/300;priming=waveOutPause(device)==0;audio_log("Stereo device opened\r\n");
    }
    int queued=audio_buffer_ms();
    if(!priming && queued==0){
        priming=waveOutPause(device)==0;
        have_previous=0;
        fade_in=rate/300;
        target_ms=audio_target_ms();
        if(target_ms<MAX_MS){
            target_ms+=40;
            if(target_ms>MAX_MS)target_ms=MAX_MS;
        }
        /* Keep some extra headroom for recurring stalls on this audio device. */
        if(recovery_floor_ms<180)recovery_floor_ms+=24;
        ++underruns;
        if(!(underruns&(underruns-1))){
            report_count("Audio queue underruns: ",underruns);
            report_count("Audio target at recovery (ms): ",target_ms);
        }
        guard_frames=rate*24;
    }
    /* Allow short scheduling stalls, then shed latency during steady playback. */
    unsigned frames=bytes/4;
    if(!priming && !guard_frames && queued>0 && queued<56 && target_ms<MAX_MS){
        target_ms=audio_target_ms()+24;
        if(target_ms>MAX_MS)target_ms=MAX_MS;
        guard_frames=rate*24;
    }
    if(guard_frames>frames)guard_frames-=frames;else guard_frames=0;
    if(priming || queued<56 || guard_frames)stable_frames=0;
    else if(target_ms>recovery_floor_ms){
        stable_frames+=bytes/4;
        if(stable_frames>=rate*4){
            stable_frames=0;
            target_ms=target_ms>recovery_floor_ms+8?target_ms-8:recovery_floor_ms;
        }
    }
    Header *h=&headers[slot];
    if(h->flags&16){skipped_buffers++;return;}
    if(h->flags&2)if(waveOutUnprepareHeader(device,h,sizeof(*h)))return;
    for(unsigned i=0;i<bytes/2;i++)samples[slot][i]=(uint16_t)(p[2*i]<<8)|p[2*i+1];
    smooth_join(samples[slot],bytes/4,rate);
    for(unsigned i=0;i<bytes/2 && fade_in;i+=2){
        unsigned ramp=rate/300;
        for(unsigned ch=0;ch<2;ch++)samples[slot][i+ch]=(int)samples[slot][i+ch]*(int)(ramp-fade_in)/(int)ramp;
        fade_in--;
    }
    *h=(Header){.data=samples[slot],.length=bytes};
    if(waveOutPrepareHeader(device,h,sizeof(*h)) || waveOutWrite(device,h,sizeof(*h))){
        audio_log("Stereo buffer submission failed\r\n");failed_rate=rate;audio_close();return;
    }
    slot=(slot+1)%BLOCKS;
    if(priming && (audio_buffer_ms()>=(int)audio_target_ms()-9 || (headers[slot].flags&16))){
        if(waveOutRestart(device)){failed_rate=rate;audio_log("Stereo device restart failed\r\n");audio_close();}
        priming=0;
    }
}
