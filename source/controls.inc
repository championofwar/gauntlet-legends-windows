/* XInput source order is also the on-disk mapping order. */
static const W sources[]={
    TXT("A"),TXT("B"),TXT("X"),TXT("Y"),TXT("Left bumper"),TXT("Right bumper"),
    TXT("Left trigger"),TXT("Right trigger"),TXT("Menu / Start"),TXT("View / Back"),
    TXT("Left stick click"),TXT("Right stick click"),TXT("D-pad up"),TXT("D-pad down"),
    TXT("D-pad left"),TXT("D-pad right"),TXT("Other stick up"),TXT("Other stick down"),
    TXT("Other stick left"),TXT("Other stick right"),TXT("Movement stick up"),TXT("Movement stick down"),TXT("Movement stick left"),TXT("Movement stick right"),TXT("PS button"),TXT("Touchpad click"),TXT("Mute button")
};
static const W actions[]={TXT("Unassigned"),TXT("N64 A"),TXT("N64 B"),TXT("Z"),TXT("Start"),
    TXT("L"),TXT("R"),TXT("C-up"),TXT("C-down"),TXT("C-left"),TXT("C-right"),
    TXT("D-up"),TXT("D-down"),TXT("D-left"),TXT("D-right"),TXT("D-up + move up"),
    TXT("D-down + move down"),TXT("D-left + move left"),TXT("D-right + move right"),TXT("Analogue movement")};
enum { NS=sizeof(sources)/sizeof(sources[0]), NA=sizeof(actions)/sizeof(actions[0]) };
static const unsigned masks[]={0,32768,16384,8192,4096,32,16,8,4,2,1,2048,1024,512,256,2048,1024,512,256,0};
typedef struct {unsigned char tag[4],map[NS],swap,mode,strength;} ControlSettings;
static const ControlSettings defaults={{'P','A','D',2},{1,2,9,7,5,6,3,3,4,0,0,0,15,16,17,18,7,8,9,10,19,19,19,19,0,0,0},0,1,2};
static unsigned layout;
static struct {unsigned char tag[4],layout;ControlSettings profile[3];} profiles;
#define controls (profiles.profile[layout])
static unsigned ps_extras;
static uint16_t control_path[32768],control_temp[32768];
static const W modes[]={TXT("Off"),TXT("Button feedback (save pak)"),TXT("Game Rumble Pak")};
static const W strengths[]={TXT("Low"),TXT("Medium"),TXT("High")};
static const uint16_t speeds[]={12000,25000,42000};
static unsigned previous_actions,motor_until;
static int pulse_active,motor_pad=-1;
static uint16_t last_speed;
extern int pak_motor;
extern void pak_select_motor(int);
extern D GetTickCount(void);
extern D XInputSetState(D,const uint16_t*);
static void apply_motor(int pad,uint16_t speed){
    uint16_t motors[2]={0,0};
    if(motor_pad>=0 && (motor_pad!=pad || !speed)){
        XInputSetState(motor_pad,motors);motor_pad=-1;last_speed=0;
    }
    if(speed && pad>=0 && (motor_pad!=pad || last_speed!=speed)){
        motors[0]=speed;motors[1]=speed/2;
        if(!XInputSetState(pad,motors)){motor_pad=pad;last_speed=speed;}
    }
}
static void stop_motor(void){pulse_active=0;apply_motor(-1,0);}
static void pulse(unsigned ms){motor_until=GetTickCount()+ms;pulse_active=1;}
void read_controls(void){
    keyboard_read();
    for(int i=0;i<3;i++)profiles.profile[i]=defaults;
    profiles.tag[0]='P';profiles.tag[1]='D';profiles.tag[2]='B';profiles.tag[3]=2;
    if(!sibling_path(control_path,TXT("Gauntlet-Controls.cfg")) ||
       !sibling_path(control_temp,TXT("Gauntlet-Controls.tmp")))return;
    H f=CreateFileW(control_path,0x80000000,1,0,3,0x80,0);
    if(f==(H)(intptr_t)-1)return;
    unsigned char data[sizeof profiles];D got=0;
    int ok=ReadFile(f,data,sizeof data,&got,0);CloseHandle(f);
    if(!ok)return;
    if(got==27 && data[0]=='P' && data[1]=='A' && data[2]=='D' && data[3]==1){
        if(data[24]>1 || data[25]>2 || data[26]<1 || data[26]>3)return;
        for(int i=0;i<20;i++)if(data[4+i]>=19)return;
        for(int i=0;i<3;i++){
            ControlSettings *c=&profiles.profile[i];
            __builtin_memcpy(c->map,data+4,20);
            c->swap=data[24];c->mode=data[25];c->strength=data[26];
        }
    }else if(got==sizeof profiles && !__builtin_memcmp(data,profiles.tag,4) && data[4]<3){
        for(int i=0;i<3;i++){
            ControlSettings c;__builtin_memcpy(&c,data+5+i*sizeof c,sizeof c);
            if(__builtin_memcmp(c.tag,defaults.tag,4)||c.swap>1||c.mode>2||!c.strength||c.strength>3)return;
            for(int j=0;j<NS;j++)if(c.map[j]>=NA || (c.map[j]==19 && (j<20 || j>23)))return;
        }
        __builtin_memcpy(&profiles,data,sizeof profiles);layout=profiles.layout;
    }
    pak_select_motor(controls.mode==2);
}
static void save_controls(H win){
    profiles.layout=(unsigned char)layout;
    H f=CreateFileW(control_temp,0x40000000,0,0,2,0x80,0);
    int ok=0;D wrote=0;
    if(f!=(H)(intptr_t)-1){
        ok=WriteFile(f,&profiles,sizeof profiles,&wrote,0) && wrote==sizeof profiles && FlushFileBuffers(f);
        CloseHandle(f);
        if(ok)ok=MoveFileExW(control_temp,control_path,9);
    }
    if(!ok)MessageBoxW(win,TXT("The mapping works for this session. Windows could not save Gauntlet-Controls.cfg."),TXT("Controller settings"),48);
}
extern H CreatePopupMenu(void);
extern int AppendMenuW(H,D,WP,W),DestroyMenu(H),ClientToScreen(H,int*);
extern int TrackPopupMenuEx(H,D,int,int,H,void*);
static D pad_inputs(const PadState *s){
    static const uint16_t bits[]={4096,8192,16384,32768,256,512,0,0,16,32,64,128,1,2,4,8};
    D active=ps_extras;
    for(int i=0;i<16;i++)if(s->buttons&bits[i])active|=1u<<i;
    if(s->lt>50)active|=1u<<6;
    if(s->rt>50)active|=1u<<7;
    int16_t xx[]={s->rx,s->lx},yy[]={s->ry,s->ly};
    for(int j=0;j<2;j++){
        unsigned shift=16+4*j;
        if(yy[j]>14000)active|=1u<<shift;
        if(yy[j]<-14000)active|=2u<<shift;
        if(xx[j]<-14000)active|=4u<<shift;
        if(xx[j]>14000)active|=8u<<shift;
    }
    return active;
}
/* A value of 15 preserves the old 12-unit threshold exactly. */
static unsigned stick_deadzone=15;
static int stick_axis(int raw){
    int value=raw/400,threshold=(int)(stick_deadzone*80+50)/100;
    return value>-threshold && value<threshold?0:value;
}
static D mapped_pad(const PadState *s,int *x,int *y){
    D active=pad_inputs(s),b=0;
    if(controls.swap){
        D right=(active>>16)&15,left=(active>>20)&15;
        active=(active&~0xff0000u)|(right<<20)|(left<<16);
    }
    *x=stick_axis(controls.swap?s->rx:s->lx);*y=stick_axis(controls.swap?s->ry:s->ly);
    if(controls.map[*y>=0?20:21]!=19)*y=0;
    if(controls.map[*x<0?22:23]!=19)*x=0;
    for(int i=0;i<NS;i++)if(active&(1u<<i)){
        unsigned a=controls.map[i];b|=masks[a];
        if(a==15)*y=80;if(a==16)*y=-80;if(a==17)*x=-80;if(a==18)*x=80;
    }
    return b;
}
