extern int QueryPerformanceCounter(int64_t*),QueryPerformanceFrequency(int64_t*);
extern H CreateSolidBrush(D);
extern int DeleteObject(H),SetStretchBltMode(H,int);
extern const char *gpu_device_name(void);
extern unsigned picture_version;
static uint16_t *append_number(uint16_t *out,unsigned n){
    uint16_t digits[10];unsigned count=0;
    do{digits[count++]='0'+n%10;n/=10;}while(n);
    while(count)*out++=digits[--count];*out=0;return out;
}
static uint16_t *append_text(uint16_t *out,W s){
    while(*s)*out++=*s++;*out=0;return out;
}
extern unsigned gpu_submissions,gpu_deferred,gpu_texture_bytes,displayed_origins;
static int64_t cpu_time,graphics_time,paint_time;
static unsigned paint_count,source_rate,upload_kb;
static int64_t stage_frequency(void){
    static int64_t hz;if(!hz)QueryPerformanceFrequency(&hz);return hz;
}
static int64_t stage_clock(void){int64_t ticks;QueryPerformanceCounter(&ticks);return ticks;}
static char *metric(char *p,const char *label,unsigned n){
    while(*label)*p++=*label++;
    char digits[10];int count=0;do{digits[count++]='0'+n%10;n/=10;}while(n);
    while(count)*p++=digits[--count];return p;
}
extern unsigned audio_join_count(void);
static unsigned joins_at_sample,largest_paint_gap_ms;
static int lowest_queue_ms=-1;
static int64_t last_distinct_paint;
static void record_paint(int64_t now){
    if(last_distinct_paint){
        unsigned gap=(unsigned)((now-last_distinct_paint)*1000/stage_frequency());
        if(gap>largest_paint_gap_ms)largest_paint_gap_ms=gap;
    }
    last_distinct_paint=now;
}
static char *timing_fields(char *p){
    int queued=audio_buffer_ms();
    if(queued>=0)p=metric(p," queue_ms=",queued);
    if(lowest_queue_ms>=0)p=metric(p," queue_min_ms=",(unsigned)lowest_queue_ms);
    p=metric(p," joins=",audio_join_count()-joins_at_sample);
    return metric(p," max_paint_gap_ms=",largest_paint_gap_ms);
}
static void timing_marker(void){
    char line[320],*p=line;
    p=metric(p,"F4 marker scan=",cart_scan_id());
    p=metric(p," origin_changes=",displayed_origins);
    p=metric(p," underruns=",audio_dropouts());
    p=timing_fields(p);
    *p++=13;*p++=10;*p=0;startup_note(line);
}
static void log_perf_sample(void){
    static int64_t since;
    extern unsigned split_triangles,split_batches;
    static unsigned previous_split,previous_batches;
    static unsigned old_src,old_scan,old_gpu,old_paint,old_busy,old_held;
    extern unsigned cart_held_frames(void);
    int64_t now=stage_clock(),dt=now-since,hz=stage_frequency();
    if(since && dt<hz*3)return;
    unsigned held=cart_held_frames();
    extern int64_t display_ticks,worker_wait_ticks,audio_task_ticks;
    if(since){
        char line[768],*p=line;
        source_rate=(displayed_origins-old_src)*hz/dt;
        upload_kb=gpu_texture_bytes/1024;
        p=metric(p,"perf world_h=",cart_output_height());
        p=metric(p," swaps/s=",source_rate);
        p=metric(p," scans/s=",(observed_frame-old_scan)*hz/dt);
        p=metric(p," gpu_submits/s=",(gpu_submissions-old_gpu)*hz/dt);
        p=metric(p," paints/s=",(paint_count-old_paint)*hz/dt);
        p=metric(p," deferred_polls=",gpu_deferred-old_busy);
        p=metric(p," game_cpu_ms/s=",cpu_time*1000/dt);
        p=metric(p," display_list_ms/s=",display_ticks*1000/dt);
        p=metric(p," raster_wait_ms/s=",worker_wait_ticks*1000/dt);
        p=metric(p," audio_task_ms/s=",audio_task_ticks*1000/dt);
        p=metric(p," gfx_host_ms/s=",graphics_time*1000/dt);
        p=metric(p," paint_ms/s=",paint_time*1000/dt);
        p=metric(p," held_scans=",held-old_held);
        p=metric(p," split_batches=",split_batches-previous_batches);
        p=metric(p," split_tris=",split_triangles-previous_split);
        p=metric(p," texture_KiB=",upload_kb);
        p=metric(p," underruns=",audio_dropouts());
        p=timing_fields(p);
        *p++=13;*p++=10;*p=0;startup_note(line);
    }
    lowest_queue_ms=-1;largest_paint_gap_ms=0;joins_at_sample=audio_join_count();
    previous_split=split_triangles;previous_batches=split_batches;
    since=now;old_src=displayed_origins;old_scan=observed_frame;old_gpu=gpu_submissions;
    old_paint=paint_count;old_busy=gpu_deferred;old_held=held;
    display_ticks=worker_wait_ticks=audio_task_ticks=0;
    cpu_time=graphics_time=paint_time=0;
}

static void draw_performance(H dc,Rect client){
    static int64_t freq,last;
    static unsigned last_image,ms[60],head,count;
    int64_t now;
    if(!freq && !QueryPerformanceFrequency(&freq))return;
    if(!QueryPerformanceCounter(&now))return;
    if(last_image!=picture_version){
        if(last && now>last && now-last<freq*3){
            ms[head]=(unsigned)((now-last)*10000/freq);
            head=(head+1)%60;if(count<60)count++;
        }else count=head=0;
        last=now;last_image=picture_version;
    }
    unsigned sum=0;for(unsigned i=0;i<count;i++)sum+=ms[i];
    uint16_t line[384],*p=line;
    p=append_number(p,sum && now-last<freq?(count*10000+sum/2)/sum:0);p=append_text(p,TXT(" FPS   "));
    unsigned interval=count?ms[(head+59)%60]:0;
    p=append_number(p,interval/10);p=append_text(p,TXT("."));p=append_number(p,interval%10);
    p=append_text(p,TXT(" ms   "));p=append_number(p,cart_output_width());p=append_text(p,TXT(" x "));p=append_number(p,cart_output_height());
    p=append_text(p,TXT("\n"));
    if((prefs[9]||prefs[4]>1||prefs[5]) && gpu_ready()){
        const char *name=gpu_device_name();for(int i=0;i<200 && name[i];i++)*p++=(unsigned char)name[i];
        *p=0;
    }else p=append_text(p,TXT("CPU renderer"));
    p=append_text(p,TXT("\nAudio underruns  "));p=append_number(p,audio_dropouts());
    extern unsigned cart_extra_cpu_enabled(void);
    if(cart_extra_cpu_enabled())append_text(p,TXT("    F5 boost on"));
    Rect panel={8,8,client.r<365?client.r:365,108};FillRect(dc,&panel,GetStockObject(4));
    Rect text={15,12,panel.r-4,79};SetTextColor(dc,0xffffff);SetBkMode(dc,1);DrawTextW(dc,line,-1,&text,0x800);
    H pen=CreateSolidBrush(0x99cf84);
    if(pen){
        for(unsigned i=0;i<count;i++){
            unsigned t=ms[(head+60-count+i)%60],height=t>=1000?26:1+t*25/1000;
            Rect bar={15+(int)i*5,103-(int)height,19+(int)i*5,103};FillRect(dc,&bar,pen);
        }
        DeleteObject(pen);
    }
}
