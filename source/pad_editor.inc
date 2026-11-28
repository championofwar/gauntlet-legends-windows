enum { DEVICE=210,SOURCE,ACTION,SWAP,RUMBLE,STRENGTH,TEST,RESET,EDITOR_BACK };
typedef struct {D size,flags;} CommonClasses;
extern int InitCommonControlsEx(const CommonClasses*);
static H dead_slider;
static int axis_x,axis_y,raw_x,raw_y;
static int editing,selected,art_layout=-1;
static D held;static int connection;
static H editor_fields[EDITOR_BACK-DEVICE+1];
static const W models[]={TXT("Xbox"),TXT("PS4 - DualShock 4"),TXT("PS5 - DualSense")};
static const W ps_faces[]={TXT("Cross"),TXT("Circle"),TXT("Square"),TXT("Triangle"),TXT("L1"),TXT("R1"),TXT("L2"),TXT("R2"),TXT("Options")};
static D diagram[480*300];
typedef struct {short x,y,w,h;} PadRegion;
static const PadRegion regions[3][NS]={
#include "pad_regions.inc"
};
extern H GetDlgItem(H,int),SetFocus(H);
static int source_count(void){return layout==0?24:layout==1?26:27;}
static int real_source(int index){
    if(controls.swap && index>=16 && index<24)return index<20?index+4:index-4;
    return index;
}
static W input_name(int id){
    static const W directions[]={TXT("Left stick up"),TXT("Left stick down"),TXT("Left stick left"),TXT("Left stick right"),
        TXT("Right stick up"),TXT("Right stick down"),TXT("Right stick left"),TXT("Right stick right")};
    if(id>=16 && id<24)return directions[id<20?id-12:id-20];
    if(layout && id<=8)return ps_faces[id];
    if(layout && id==9)return layout==1?TXT("Share"):TXT("Create");
    if(layout && id==10)return TXT("L3");if(layout && id==11)return TXT("R3");
    return sources[id];
}
static H ef(int id){return editor_fields[id-DEVICE];}
static void set_combo(int id,const W *items,int count,int index){
    H c=ef(id);SendMessageW(c,0x14b,0,0);
    for(int i=0;i<count;i++)SendMessageW(c,0x143,0,(LP)items[i]);
    SendMessageW(c,0x14e,index,0);
}
static void select_input(int physical){
    selected=physical;
    SendMessageW(ef(SOURCE),0x14e,physical,0);
    int id=real_source(physical);
    set_combo(ACTION,actions,(id>=20 && id<=23)?NA:NA-1,controls.map[id]);
}
static void refresh_editor(void){
    W names[NS];for(int i=0;i<source_count();i++)names[i]=input_name(i);
    set_combo(SOURCE,names,source_count(),0);
    static const W sticks[]={TXT("Left stick"),TXT("Right stick")};
    set_combo(SWAP,sticks,2,controls.swap);set_combo(RUMBLE,modes,3,controls.mode);
    set_combo(STRENGTH,strengths,3,controls.strength-1);
    select_input(selected<source_count()?selected:0);
}
static void read_diagram(void){
    static const W files[]={TXT("Pad-0.bgr"),TXT("Pad-1.bgr"),TXT("Pad-2.bgr")};
    static uint16_t filename[32768];art_layout=-1;
    if(!sibling_path(filename,files[layout]))return;
    H f=CreateFileW(filename,0x80000000,1,0,3,0x80,0);
    if(f==(H)(intptr_t)-1)return;
    D got=0;if(ReadFile(f,diagram,sizeof diagram,&got,0) && got==sizeof diagram)art_layout=layout;
    CloseHandle(f);
}
static void main_controls(H win,int show){
    page_controls(win,show);
}
static void close_editor(H win){
    for(int i=DEVICE;i<=EDITOR_BACK;i++)if(ef(i)){DestroyWindow(ef(i));editor_fields[i-DEVICE]=0;}
    if(dead_slider){DestroyWindow(dead_slider);dead_slider=0;}
    editing=0;main_controls(win,1);SetFocus(GetDlgItem(win,CONTROLS));InvalidateRect(win,0,0);
}
static void open_editor(H win){
    main_controls(win,0);editing=1;selected=0;
    const int xyw[][3]={{20,50,224},{514,114,200},{514,181,200},{20,401,193},{226,401,257},{496,401,218}};
    for(int id=DEVICE;id<=STRENGTH;id++){
        const int *r=xyw[id-DEVICE];editor_fields[id-DEVICE]=child(win,TXT("COMBOBOX"),TXT(""),0x00200203,r[0],r[1],r[2],220,id);
    }
    editor_fields[TEST-DEVICE]=child(win,TXT("BUTTON"),TXT("Test vibration"),0,20,457,145,36,TEST);
    editor_fields[RESET-DEVICE]=child(win,TXT("BUTTON"),TXT("Restore defaults"),0,178,457,152,36,RESET);
    editor_fields[EDITOR_BACK-DEVICE]=child(win,TXT("BUTTON"),TXT("Back to Controls"),0,20,8,145,34,EDITOR_BACK);
    for(int id=DEVICE;id<=EDITOR_BACK;id++)if(!ef(id)){close_editor(win);MessageBoxW(win,TXT("Could not open the controller editor."),TXT("Controller"),48);return;}
    CommonClasses common={sizeof(common),4};
    if(InitCommonControlsEx(&common))dead_slider=child(win,TXT("msctls_trackbar32"),TXT("Movement deadzone"),1,22,547,304,35,230);
    if(!dead_slider){close_editor(win);MessageBoxW(win,TXT("Could not open the deadzone control."),TXT("Controller"),48);return;}
    SendMessageW(dead_slider,0x406,1,40<<16);SendMessageW(dead_slider,0x405,1,stick_deadzone);
    SendMessageW(dead_slider,0x414,0,5);
    set_combo(DEVICE,models,3,layout);refresh_editor();read_diagram();
    SetFocus(ef(DEVICE));InvalidateRect(win,0,0);
}
static void paint_editor(H win){
    Paint pt;H dc=BeginPaint(win,&pt);Rect client;GetClientRect(win,&client);fill(dc,client,0x141a20);
    label(dc,box(180,8,520,34),TXT("Controller settings"),1,0xffecc6,4);
    label(dc,box(258,46,452,28),TXT("Click a button to assign its game action."),0,0xc6ced2,4);
    if(art_layout==(int)layout){
        Bitmap b={40,480,-300,1,32,0,sizeof diagram};SetStretchBltMode(dc,3);
        StretchDIBits(dc,unit(14),unit(78),unit(480),unit(300),0,0,480,300,diagram,&b,0,0xcc0020);
        for(int i=0;i<source_count();i++)if(i==selected || (held&(1u<<i))){
            unsigned color=(held&(1u<<i))?0x82dcca:0xe5b974;
            PadRegion r=regions[layout][i];int x=14+r.x-2,y=78+r.y-2,w=r.w+4,h=r.h+4;
            fill(dc,box(x,y,w,2),color);fill(dc,box(x,y+h-2,w,2),color);
            fill(dc,box(x,y,2,h),color);fill(dc,box(x+w-2,y,2,h),color);
        }
    }else label(dc,box(24,130,440,70),TXT("Diagram unavailable. Use the Input list."),0,0xffecc6,4);
    label(dc,box(514,89,205,21),TXT("Controller input"),0,0xc6ced2,4);
    label(dc,box(514,156,205,22),TXT("Game action"),0,0xc6ced2,4);
    label(dc,box(20,374,195,22),TXT("Movement stick"),0,0xc6ced2,4);
    label(dc,box(226,374,250,22),TXT("Vibration mode"),0,0xc6ced2,4);
    label(dc,box(496,374,210,22),TXT("Vibration strength"),0,0xc6ced2,4);
    label(dc,box(514,236,208,30),connection==2?TXT("PlayStation HID connected"):connection==1?TXT("XInput connected"):TXT("Controller offline"),0,connection?0x82dcca:0xa6b0b6,4);
    uint16_t title[64];uint16_t *p=append_text(title,TXT("Movement deadzone   "));append_text(append_number(p,stick_deadzone),TXT("%"));
    label(dc,box(20,513,324,28),title,1,0xffecc6,4);
    label(dc,box(355,516,178,30),axis_x||axis_y?TXT("Movement active"):TXT("Stick centred"),0,axis_x||axis_y?0x82dcca:0xa6b0b6,4);
    fill(dc,box(576,510,100,100),0x273339);int range=(int)stick_deadzone/2;
    fill(dc,box(626-range,560-range,2*range+1,2*range+1),0x41505a);
    fill(dc,box(626+raw_x/670-2,560-raw_y/670-2,5,5),0xffdaa0);
    EndPaint(win,&pt);
}
static void deadzone_scroll(H win,WP key){
    (void)key;LP v=SendMessageW(dead_slider,0x400,0,0);
    if(v<0 || v>40 || (unsigned)v==stick_deadzone)return;
    stick_deadzone=prefs[18]=(unsigned char)v;save_graphics();InvalidateRect(win,0,0);
}
static void diagram_click(H win,LP position){
    int x=(int16_t)position,y=(int16_t)(position>>16);
    for(int i=0;i<source_count();i++){
        PadRegion r=regions[layout][i];Rect b=box(14+r.x,78+r.y,r.w,r.h);
        if(x<b.l||x>=b.r||y<b.t||y>=b.b)continue;
        select_input(i);InvalidateRect(win,0,0);SetFocus(ef(ACTION));SendMessageW(ef(ACTION),0x14f,1,0);return;
    }
}
static void editor_command(H win,int id,int note){
    if(note==1 && id>=DEVICE && id<=STRENGTH){
        LP index=SendMessageW(ef(id),0x147,0,0);if(index<0)return;
        if(id==DEVICE){
            if(index>2)return;stop_motor();layout=(unsigned)index;sony_device=ignored_device=0;sony_extra=0;selected=0;
            refresh_editor();read_diagram();
        }else if(id==SOURCE){if(index<source_count())select_input(index);}
        else if(id==ACTION)controls.map[real_source(selected)]=(unsigned char)index;
        else if(id==SWAP){controls.swap=(unsigned char)index;select_input(selected);}
        else if(id==RUMBLE)controls.mode=(unsigned char)index;
        else controls.strength=(unsigned char)(index+1);
        if(id!=SOURCE){stop_motor();pak_select_motor(controls.mode==2);save_controls(win);}
    }else if(note==0){
        if(id==EDITOR_BACK || id==2 || id==1){close_editor(win);return;}
        if(id==RESET){stick_deadzone=prefs[18]=15;SendMessageW(dead_slider,0x405,1,15);save_graphics();controls=defaults;stop_motor();pak_select_motor(controls.mode==2);refresh_editor();save_controls(win);}
        if(id==TEST){
            if(sony_live() || slot<0)MessageBoxW(win,TXT("Vibration requires a controller connected through XInput."),TXT("Test vibration"),64);
            else pulse(320);
        }
    }
    InvalidateRect(win,0,0);
}

static void editor_feedback(H win){
    PadState s={0};D before=held;int old=connection;
    if(sony_live()){s=sony_state;ps_extras=sony_extra;connection=2;}
    else{ps_extras=0;connection=slot>=0 && !XInputGetState(slot,&s);}
    held=connection?pad_inputs(&s):0;
    int x=controls.swap?s.rx:s.lx,y=controls.swap?s.ry:s.ly;
    int moved=x!=raw_x || y!=raw_y || axis_x!=stick_axis(x) || axis_y!=stick_axis(y);
    raw_x=x;raw_y=y;axis_x=stick_axis(x);axis_y=stick_axis(y);
    if(moved || before!=held || old!=connection)InvalidateRect(win,0,0);
}
