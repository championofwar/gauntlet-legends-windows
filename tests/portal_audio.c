/* Recover from a drained HD queue and exercise another late tick on the portal walk. */
#define main original_stall_test
#include "audio_queue.c"
#undef main
int main(void){
    static unsigned char pcm[192*4];
    const unsigned rate=22050;
    audio_video_headroom(1);
    do{audio_submit(pcm,sizeof pcm,rate);}while(priming);
    target_ms=NORMAL_MS;
    consume(rate*4);
    do{audio_submit(pcm,sizeof pcm,rate);}while(priming);
    unsigned rebuffered=audio_buffer_ms();
    consume((rate*180/1000)*4);
    audio_submit(pcm,sizeof pcm,rate);
    printf("Recovery queue %u ms; underruns after two stalls %u\n",rebuffered,underruns);
    return underruns!=1;
}
