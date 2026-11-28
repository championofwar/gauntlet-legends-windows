/* Test adapters for the packaged x64 PE on a Linux x64 host. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#define WIN __attribute__((ms_abi))
typedef uintptr_t Word;
typedef struct {unsigned packet;uint16_t buttons;uint8_t lt,rt;int16_t lx,ly,rx,ry;} Pad;
static Pad pad;
unsigned submissions, nonzero_buffers;
uint64_t pcm_hash=1469598103934665603ULL;
void input(unsigned buttons,int x,int y){pad.packet++;pad.buttons=buttons;pad.lx=x;pad.ly=y;}
Word call_win(void *fn,Word a,Word b,Word c,Word d){
    typedef Word (WIN *Call)(Word,Word,Word,Word);
    return ((Call)fn)(a,b,c,d);
}
WIN Word unexpected(void){abort();}
WIN Word zero(void){return 0;}
WIN Word foreground(void){return 1;}
WIN Word get_pad(unsigned player,Pad *dest){if(player)return 1167;*dest=pad;return 0;}
WIN Word audio_open(void **out){*out=(void*)1;return 0;}
WIN Word audio_write(void *device,const void *header){
    const uint8_t *data=*(const uint8_t *const *)header;
    unsigned length=*(const unsigned*)((const uint8_t*)header+8);
    unsigned any=0;
    for(unsigned i=0;i<length;i++){any|=data[i];pcm_hash=(pcm_hash^data[i])*1099511628211ULL;}
    submissions++;nonzero_buffers+=(any!=0);return 0;
}

/* Map Win32 auto-reset events and joining a worker to real POSIX threads. */
#include <pthread.h>
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned signaled,thread;
    pthread_t tid;
    unsigned (WIN *entry)(void*);
    void *argument;
} TestHandle;
WIN TestHandle *event_create(void *sa,int manual,int initial,const void *name){
    if(manual)abort();
    TestHandle *h=calloc(1,sizeof(*h));
    pthread_mutex_init(&h->mutex,0);pthread_cond_init(&h->cond,0);h->signaled=initial;
    return h;
}
WIN int event_set(TestHandle *h){
    pthread_mutex_lock(&h->mutex);h->signaled=1;pthread_cond_signal(&h->cond);
    pthread_mutex_unlock(&h->mutex);return 1;
}
static void *run_worker(void *handle){
    TestHandle *h=handle;h->entry(h->argument);return 0;
}
WIN TestHandle *thread_create(void *sa,Word stack,unsigned (WIN *entry)(void*),void *arg,unsigned flags,unsigned *tid){
    TestHandle *h=calloc(1,sizeof(*h));h->entry=entry;h->argument=arg;h->thread=1;
    if(pthread_create(&h->tid,0,run_worker,h)){free(h);return 0;}
    return h;
}
WIN unsigned handle_wait(TestHandle *h,unsigned timeout){
    if(timeout!=~0u)abort();
    if(h->thread)return pthread_join(h->tid,0);
    pthread_mutex_lock(&h->mutex);
    while(!h->signaled)pthread_cond_wait(&h->cond,&h->mutex);
    h->signaled=0;pthread_mutex_unlock(&h->mutex);return 0;
}
WIN int handle_close(TestHandle *h){
    if(!h->thread){pthread_mutex_destroy(&h->mutex);pthread_cond_destroy(&h->cond);}
    free(h);return 1;
}
