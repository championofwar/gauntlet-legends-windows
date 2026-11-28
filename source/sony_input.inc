// Sony USB and full Bluetooth HID input. XInput remains the fallback for adapters.
typedef struct {uint16_t page,usage;D flags;H target;} RawRegistration;
typedef struct {D type,size;H device;WP parameter;} RawHeader;
typedef struct {D size,type,vid,pid,version;uint16_t page,usage;D reserved[2];} DeviceInfo;
_Static_assert(sizeof(DeviceInfo)==32,"Win32 device info");
extern int RegisterRawInputDevices(const RawRegistration*,D,D);
extern D GetRawInputData(H,D,void*,D*,D),GetRawInputDeviceInfoW(H,D,void*,D*);
static PadState sony_state;
static unsigned sony_extra,sony_layout;
static H sony_device,ignored_device;
static int sony_live(void){return sony_device && sony_layout==layout;}
static void register_pads(H win){
    sony_device=ignored_device=0;sony_extra=0;
    RawRegistration devices[]={{1,5,0x2000,win},{1,4,0x2000,win}};
    RegisterRawInputDevices(devices,2,sizeof devices[0]);
}
static unsigned sony_model(H dev){
    DeviceInfo i={.size=sizeof i};D bytes=sizeof i;
    if(GetRawInputDeviceInfoW(dev,0x2000000b,&i,&bytes)==(D)-1 || i.type!=2 || i.vid!=0x054c)return 0;
    if(i.pid==0x05c4 || i.pid==0x09cc)return 1;
    return i.pid==0x0ce6?2:0;
}
static int axis_byte(unsigned v){return v<128?((int)v-128)*256:((int)v-128)*32767/127;}
static int decode_sony(unsigned model,const unsigned char *r,unsigned len,PadState *s,unsigned *extra){
    if(!len)return 0;
    unsigned start=1;
    if(model==1 && r[0]==0x11)start=3;
    else if(model==2 && r[0]==0x31)start=2;
    else if(r[0]!=1)return 0;
    int extended=model==2 && (r[0]==0x31 || len>=64);
    if(len<start+(extended?10:9))return 0;
    const unsigned char *p=r+start,*button=p+(extended?7:4);
    int lx=axis_byte(p[0]),ly=axis_byte(p[1]),rx=axis_byte(p[2]),ry=axis_byte(p[3]);
    s->lx=lx;s->rx=rx;s->ly=ly==-32768?32767:-ly;s->ry=ry==-32768?32767:-ry;
    s->lt=p[extended?4:7];s->rt=p[extended?5:8];
    static const unsigned hat[]={1,9,8,10,2,6,4,5};
    unsigned dir=button[0]&15,b=button[1],a=button[0];
    s->buttons=(dir<8?hat[dir]:0)|((a&16)?16384:0)|((a&32)?4096:0)|((a&64)?8192:0)|((a&128)?32768:0)|
        ((b&1)?256:0)|((b&2)?512:0)|((b&16)?32:0)|((b&32)?16:0)|((b&64)?64:0)|((b&128)?128:0);
    if((b&4) && !s->lt)s->lt=255;
    if((b&8) && !s->rt)s->rt=255;
    *extra=(unsigned)(button[2]&(extended?7:3))<<24;
    return 1;
}
static void raw_pad_event(D msg,WP key,LP param){
    if(msg==0xfe){
        if(key==2 && (H)param==sony_device){sony_device=0;sony_extra=0;}
        if((H)param==ignored_device)ignored_device=0;
        return;
    }
    if(msg!=0xff || !layout)return;
    union {RawHeader h;unsigned char data[8192];} packet;
    D bytes=sizeof packet;
    if(GetRawInputData((H)param,0x10000003,&packet,&bytes,sizeof(RawHeader))==(D)-1 || bytes<sizeof(RawHeader)+8 || packet.h.type!=2)return;
    H dev=packet.h.device;
    if(dev==ignored_device || (sony_live() && dev!=sony_device))return;
    unsigned model=dev==sony_device?sony_layout:sony_model(dev);
    if(model!=layout){ignored_device=dev;return;}
    D length,count;unsigned char *p=packet.data+sizeof(RawHeader);
    __builtin_memcpy(&length,p,4);__builtin_memcpy(&count,p+4,4);
    if(!count || !length || count>(bytes-sizeof(RawHeader)-8)/length)return;
    if(decode_sony(model,p+8+(count-1)*length,length,&sony_state,&sony_extra)){
        sony_device=dev;sony_layout=model;
    }
}
