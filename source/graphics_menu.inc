extern int gpu_ready(void);
extern unsigned cart_fast_scale(void),cart_picture_scale(void);
extern void cart_visuals(unsigned,unsigned,unsigned);
extern int GetWindowRect(H,Rect*),SetWindowPos(H,H,int,int,int,int,D);
extern LP SetWindowLongPtrW(H,int,LP);
extern H MonitorFromWindow(H,D);
typedef struct {D size;Rect monitor,work;D flags;} MonitorInfo;
extern int GetMonitorInfoW(H,MonitorInfo*);
typedef struct {int width,height;W label;} ScreenSize;
static const ScreenSize screens[]={
    {0,0,TXT("Auto")},
    {640,480,TXT("640 x 480")},
    {800,600,TXT("800 x 600")},
    {1024,768,TXT("1024 x 768")},
    {1280,720,TXT("1280 x 720")},
    {1280,800,TXT("1280 x 800")},
    {1280,960,TXT("1280 x 960")},
    {1280,1024,TXT("1280 x 1024")},
    {1366,768,TXT("1366 x 768")},
    {1440,900,TXT("1440 x 900")},
    {1600,900,TXT("1600 x 900")},
    {1600,1200,TXT("1600 x 1200")},
    {1920,1080,TXT("1920 x 1080")},
    {1920,1200,TXT("1920 x 1200")},
    {2560,1080,TXT("2560 x 1080")},
    {2560,1440,TXT("2560 x 1440")},
    {2560,1600,TXT("2560 x 1600")},
    {3440,1440,TXT("3440 x 1440")},
    {3840,2160,TXT("3840 x 2160")}
};
enum { SCREEN_COUNT=sizeof(screens)/sizeof(screens[0]) };
extern int AdjustWindowRect(Rect*,D,int);
static void resize_window(H win,unsigned index){
    MonitorInfo mi={.size=sizeof(mi)};
    if(!GetMonitorInfoW(MonitorFromWindow(win,2),&mi))return;
    Rect frame={0,0,0,0};
    if(!AdjustWindowRect(&frame,0xcf0000,0))return;
    int dw=frame.r-frame.l,dh=frame.b-frame.t;
    int maxw=mi.work.r-mi.work.l-dw,maxh=mi.work.b-mi.work.t-dh;
    if(maxw<=0 || maxh<=0)return;
    int w=index?screens[index].width:1056-dw,h=index?screens[index].height:812-dh;
    if(w>maxw){h=h*maxw/w;w=maxw;}
    if(h>maxh){w=w*maxh/h;h=maxh;}
    SetWindowPos(win,0,mi.work.l+(maxw-w)/2,mi.work.t+(maxh-h)/2,w+dw,h+dh,0x34);
}
static Rect windowed_rect;

static unsigned char prefs[19]={'G','F','X',6,1,0,0,0,0,0,0,2,1,2,2,0,4,4,15};
static uint16_t config_path[32768];
static void read_graphics(void){
    if(!sibling_path(config_path,TXT("Gauntlet-Graphics.cfg")))return;
    H f=CreateFileW(config_path,0x80000000,1,0,3,0x80,0);
    if(f==(H)(intptr_t)-1)return;
    unsigned char data[20]={0};D got=0;
    int ok=ReadFile(f,data,sizeof data,&got,0);
    CloseHandle(f);
    if(!ok || data[0]!='G'||data[1]!='F'||data[2]!='X'||
       !((got==8 && data[3]==1) || (got==9 && data[3]==2) ||
         (got==10 && (data[3]==3 || data[3]==4)) || (got==15 && data[3]==5) || (got==19 && data[3]==6)) ||
       data[4]<1||data[4]>3||data[5]>1||data[6]>1||data[7]>1||data[8]>=SCREEN_COUNT)return;
    for(int i=4;i<9;i++)prefs[i]=data[i];
    static const unsigned char limits[]={4,3,2,2,4,4,1,4,4,40};
    if(got>=15)for(unsigned i=9;i<got;i++)if(data[i]<=limits[i-9])prefs[i]=data[i];
    stick_deadzone=prefs[18];
}
static void save_graphics(void){
    if(!config_path[0])return;
    H f=CreateFileW(config_path,0x40000000,0,0,2,0x80,0);
    D written=0;
    if(f==(H)(intptr_t)-1){startup_note("Graphics settings save failed\r\n");return;}
    if(!WriteFile(f,prefs,sizeof prefs,&written,0)||written!=sizeof prefs)startup_note("Graphics settings save failed\r\n");
    CloseHandle(f);
}
static void display_mode(H win){
    Rect r=windowed_rect;
    if(prefs[7]){
        MonitorInfo mi={.size=sizeof(mi)};
        if(!GetMonitorInfoW(MonitorFromWindow(win,2),&mi) || !GetWindowRect(win,&windowed_rect)){
            prefs[7]=0;return;
        }
        r=mi.monitor;
    }
    SetWindowLongPtrW(win,-16,prefs[7]?0x90000000u:0x10cf0000u);
    SetWindowPos(win,0,r.l,r.t,r.r-r.l,r.b-r.t,0x34);
    if(!prefs[7] && prefs[8])resize_window(win,prefs[8]);
}
static void update_graphics(void){
    extern void cart_modern(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
    static const unsigned heights[]={0,720,1080,1440,2160},af[]={0,4,8,16},hud[]={75,85,100};
    cart_modern(heights[prefs[9]],af[prefs[10]],hud[prefs[11]],prefs[12],80+10*prefs[13],80+10*prefs[14]);
    extern void cart_audio_levels(unsigned,unsigned);
    cart_audio_levels(25*prefs[16],25*prefs[17]);
    cart_visuals(prefs[4],prefs[6],prefs[5]);

}
