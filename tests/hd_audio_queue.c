/* An already primed queue, with a recurring late render tick in steady play. */
#define main startup_stall_test
#include "audio_queue.c"
#undef main
int main(int argc,char **argv){
    (void)argv;
    static unsigned char pcm[192*4];
    const unsigned rate=22050;
    do{audio_submit(pcm,sizeof pcm,rate);}while(priming);
    target_ms=NORMAL_MS;
    int hd=argc>1;
    audio_video_headroom(hd);
    for(int tick=1;tick<=800;tick++){
        while(audio_buffer_ms()<(int)audio_target_ms())audio_submit(pcm,sizeof pcm,rate);
        unsigned delay=tick%80==0?140:10;
        consume((rate*delay/1000)*4);
        audio_submit(pcm,sizeof pcm,rate);
    }
    printf("%s queue: %u underruns with repeated 140 ms late ticks\n",hd?"High-resolution":"Normal",audio_dropouts());
    return hd?(audio_dropouts()!=0):0;
}
