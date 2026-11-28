/* Global draw state is immutable until both threads finish this batch. */
extern void *CreateEventW(void*,int,int,const uint16_t*);
extern void *CreateThread(void*,uintptr_t,unsigned (*)(void*),void*,unsigned,unsigned*);
extern int SetEvent(void*),CloseHandle(void*);
extern unsigned WaitForSingleObject(void*,unsigned);
static void *worker_ready,*worker_finished,*worker_thread;
enum { BATCH_LIMIT=32 };
static RasterRows row_batch[BATCH_LIMIT];
static U batch_count,batch_pixels;
U split_batches;
static void run_batch(int parity,int step){
    for(U i=0;i<batch_count;i++){
        const RasterRows *job=&row_batch[i];
        int first=job->y0+(step==2 && (job->y0&1)!=parity);
        raster_rows(job,first,job->y1,step);
    }
}
enum { PARKED, POLLING, DRAW, DRAWING, EXIT };
static unsigned worker_state,job_done;
unsigned split_triangles;

static int dispatch_rows(void){
    __atomic_store_n(&job_done,0,__ATOMIC_RELAXED);
    if(__atomic_exchange_n(&worker_state,DRAW,__ATOMIC_ACQ_REL)!=PARKED)return 1;
    if(SetEvent(worker_ready))return 1;
    unsigned expected=DRAW;
    return !__atomic_compare_exchange_n(&worker_state,&expected,PARKED,0,
                                        __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void wait_for_rows(void){
    for(int i=0;i<256;i++){
        if(__atomic_load_n(&job_done,__ATOMIC_ACQUIRE))return;
        __builtin_ia32_pause();
    }
    while(!__atomic_load_n(&job_done,__ATOMIC_ACQUIRE))
        WaitForSingleObject(worker_finished,0xffffffffu);
}
static unsigned raster_worker(void *unused){
    (void)unused;
    for(;;){
        unsigned state=__atomic_load_n(&worker_state,__ATOMIC_ACQUIRE);
        if(state==EXIT)break;
        if(state==DRAW && __atomic_compare_exchange_n(&worker_state,&state,DRAWING,0,
                                                      __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
            run_batch(1,2);
            __atomic_store_n(&worker_state,POLLING,__ATOMIC_RELEASE);
            __atomic_store_n(&job_done,1,__ATOMIC_RELEASE);
            SetEvent(worker_finished);
        }
        /* Hand off nearby triangles cheaply, then sleep between frames. */
        for(int i=0;i<512 && __atomic_load_n(&worker_state,__ATOMIC_ACQUIRE)==POLLING;i++)
            __builtin_ia32_pause();
        unsigned expected=POLLING;
        __atomic_compare_exchange_n(&worker_state,&expected,PARKED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        if(__atomic_load_n(&worker_state,__ATOMIC_ACQUIRE)==PARKED)
            WaitForSingleObject(worker_ready,0xffffffffu);
    }
    return 0;
}
void raster_worker_stop(void){
    if(worker_thread){
        if(__atomic_exchange_n(&worker_state,EXIT,__ATOMIC_ACQ_REL)==PARKED)
            SetEvent(worker_ready);
        WaitForSingleObject(worker_thread,0xffffffffu);
        CloseHandle(worker_thread);
        worker_thread=0;
    }
    if(worker_ready)CloseHandle(worker_ready);
    if(worker_finished)CloseHandle(worker_finished);
    worker_ready=worker_finished=0;
}
int raster_worker_start(void){
    if(worker_thread)return 1;
    worker_state=PARKED;
    worker_ready=CreateEventW(0,0,0,0);
    worker_finished=CreateEventW(0,0,0,0);
    if(worker_ready && worker_finished)worker_thread=CreateThread(0,0,raster_worker,0,0,0);
    if(!worker_thread)raster_worker_stop();
    return worker_thread!=0;
}
static int intersects(uint64_t a,uint64_t end_a,uint64_t b,uint64_t end_b){
    return a<end_b && b<end_a;
}
static int independent_rows(void){
    /* Keep feedback textures and overlapping colour/depth images in draw order. */
    uint64_t span=2ull*width*220,ce=buffer+span,ze=zbuffer+span;
    if(width!=416 || ce>sizeof(ram) || ze>sizeof(ram) || (buffer&1) || (zbuffer&1))return 0;
    if(intersects(buffer,ce,zbuffer,ze))return 0;
    if(!texture_on)return 1;
    if(uses_tile_pair(triangle_tile))return 0; /* Second tile may read the active target. */
    const Tile *t=&tiles[triangle_tile];
    U xmax=t->ms?(1u<<t->ms)-1:t->w>0?(U)t->w-1:0;
    U ymax=t->mt?(1u<<t->mt)-1:t->h>0?(U)t->h-1:0;
    U stride=load_stride[t->bank]?load_stride[t->bank]:t->line*(t->size==3?2:1);
    uint64_t begin=tex_base[t->bank];
    uint64_t end=begin+(uint64_t)ymax*stride+(((uint64_t)xmax<<t->size)>>1)+4;
    return end<=sizeof(ram) && !intersects(begin,end,buffer,ce) && !intersects(begin,end,zbuffer,ze);
}

enum { MIN_BATCH_PIXELS=8192 };
static void flush_rows(void){
    if(!batch_count)return;
    if(batch_pixels>=MIN_BATCH_PIXELS && dispatch_rows()){
        run_batch(0,2);
        if(!__atomic_load_n(&job_done,__ATOMIC_ACQUIRE)){
            int64_t begin=profile_clock();
            wait_for_rows();
            worker_wait_ticks+=profile_clock()-begin;
        }
        split_triangles+=batch_count;
        split_batches++;
    }else run_batch(0,1);
    batch_count=batch_pixels=0;
}
static int draw_target_overlap(U addr,U bytes){
    uint64_t end=(uint64_t)addr+bytes,span=2ull*width*220;
    return intersects(addr,end,buffer,buffer+span) || intersects(addr,end,zbuffer,zbuffer+span);
}
