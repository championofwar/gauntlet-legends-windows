/* Deterministic waveOut simulation. One delayed timer tick during initial playback. */
#include <stdio.h>
#include <string.h>
#ifndef OUTPUT_SOURCE
#define OUTPUT_SOURCE "../source/output.c"
#endif
#include OUTPUT_SOURCE
static unsigned play_head, paused;
static unsigned left_in_header;
static Header *submitted[256];
static unsigned submitted_count;
void audio_log(const char *msg){(void)msg;}
unsigned cart_held_frames(void){return 0;}
unsigned waveOutOpen(void **out,unsigned id,const WaveFormat *fmt,uintptr_t cb,uintptr_t ctx,unsigned flags){*out=(void*)1;return 0;}
unsigned waveOutPause(void *d){paused=1;return 0;}
unsigned waveOutRestart(void *d){paused=0;return 0;}
unsigned waveOutWrite(void *d,Header *h,unsigned size){h->flags|=16;submitted[submitted_count++%256]=h;return 0;}
unsigned waveOutPrepareHeader(void *d,Header *h,unsigned size){h->flags|=2;return 0;}
unsigned waveOutUnprepareHeader(void *d,Header *h,unsigned size){h->flags&=~2u;return 0;}
unsigned waveOutReset(void *d){return 0;}
unsigned waveOutClose(void *d){return 0;}
static void consume(unsigned bytes){
    while(bytes && !paused && play_head<submitted_count){
        Header *h=submitted[play_head%256];
        if(!left_in_header)left_in_header=h->length;
        unsigned used=left_in_header<bytes?left_in_header:bytes;
        bytes-=used;left_in_header-=used;
        if(!left_in_header){h->flags&=~16u;play_head++;}
    }
}
int main(void){
    static unsigned char pcm[192*4];
    const unsigned rate=22050;
    do{audio_submit(pcm,sizeof pcm,rate);}while(priming);
    unsigned filled=audio_buffer_ms();
    consume(rate*230/1000*4);
    audio_submit(pcm,sizeof pcm,rate);
    printf("Initial queue %u ms, underruns after a 230 ms stall %u\n",filled,audio_dropouts());
    return audio_dropouts()!=0;
}
