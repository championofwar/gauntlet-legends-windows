#include <stdint.h>
#include <stddef.h>

typedef void *H;
typedef uintptr_t WP;
typedef intptr_t LP;
typedef uint32_t D;
typedef const uint16_t *W;
typedef LP (*WndProc)(H,D,WP,LP);
typedef struct {int l,t,r,b;} Rect;
typedef struct {unsigned style;WndProc proc;int ce,we;H instance,icon,cursor,brush;W menu,name;} Class;
typedef struct {H hwnd;D message;WP wp;LP lp;D time;int x,y;D priv;} Msg;
typedef struct {H dc;int erase;Rect rect;int restore,update;unsigned char reserved[32];} Paint;
typedef struct {D size;H owner,instance;W filter;uint16_t *custom;D maxcustom,index;uint16_t *file;D maxfile;
 uint16_t *filetitle;D maxtitle;W initial,title;D flags;uint16_t offset,extension;W defext;LP customdata;
 H hook;W template_name;H reserved;D dwReserved,flagsEx;} OpenFile;
typedef struct {D size;int w,h;uint16_t planes,depth;D compression,imagesize;int x,y;D used,important;} Bitmap;
_Static_assert(sizeof(OpenFile)==152 && sizeof(Msg)==48 && sizeof(Class)==72 && sizeof(Paint)==72,"Win64 layout");
extern H GetModuleHandleW(W),CreateFileW(W,D,D,H,D,D,H),LoadCursorW(H,W),BeginPaint(H,Paint*);
extern int ReadFile(H,void*,D,D*,H),CloseHandle(H),GetOpenFileNameW(OpenFile*),MessageBoxW(H,W,W,D);
extern D GetFileSize(H,D*),CommDlgExtendedError(void);
extern void ExitProcess(D),PostQuitMessage(int);
extern uint16_t RegisterClassW(const Class*);
extern H CreateWindowExW(D,W,W,D,int,int,int,int,H,H,H,H);
extern int ShowWindow(H,int),UpdateWindow(H),GetMessageW(Msg*,H,D,D),TranslateMessage(const Msg*);
extern LP DispatchMessageW(const Msg*),DefWindowProcW(H,D,WP,LP);
extern int EndPaint(H,const Paint*),GetClientRect(H,Rect*),DestroyWindow(H),FillRect(H,const Rect*,H);
extern H GetStockObject(int);
extern int StretchDIBits(H,int,int,int,int,int,int,int,int,const void*,const Bitmap*,D,D);
#define TXT(x) ((W)L##x)
static unsigned char rom[1<<24];
static uint16_t path[32768];
static H log_file;
extern D GetModuleFileNameW(H,uint16_t*,D);
extern int WriteFile(H,const void*,D,D*,H),FlushFileBuffers(H);
static void startup_note(const char *message){
    if(!log_file || log_file==(H)(intptr_t)-1)return;
    D len=0,written;while(message[len])len++;
    WriteFile(log_file,message,len,&written,0);
}
extern D instructions,stop_pc,stop_word,stop_address;
extern H AddVectoredExceptionHandler(D, int (*)(void*));
extern D GetCurrentProcessId(void);
extern void GetLocalTime(uint16_t*);
static uintptr_t image_base;
static volatile D observed_frame;
static const char *volatile phase="startup";
static void hex_field(char **end,const char *label,uintptr_t value,int digits){
    char *p=*end;
    while(*label)*p++=*label++;
    for(int i=digits-1;i>=0;i--)*p++="0123456789ABCDEF"[(value>>(4*i))&15];
    *p=0;*end=p;
}
extern D native_steps,copied_steps,tower_steps,missing_steps,native_audio_tasks,legacy_audio_tasks;
static void game_state(void){
    char buf[240],*p=buf;
    hex_field(&p," frame=",observed_frame,8);
    hex_field(&p," guest_pc=",stop_pc,8);
    hex_field(&p," instruction=",stop_word,8);
    hex_field(&p," guest_fault_address=",stop_address,8);
    hex_field(&p," steps=",instructions,8);
    hex_field(&p," native=",native_steps,8);
    hex_field(&p," copied=",copied_steps,8);
    hex_field(&p," missing=",missing_steps,8);
    hex_field(&p," tower=",tower_steps,8);
    hex_field(&p," rsp_audio=",native_audio_tasks,8);
    hex_field(&p," other_audio=",legacy_audio_tasks,8);
    *p++=13;*p++=10;*p=0;startup_note(buf);
}
typedef struct {
    D code,flags;
    void *nested,*address;
    D count;
    uintptr_t info[15];
} ExceptionRecord;
_Static_assert(offsetof(ExceptionRecord,info)==32,"Win64 exception layout");
static int capture_fault(void *exception_pointers){
    ExceptionRecord *r=*(ExceptionRecord**)exception_pointers;
    static int busy;
    if(busy || (r->code&0xf0000000)!=0xc0000000)return 0;
    busy=1;
    char buf[240],*p=buf;
    hex_field(&p,"Windows exception=",r->code,8);
    hex_field(&p," host_pc=",(uintptr_t)r->address,16);
    hex_field(&p," exe_base=",image_base,16);
    if(r->code==0xc0000005 && r->count>=2){
        hex_field(&p," access_kind=",r->info[0],2);
        hex_field(&p," address=",r->info[1],16);
    }
    *p++=13;*p++=10;*p=0;startup_note(buf);
    startup_note(phase);game_state();if(log_file && log_file!=(H)(intptr_t)-1)FlushFileBuffers(log_file);busy=0;
    return 0;
}
static void session_header(void){
    uint16_t t[8]={0};GetLocalTime(t);
    char msg[]="\r\n--- Towerbound static playtest 17 | 0000-00-00 00:00:00 ---\r\n";
    const int indices[]={0,1,3,4,5,6};
    char *p=msg;while(*p!='|')p++;p+=2;
    for(int i=0;i<6;i++){
        int digits=i?2:4,n=t[indices[i]];
        for(int j=digits-1;j>=0;j--){p[j]='0'+n%10;n/=10;}
        p+=digits+1;
    }
    startup_note(msg);
}
void audio_log(const char *message){startup_note(message);}
extern void audio_close(void);
static void open_log(void){
    static uint16_t filename[32768];
    D n=GetModuleFileNameW(0,filename,32768);
    if(!n||n>=32768)return;
    while(n && filename[n-1]!=92 && filename[n-1]!=47)n--;
    static const uint16_t suffix[]=L"Gauntlet-log.txt";
    if(n+sizeof(suffix)/2>32768)return;
    for(unsigned i=0;i<sizeof(suffix)/2;i++)filename[n+i]=suffix[i];
    log_file=CreateFileW(filename,4,3,0,4,0x80,0);
    if(log_file==(H)(intptr_t)-1){
        MessageBoxW(0,TXT("The game could not open Gauntlet-log.txt. Extract the ZIP to a writable folder to enable logging."),TXT("Gauntlet playtest"),48);
        return;
    }
    session_header();
}
#include "save.inc"
static int validate_rom(void){
    D crc=~0u,table[256];
    for(unsigned i=0;i<256;i++){
        D x=i;for(int b=0;b<8;b++)x=(x>>1)^((0u-(x&1))&0xedb88320u);
        table[i]=x;
    }
    for(unsigned i=0;i<sizeof(rom);i++)crc=(crc>>8)^table[(crc^rom[i])&255];
    return ~crc!=0x64765e82u;
}
typedef struct {
    D packet;
    uint16_t buttons;
    uint8_t lt,rt;
    int16_t lx,ly,rx,ry;
} PadState;
_Static_assert(sizeof(PadState)==16,"XInput state layout");
extern D XInputGetState(D,PadState*);
extern WP SetTimer(H,WP,D,H);
extern int SetWindowTextW(H,W),InvalidateRect(H,const Rect*,int),DrawTextW(H,W,int,Rect*,D),SetBkMode(H,int);
extern D SetTextColor(H,D);
extern H GetForegroundWindow(void);

extern int16_t GetAsyncKeyState(int);
extern void cart_start(const unsigned char*),cart_pad(D,int,int);
extern int cart_slice(D);
extern D cart_scan_id(void);
extern void cart_aspect(int,int);
extern unsigned cart_picture_width(void);
extern D cart_picture(D*), stop_pc,stop_word;
extern unsigned cart_output_width(void),cart_output_height(void);
static D picture[5760*2160];static int slot=-1,stopped;
static Bitmap bitmap={40,416,-220,1,32,0,416*220*4,0,0,0,0};
static void keyboard_read(void);
#include "controls.inc"
#include "keyboard.inc"
#include "sony_input.inc"
#include "graphics_menu.inc"
void controller(H hwnd){
    PadState state={0};
    if(slot<0 || XInputGetState(slot,&state)){
        for(slot=0;slot<4;slot++)if(!XInputGetState(slot,&state))break;
        if(slot==4){slot=-1;state=(PadState){0};}
    }
    ps_extras=0;
    if(sony_live()){state=sony_state;ps_extras=sony_extra;slot=-1;}
    int x,y;D buttons=mapped_pad(&state,&x,&y);
    if(GetForegroundWindow()!=hwnd){stop_motor();previous_actions=buttons;cart_pad(0,0,0);return;}
    if(slot<0)stop_motor();
    if(controls.mode==1 && (buttons&~previous_actions&0xe000))pulse(65);
    previous_actions=buttons;
    if(pulse_active && (int32_t)(GetTickCount()-motor_until)>=0)pulse_active=0;
    apply_motor(slot,(pulse_active || (controls.mode==2 && pak_motor))?speeds[controls.strength-1]:0);
    keyboard_input(&buttons,&x,&y);
    cart_pad(buttons,x,y);
}
extern int audio_buffer_ms(void);
extern unsigned audio_dropouts(void);
extern int gpu_open(void);
extern void gpu_close(void);
extern void cart_audio_priority(int,int);
extern unsigned audio_target_ms(void);
extern D GetTickCount(void),timeBeginPeriod(D),timeEndPeriod(D);
#include "performance.inc"
static H live_window,game_window;
static void toggle_live(H);
static LP window_proc(H hwnd,D msg,WP key,LP param){
    /* Key messages survive a tap between render ticks. Bit 30 marks a repeat. */
    if(msg==0xff || msg==0xfe)raw_pad_event(msg,key,param);
    if(msg==8)stop_motor();
    if(msg==0x100 && key==0x74){
        extern unsigned cart_toggle_extra_cpu(void);
        if(!(param&(1u<<30))){
            startup_note(cart_toggle_extra_cpu()?"Extra guest CPU on\r\n":"Extra guest CPU off\r\n");
        }
        return 0;
    }
    if(msg==0x100 && key==0x73){if(!(param&(1u<<30)))timing_marker();return 0;}
    if(msg==0x100 && key==0x72 && !(param&(1u<<30))){prefs[15]^=1;save_graphics();InvalidateRect(hwnd,0,0);return 0;}
    if(msg==0x113 && !stopped){
        Rect client;GetClientRect(hwnd,&client);
        extern void gpu_viewport(unsigned,unsigned);
        int vw=client.r,vh=client.b;
        if(prefs[8] && vw>screens[prefs[8]].width)vw=screens[prefs[8]].width;
        if(prefs[8] && vh>screens[prefs[8]].height)vh=screens[prefs[8]].height;
        gpu_viewport(vw,vh);
        cart_aspect(prefs[8]?screens[prefs[8]].width:client.r,prefs[8]?screens[prefs[8]].height:client.b);
        phase="controller";controller(hwnd);
        phase="CPU and graphics";
        static int pumping;
        if(pumping)return 0;
        pumping=1;
        D reason=0;
        extern void audio_video_headroom(int);
        audio_video_headroom(prefs[9]>=3 && gpu_ready());
        int queued=audio_buffer_ms();
        if(queued>=0 && (lowest_queue_ms<0 || queued<lowest_queue_ms))lowest_queue_ms=queued;
        unsigned refill_budget=queued>=0 && queued<96?32:18;
        int64_t cpu_begin=stage_clock();
        if(queued<(int)audio_target_ms()){
            D initial_scan=cart_scan_id();
            for(int i=0;i<48;i++){
                cart_audio_priority(audio_buffer_ms(),(int)audio_target_ms());
                reason=cart_slice(100000);
                int remaining_audio=audio_buffer_ms();
                if(prefs[9] && gpu_ready() && cart_scan_id()!=initial_scan)break;
                if(reason || remaining_audio>=(int)audio_target_ms() || stage_clock()-cpu_begin>=stage_frequency()*refill_budget/1000)break;
                if(queued<0 && i>=2)break;
            }
        }
        cpu_time+=stage_clock()-cpu_begin;
        if(pak_dirty)pak_flush();
        pumping=0;
        if(reason){
            stop_motor();stopped=1;char buf[60],*p=buf;hex_field(&p,"Runtime stop reason=",reason,8);startup_note(buf);game_state();
            uint16_t info[]=L"Runtime stopped at PC 00000000, instruction 00000000.";
            for(int i=0;i<8;i++){
                info[22+i]=L"0123456789ABCDEF"[stop_pc>>(28-4*i)&15];
                info[44+i]=L"0123456789ABCDEF"[stop_word>>(28-4*i)&15];
            }
            MessageBoxW(hwnd,info,TXT("Gauntlet Legends"),16);
        }
        phase="video output";D previous=picture_version;int64_t gfx_begin=stage_clock();D frame=cart_picture(picture);
        graphics_time+=stage_clock()-gfx_begin;observed_frame=frame;
        extern unsigned cart_bitmap_width(void),cart_bitmap_height(void);
        bitmap.w=cart_bitmap_width();bitmap.h=-(int)cart_bitmap_height();bitmap.imagesize=bitmap.w*(-bitmap.h)*4;
        static D last_log;
        if(frame && (!last_log || frame-last_log>=300)){
            char line[]="Game frame 00000000 (hex)\r\n";
            for(int i=0;i<8;i++)line[11+i]="0123456789ABCDEF"[(frame>>(28-i*4))&15];
            startup_note(line);game_state();last_log=frame;
        }
        log_perf_sample();
        if(picture_version!=previous){
            InvalidateRect(hwnd,0,0);
        }
        return 0;
    }
    if(msg==20)return 1;
    if(msg==15){
        int64_t paint_begin=stage_clock();phase="window paint";Paint paint;Rect r;H dc=BeginPaint(hwnd,&paint);
        GetClientRect(hwnd,&r);
        int w=r.r,maxh=r.b;
        if(prefs[8]){
            const ScreenSize *size=&screens[prefs[8]];
            if(w>size->width)w=size->width;
            if(maxh>size->height)maxh=size->height;
        }
        int source_width=cart_picture_width();
        int aspect_w=prefs[8]?screens[prefs[8]].width:r.r;
        int aspect_h=prefs[8]?screens[prefs[8]].height:r.b;
        // The software frame uses pairs of side pixels. Present at the exact
        // selected aspect once the corresponding frame has reached scanout.
        if(aspect_h<=0 || aspect_w*3<aspect_h*4 || aspect_w*3>aspect_h*8 ||
           (long long)aspect_w*312<(long long)(source_width-2)*aspect_h ||
           (long long)aspect_w*312>(long long)(source_width+2)*aspect_h){
            aspect_w=source_width;aspect_h=312;
        }
        int h=(w*aspect_h+aspect_w/2)/aspect_w;
        if(h>maxh){h=maxh;w=(h*aspect_w+aspect_h/2)/aspect_h;}
        int left=(r.r-w)/2,top=(r.b-h)/2;
        Rect bars[4]={{0,0,r.r,top},{0,top+h,r.r,r.b},{0,top,left,top+h},{left+w,top,r.r,top+h}};
        for(int i=0;i<4;i++)if(bars[i].r>bars[i].l && bars[i].b>bars[i].t)FillRect(dc,&bars[i],GetStockObject(4));
        if(bitmap.w>w || -bitmap.h>h)SetStretchBltMode(dc,4);
        StretchDIBits(dc,(r.r-w)/2,(r.b-h)/2,w,h,0,0,bitmap.w,-bitmap.h,picture,&bitmap,0,0xcc0020);
        if(prefs[15])draw_performance(dc,r);
        EndPaint(hwnd,&paint);
        paint_time+=stage_clock()-paint_begin;
        static D last_painted;if(last_painted!=picture_version){paint_count++;last_painted=picture_version;record_paint(stage_clock());}
        return 0;
    }
    if(msg==2){stop_motor();PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,key,param);
}
#include "launcher.inc"
#include "launcher_ui.inc"
extern int SetProcessDPIAware(void);
extern int raster_worker_start(void);
extern void raster_worker_stop(void);
extern unsigned GetEnvironmentVariableW(W,uint16_t*,unsigned);
void start(void){
    SetProcessDPIAware();
    open_log();
    extern int profile_runtime;
    profile_runtime=1;
    image_base=(uintptr_t)GetModuleHandleW(0);
    if(!AddVectoredExceptionHandler(1,capture_fault))startup_note("Windows exception logging unavailable\r\n");
    phase="launcher";
    read_controls();read_graphics();
    gpu_open();
    if(!run_launcher()){gpu_close();ExitProcess(0);return;}
    startup_note("ROM verified\r\n");
    if(!pak_open()){MessageBoxW(0,TXT("Could not open the Player 1 Controller Pak. Close other copies of the game and check that the extracted folder is writable and the .mpk is 32 KB."),TXT("Gauntlet playtest"),16);ExitProcess(7);return;}
    phase="cartridge startup";cart_start(rom);
    startup_note("Extra guest CPU on (default)\r\n");
    W name=TXT("GauntletNativeStartup");H instance=GetModuleHandleW(0);
    Class wc={0};wc.style=3;wc.proc=window_proc;wc.instance=instance;wc.name=name;
    wc.cursor=LoadCursorW(0,(W)(uintptr_t)32512);wc.brush=GetStockObject(4);
    H win=RegisterClassW(&wc)?CreateWindowExW(0,name,TXT("Towerbound | Static Recomp Playtest 17"),0xcf0000,
        (int)0x80000000,0,1056,812,0,0,instance,0):0;
    if(!win){startup_note("Window creation failed\r\n");MessageBoxW(0,TXT("Could not create the game window."),name,16);ExitProcess(5);return;}

    update_graphics();if(prefs[7])display_mode(win);else if(prefs[8])resize_window(win,prefs[8]);
    D fine_timer=timeBeginPeriod(1);
    /* Keep a fallback tick for nested Win32 move/menu loops. */
    SetTimer(win,1,16,0);
    startup_note("Game window ready\r\n");
    uint16_t mode[8]={0};
    GetEnvironmentVariableW(TXT("TOWERBOUND_RASTER_THREADS"),mode,8);
    int worker=mode[0]!='1' && raster_worker_start();
    startup_note(worker?"CPU renderer: two threads available\r\n":"CPU renderer: single thread\r\n");
    register_pads(win);ShowWindow(win,1);UpdateWindow(win);Msg m;
    extern int PeekMessageW(Msg*,H,D,D,D);
    extern D MsgWaitForMultipleObjectsEx(D,const H*,D,D,D);
    int quit=0;int64_t quantum=stage_frequency()*3/1000,last_tick=stage_clock()-quantum;
    while(!quit){
        for(int n=0;n<64 && PeekMessageW(&m,0,0,0,1);n++){
            if(m.hwnd==win && m.message==0x113 && m.wp==1)continue;
            if(m.message==0x12){quit=1;break;}
            if(m.message==0x100 && m.wp==0x70){if(!(m.lp&(1u<<30)))toggle_live(win);continue;}
            if(live_window && IsDialogMessageW(live_window,&m))continue;
            TranslateMessage(&m);DispatchMessageW(&m);
        }
        if(quit)break;
        int64_t now=stage_clock();
        if(now-last_tick>=quantum){
            last_tick=now;window_proc(win,0x113,1,0);
        }
        int64_t left=quantum-(stage_clock()-last_tick);
        D wait_ms=left>0?(D)((left*1000+stage_frequency()-1)/stage_frequency()):0;
        MsgWaitForMultipleObjectsEx(0,0,wait_ms,0x4ff,4);
    }
    for(int i=0;i<4;i++)if(fonts[i])DeleteObject(fonts[i]);
    if(!fine_timer)timeEndPeriod(1);
    pak_flush();if(pak_ready)CloseHandle(pak_lock);
    raster_worker_stop();gpu_close();audio_close();
    startup_note("Game window closed\r\n");
    if(log_file && log_file!=(H)(intptr_t)-1)CloseHandle(log_file);
    ExitProcess(0);
}
