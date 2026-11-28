extern H CreateSolidBrush(D),SelectObject(H,H);
extern int DeleteObject(H),SetStretchBltMode(H,int),DrawFocusRect(H,Rect*);
extern H CreateFontW(int,int,int,int,int,D,D,D,D,D,D,D,D,W);
extern LP SendMessageW(H,D,WP,LP);
extern int IsDialogMessageW(H,Msg*),GetSystemMetrics(int),KillTimer(H,WP);
enum { UI_W=736,UI_H=656,HERO_W=736,HERO_H=178,PLAY=101,BROWSE,CONTROLS,BACKUP,RESTORE,SETTING=110 };
typedef struct {D type,id,item,action,state;H window,dc;Rect bounds;WP data;} DrawItem;
static D hero_pixels[HERO_W*HERO_H];
static int hero_ready,ui_scale=1000,ui_done,ui_play;
static H fonts[4],fields[14];
static int unit(int value){return value*ui_scale/1000;}
static Rect box(int x,int y,int w,int h){return (Rect){unit(x),unit(y),unit(x+w),unit(y+h)};}
static D rgb(unsigned v){return ((v&255)<<16)|(v&0xff00)|(v>>16);}
static void fill(H dc,Rect r,unsigned color){
    H brush=CreateSolidBrush(rgb(color));
    if(brush){FillRect(dc,&r,brush);DeleteObject(brush);}
}
static void label(H dc,Rect r,W text,int font,unsigned color,D flags){
    H old=SelectObject(dc,fonts[font]);SetBkMode(dc,1);SetTextColor(dc,rgb(color));
    DrawTextW(dc,text,-1,&r,flags|0x820);SelectObject(dc,old);
}
static void read_art(void){
    static uint16_t file[32768];if(!sibling_path(file,TXT("Launcher-art.bgr")))return;
    H f=CreateFileW(file,0x80000000,1,0,3,0x80,0);if(f==(H)(intptr_t)-1)return;
    D got=0;hero_ready=ReadFile(f,hero_pixels,sizeof hero_pixels,&got,0)&&got==sizeof hero_pixels;CloseHandle(f);
}
static const W labels[]={TXT("Frame scale (original)"),TXT("Anti-aliasing"),TXT("Upscale filter (original)"),TXT("Display mode"),TXT("Window resolution"),TXT("World resolution"),TXT("Anisotropic filtering (HD)"),TXT("HUD size"),TXT("HUD position"),TXT("Brightness"),TXT("Gamma"),TXT("Performance overlay"),TXT("Music volume"),TXT("Effects volume")};
static const W options[14][5]={
{TXT("Original"),TXT("2x"),TXT("3x")},
{TXT("Off"),TXT("FXAA")},
{TXT("Original"),TXT("Bilinear")},
{TXT("Windowed"),TXT("Borderless fullscreen")},
{TXT("Auto")},
{TXT("Original"),TXT("720p"),TXT("1080p"),TXT("1440p"),TXT("2160p")},
{TXT("Off"),TXT("4x"),TXT("8x"),TXT("16x")},
{TXT("75%"),TXT("85%"),TXT("100%")},
{TXT("Left"),TXT("Centre"),TXT("Right")},
{TXT("80%"),TXT("90%"),TXT("100%"),TXT("110%"),TXT("120%")},
{TXT("0.8"),TXT("0.9"),TXT("1.0"),TXT("1.1"),TXT("1.2")},{TXT("Off"),TXT("On")},{TXT("Mute"),TXT("25%"),TXT("50%"),TXT("75%"),TXT("100%")},{TXT("Mute"),TXT("25%"),TXT("50%"),TXT("75%"),TXT("100%")}};
static const int counts[]={3,2,2,2,SCREEN_COUNT,5,4,3,3,5,5,2,5,5};
static const int field_x[]={26,255,484,26,255,484,26,255,484,26,255,484,26,255},field_y[]={239,239,239,293,293,293,347,347,347,401,401,401,455,455},field_w[]={214,214,226,214,214,226,214,214,226,214,214,226,214,214};
static H child(H win,W type,W text,D style,int x,int y,int w,int h,int id){
    H c=CreateWindowExW(0,type,text,style|0x50010000,unit(x),unit(y),unit(w),unit(h),win,(H)(uintptr_t)id,GetModuleHandleW(0),0);
    if(c)SendMessageW(c,0x30,(WP)fonts[0],1);
    return c;
}
enum { PAGE_GAME,PAGE_GRAPHICS,PAGE_CONTROLS,PAGE_AUDIO,PAGE_COUNT };
enum { TAB_FIRST=250,KEY_RESET=260,KEY_FIRST=300 };
static const W page_names[]={TXT("Game"),TXT("Graphics"),TXT("Controls"),TXT("Audio")};
static int launcher_page;
static unsigned char key_down[KEY_COUNT];
extern H GetDlgItem(H,int);
static int field_top(int i,int live){return live?field_y[i]:i>=12?286:field_y[i]+36;}
static int on_page(int id){
    if(id==PLAY || (id>=TAB_FIRST && id<TAB_FIRST+PAGE_COUNT))return 1;
    if(id==CONTROLS || id==KEY_RESET || (id>=KEY_FIRST && id<KEY_FIRST+KEY_COUNT))return launcher_page==PAGE_CONTROLS;
    if(id>=SETTING && id<SETTING+14)return launcher_page==(id<SETTING+12?PAGE_GRAPHICS:PAGE_AUDIO);
    return launcher_page==PAGE_GAME;
}
static void page_controls(H win,int visible){
    for(int id=PLAY;id<=RESTORE;id++)ShowWindow(GetDlgItem(win,id),visible && on_page(id));
    for(int i=0;i<14;i++)ShowWindow(fields[i],visible && on_page(SETTING+i));
    for(int i=0;i<PAGE_COUNT;i++){
        H tab=GetDlgItem(win,TAB_FIRST+i);
        ShowWindow(tab,visible);InvalidateRect(tab,0,0);
    }
    for(unsigned i=0;i<KEY_COUNT;i++)ShowWindow(GetDlgItem(win,KEY_FIRST+i),visible && on_page(KEY_FIRST+i));
    ShowWindow(GetDlgItem(win,KEY_RESET),visible && on_page(KEY_RESET));
    InvalidateRect(win,0,0);
}
static const W movement_names[]={TXT("Move up"),TXT("Move down"),TXT("Move left"),TXT("Move right")};
static const W short_keys[]={TXT(""),TXT("A"),TXT("B"),TXT("Z"),TXT("Start"),TXT("L"),TXT("R"),TXT("C ↑"),TXT("C ↓"),TXT("C ←"),TXT("C →"),TXT("D ↑"),TXT("D ↓"),TXT("D ←"),TXT("D →"),TXT("D+ ↑"),TXT("D+ ↓"),TXT("D+ ←"),TXT("D+ →"),TXT("↑"),TXT("↓"),TXT("←"),TXT("→")};
static void assign_key(H win,unsigned index){
    H menu=CreatePopupMenu();if(!menu)return;
    for(unsigned a=0;a<KEY_ACTION_COUNT;a++)
        AppendMenuW(menu,keyboard_map[index]==a?8:0,a+1,a<19?actions[a]:movement_names[a-19]);
    H button=GetDlgItem(win,KEY_FIRST+index);
    Rect bounds;GetWindowRect(button,&bounds);
    int chosen=TrackPopupMenuEx(menu,0x100|2,bounds.l,bounds.b,win,0);
    DestroyMenu(menu);
    if(chosen>0 && chosen<=KEY_ACTION_COUNT){
        keyboard_map[index]=chosen-1;keyboard_save(win);InvalidateRect(button,0,0);
    }
}
static int make_fields(H win,int count_fields){
    for(int i=0;i<count_fields;i++){
        H c=fields[i]=child(win,TXT("COMBOBOX"),labels[i],0x00200203,field_x[i],field_top(i,count_fields==12),field_w[i],220,SETTING+i);
        if(!c)return 0;
        int count=counts[i];
        for(int j=0;j<count;j++)if(SendMessageW(c,0x143,0,(LP)(i==4?screens[j].label:options[i][j]))<0)return 0;
        SendMessageW(c,0x14e,prefs[4+i]-(i==0),0);
    }
    return 1;
}
static int make_widgets(H win){
    for(int i=0;i<PAGE_COUNT;i++)
        if(!child(win,TXT("BUTTON"),page_names[i],11,26+172*i,188,167,36,TAB_FIRST+i))return 0;
    for(unsigned i=0;i<KEY_COUNT;i++){
        const KeyboardKey *k=&keyboard_keys[i];
        if(!child(win,TXT("BUTTON"),k->name,11,k->x,k->y,k->w,39,KEY_FIRST+i))return 0;
    }
    if(!make_fields(win,14) ||
       !child(win,TXT("BUTTON"),TXT("Back up Pak"),0,26,382,135,34,BACKUP) ||
       !child(win,TXT("BUTTON"),TXT("Restore Pak"),0,172,382,135,34,RESTORE) ||
       !child(win,TXT("BUTTON"),TXT("Choose ROM"),11,570,265,140,36,BROWSE) ||
       !child(win,TXT("BUTTON"),TXT("Controller settings"),11,491,237,219,36,CONTROLS) ||
       !child(win,TXT("BUTTON"),TXT("Reset keyboard"),0,550,530,151,32,KEY_RESET) ||
       !child(win,TXT("BUTTON"),TXT("Play game"),11,514,594,196,46,PLAY))return 0;
    page_controls(win,1);return 1;
}
extern int SaveDC(H),RestoreDC(H,int),IntersectClipRect(H,int,int,int,int),SetTextCharacterExtra(H,int);
static void title_word(H dc,W word,int font,int x,int y,int w,int h,int spacing,const unsigned *colours){
    Rect r=box(x,y,w,h);
    int old=SetTextCharacterExtra(dc,unit(spacing));
    for(int dy=-1;dy<=3;dy++)for(int dx=-1;dx<=1;dx++){
        Rect edge={r.l+unit(dx),r.t+unit(dy),r.r+unit(dx),r.b+unit(dy)};
        label(dc,edge,word,font,0x21100e,5);
    }
    label(dc,r,word,font,colours[3],5);
    for(int i=0;i<4;i++){
        int saved=SaveDC(dc);if(!saved)break;
        IntersectClipRect(dc,r.l,r.t+(r.b-r.t)*i/4,r.r,r.t+(r.b-r.t)*(i+1)/4);
        label(dc,r,word,font,colours[i],5);RestoreDC(dc,saved);
    }
    SetTextCharacterExtra(dc,old);
}
static void paint_title(H dc){
    static const unsigned gold[]={0xffedaf,0xffde81,0xe4a044,0xb8753a};
    static const unsigned ruby[]={0xffc59a,0xff8863,0xdb503e,0xa23331};
    title_word(dc,TXT("GAUNTLET"),2,15,22,416,86,0,gold);
    fill(dc,box(49,121,49,1),0xb38248);fill(dc,box(346,121,48,1),0xb38248);
    title_word(dc,TXT("LEGENDS"),3,95,99,255,45,5,ruby);
}
static void paint_launcher(H win,int live){
    Paint pt;H dc=BeginPaint(win,&pt);Rect client;GetClientRect(win,&client);
    fill(dc,client,0x141a20);fill(dc,box(0,0,UI_W,HERO_H),0x202f38);
    if(hero_ready){
        Bitmap b={40,HERO_W,-HERO_H,1,32,0,sizeof hero_pixels};
        SetStretchBltMode(dc,3);
        StretchDIBits(dc,0,0,unit(UI_W),unit(HERO_H),0,0,HERO_W,HERO_H,hero_pixels,&b,0,0xcc0020);
    }
    paint_title(dc);
    fill(dc,box(0,177,UI_W,1),0x896f48);
    if(live)label(dc,box(26,185,500,28),TXT("Live graphics settings"),1,0xf1e8da,4);
    for(int i=0;i<(live?12:14);i++)if(live || on_page(SETTING+i))
        label(dc,box(field_x[i],field_top(i,live)-24,field_w[i],23),labels[i],0,0xc6ced2,4);
    if(live){EndPaint(win,&pt);return;}
    if(launcher_page==PAGE_GAME){
        label(dc,box(26,235,520,27),TXT("Game ROM"),1,0xf1e8da,4);
        label(dc,box(26,267,528,32),path[0]?path:TXT("Choose your USA .z64 file"),0,0xc6ced2,4|0x4000);
        fill(dc,box(26,324,684,1),0x384249);
        label(dc,box(26,343,500,28),TXT("Controller Pak"),1,0xf1e8da,4);
    }else if(launcher_page==PAGE_CONTROLS){
        label(dc,box(26,236,400,32),TXT("Keyboard mapping"),1,0xf1e8da,4);
        label(dc,box(26,271,680,25),TXT("Select a key to assign its N64 action."),0,0xc6ced2,4);
        fill(dc,box(25,298,686,224),0x806d50);
        fill(dc,box(27,300,682,220),0x1a2229);
    }
    fill(dc,box(0,577,736,82),0x0e1419);
    label(dc,box(251,598,244,36),prefs[9]?(gpu_ready()?TXT("Vulkan HD world"):TXT("CPU fallback")):prefs[4]>1 || prefs[5]?(gpu_ready()?TXT("Vulkan frame scaling"):TXT("CPU rendering")):TXT("Original rendering"),0,0xa6b0b6,4);
    EndPaint(win,&pt);
}
static void draw_button(DrawItem *item){
    Rect r=item->bounds;int play=item->id==PLAY,down=item->state&1;
    if(item->id>=KEY_FIRST && item->id<KEY_FIRST+KEY_COUNT){
        unsigned index=item->id-KEY_FIRST,action=keyboard_map[index];
        down=down || key_down[index];
        fill(item->dc,r,down?0xffd184:action?0x968163:0x465059);
        r.l++;r.t++;r.r--;r.b--;fill(item->dc,r,down?0x4a4842:0x232c34);
        Rect top=r,bottom=r;top.b=action?(r.t+r.b)/2:r.b;bottom.t=top.b;
        label(item->dc,top,keyboard_keys[index].name,0,0xf1e8da,5);
        if(action)label(item->dc,bottom,short_keys[action],0,0xe5b974,5);
    }else if(item->id>=TAB_FIRST && item->id<TAB_FIRST+PAGE_COUNT){
        int tab=item->id-TAB_FIRST;
        fill(item->dc,r,tab==launcher_page?0x38434a:0x202830);
        label(item->dc,r,page_names[tab],1,tab==launcher_page?0xffe4ad:0xc4cbd0,5);
        if(tab==launcher_page){Rect bar=r;bar.t=r.b-unit(3);fill(item->dc,bar,0xe5b974);}
    }else{
    fill(item->dc,r,play?(down?0xc29654:0xe5b974):(down?0x35434c:0x253039));
    label(item->dc,r,play?TXT("Play game"):item->id==BROWSE?TXT("Choose ROM"):TXT("Controller settings"),1,play?0x201c17:0xf3e9d6,5);
    }
    if(item->state&16){r.l+=3;r.t+=3;r.r-=3;r.b-=3;DrawFocusRect(item->dc,&r);}
}
static void poll_launcher_pad(void){
    PadState state;
    if(slot<0 || XInputGetState(slot,&state)){
        for(slot=0;slot<4;slot++)if(!XInputGetState(slot,&state))break;
        if(slot==4)slot=-1;
    }
    if(pulse_active && (int32_t)(GetTickCount()-motor_until)>=0)pulse_active=0;
    apply_motor(slot,pulse_active?speeds[controls.strength-1]:0);
}
#include "pak_archive.inc"
#include "pad_editor.inc"
static LP launcher_proc(H win,D msg,WP key,LP param){
    if(msg==0xff || msg==0xfe)raw_pad_event(msg,key,param);
    if(editing && msg==0x114 && (H)param==dead_slider){deadzone_scroll(win,key);return 0;}
    if(editing && msg==0x201){diagram_click(win,param);return 0;}
    if(msg==15){if(editing)paint_editor(win);else paint_launcher(win,0);return 0;}
    if(msg==20)return 1;
    if(msg==0x2b){draw_button((DrawItem*)param);return 1;}
    if(msg==0x113){
        poll_launcher_pad();
        if(editing)editor_feedback(win);
        else if(launcher_page==PAGE_CONTROLS){
            int focused=GetForegroundWindow()==win;
            for(unsigned i=0;i<KEY_COUNT;i++){
                int pressed=focused && GetAsyncKeyState(keyboard_keys[i].vk)<0;
                if(pressed!=key_down[i]){
                    key_down[i]=pressed;InvalidateRect(GetDlgItem(win,KEY_FIRST+i),0,0);
                }
            }
        }
        return 0;
    }
    if(msg==0x111){
        unsigned id=key&65535,notification=key>>16;
        if(editing){editor_command(win,id,notification);return 0;}
        if(id>=SETTING && id<SETTING+14 && notification==1){
            int i=id-SETTING;LP sel=SendMessageW(fields[i],0x147,0,0);
            if(sel>=0){prefs[4+i]=(unsigned char)(sel+(i==0));save_graphics();InvalidateRect(win,0,0);}
        }else if(notification==0){
            if(id>=TAB_FIRST && id<TAB_FIRST+PAGE_COUNT){
                launcher_page=id-TAB_FIRST;page_controls(win,1);return 0;
            }
            if(id>=KEY_FIRST && id<KEY_FIRST+KEY_COUNT){assign_key(win,id-KEY_FIRST);return 0;}
            if(id==KEY_RESET){
                if(MessageBoxW(win,TXT("Restore the default keyboard bindings?"),TXT("Keyboard mapping"),0x24)==6){
                    keyboard_defaults();keyboard_save(win);page_controls(win,1);
                    for(unsigned i=0;i<KEY_COUNT;i++)InvalidateRect(GetDlgItem(win,KEY_FIRST+i),0,0);
                }
                return 0;
            }
            if(id==BACKUP || id==RESTORE)pak_archive(win,id==RESTORE);
            if(id==BROWSE){browse_rom(win);InvalidateRect(win,0,0);}
            if(id==CONTROLS){poll_launcher_pad();open_editor(win);}
            if(id==PLAY || id==1){
                if(!path[0] || !load_selected_rom()){
                    if(path[0])MessageBoxW(win,TXT("The saved ROM is unavailable or has changed. Choose its current location."),TXT("Gauntlet Launcher"),48);
                    if(!browse_rom(win)){InvalidateRect(win,0,0);return 0;}
                }
                ui_play=1;DestroyWindow(win);
            }
            if(id==2)DestroyWindow(win);
        }
        return 0;
    }
    if(msg==2){KillTimer(win,1);stop_motor();ui_done=1;return 0;}
    return DefWindowProcW(win,msg,key,param);
}
static int run_launcher(void){
    has_config=sibling_path(config,TXT("Gauntlet-ROM.cfg"));
    if(!has_config || !recall_rom(config))path[0]=0;
    read_art();
    int sw=GetSystemMetrics(0),sh=GetSystemMetrics(1);
    if(sw>0 && sw<UI_W+30)ui_scale=(sw-30)*1000/UI_W;
    if(sh>0 && sh<unit(UI_H)+90)ui_scale=(sh-90)*1000/UI_H;
    if(ui_scale<450)ui_scale=450;
    for(int i=0;i<4;i++)fonts[i]=CreateFontW(-unit(i==2?65:i==3?31:i==1?18:15),0,0,0,i>=2?800:i?600:400,0,0,0,1,0,0,5,0,i>=2?TXT("Georgia"):TXT("Segoe UI"));
    Class wc={0};wc.proc=launcher_proc;wc.instance=GetModuleHandleW(0);wc.name=TXT("GauntletLauncher");wc.cursor=LoadCursorW(0,(W)(uintptr_t)32512);
    D style=0x02ca0000;Rect r=box(0,0,UI_W,UI_H);AdjustWindowRect(&r,style,0);
    H win=RegisterClassW(&wc)?CreateWindowExW(0,wc.name,TXT("Towerbound | Static Recomp Playtest 17"),style,(sw-r.r+r.l)/2,(sh-r.b+r.t)/2,r.r-r.l,r.b-r.t,0,0,wc.instance,0):0;
    if(!win || !make_widgets(win)){
        MessageBoxW(0,TXT("The launcher window could not be created."),TXT("Gauntlet Launcher"),16);
        if(win)DestroyWindow(win);
    }else{
        register_pads(win);ShowWindow(win,1);UpdateWindow(win);SetTimer(win,1,50,0);
        Msg m;while(!ui_done && GetMessageW(&m,0,0,0)>0){
            if(!IsDialogMessageW(win,&m)){TranslateMessage(&m);DispatchMessageW(&m);}
        }
    }

    return ui_play;
}

static LP live_proc(H win,D msg,WP key,LP param){
    if(msg==15){paint_launcher(win,1);return 0;}
    if(msg==20)return 1;
    if(msg==0x111){
        unsigned id=key&65535;
        if(id>=SETTING && id<SETTING+12 && key>>16==1){
            int i=id-SETTING;LP selected=SendMessageW(fields[i],0x147,0,0);
            if(selected<0 || selected>=counts[i])return 0;
            unsigned new_value=selected+(i==0);if(prefs[4+i]==new_value)return 0;
            prefs[4+i]=new_value;
            if(i==3)display_mode(game_window);
            if(i==4 && !prefs[7])resize_window(game_window,prefs[8]);
            update_graphics();save_graphics();InvalidateRect(game_window,0,0);
        }else if(id==PLAY || id==1 || id==2)DestroyWindow(win);
        return 0;
    }
    if(msg==2){live_window=0;SetFocus(game_window);return 0;}
    return DefWindowProcW(win,msg,key,param);
}
static void toggle_live(H owner){
    if(live_window){DestroyWindow(live_window);return;}
    game_window=owner;
    static int registered;
    Class wc={0};wc.proc=live_proc;wc.instance=GetModuleHandleW(0);wc.name=TXT("TowerboundLiveSettings");wc.cursor=LoadCursorW(0,(W)(uintptr_t)32512);
    if(!registered)registered=RegisterClassW(&wc)!=0;
    D style=0x02ca0000;Rect r=box(0,0,UI_W,504);AdjustWindowRect(&r,style,0);
    if(registered)live_window=CreateWindowExW(0,wc.name,TXT("Towerbound | Live graphics settings"),style,
        (GetSystemMetrics(0)-r.r+r.l)/2,(GetSystemMetrics(1)-r.b+r.t)/2,r.r-r.l,r.b-r.t,owner,0,wc.instance,0);
    if(!live_window || !make_fields(live_window,12) ||
       !child(live_window,TXT("BUTTON"),TXT("Return to game (F1)"),0,478,449,232,36,PLAY)){
        if(live_window)DestroyWindow(live_window);
        MessageBoxW(owner,TXT("Windows could not open live settings."),TXT("Towerbound"),48);return;
    }
    stop_motor();ShowWindow(live_window,1);UpdateWindow(live_window);SetFocus(fields[5]);
}
