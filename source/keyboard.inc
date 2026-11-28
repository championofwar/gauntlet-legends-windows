typedef struct { W name; unsigned char vk,initial; short x,y,w; } KeyboardKey;
static const KeyboardKey keyboard_keys[]={
    {TXT("1"),49,0,38,307,48},
    {TXT("2"),50,0,90,307,48},
    {TXT("3"),51,0,142,307,48},
    {TXT("4"),52,0,194,307,48},
    {TXT("5"),53,0,246,307,48},
    {TXT("6"),54,0,298,307,48},
    {TXT("7"),55,0,350,307,48},
    {TXT("8"),56,0,402,307,48},
    {TXT("9"),57,0,454,307,48},
    {TXT("0"),48,0,506,307,48},
    {TXT("-"),189,0,558,307,48},
    {TXT("="),187,0,610,307,48},
    {TXT("Q"),81,5,76,349,48},
    {TXT("W"),87,19,128,349,48},
    {TXT("E"),69,6,180,349,48},
    {TXT("R"),82,0,232,349,48},
    {TXT("T"),84,0,284,349,48},
    {TXT("Y"),89,0,336,349,48},
    {TXT("U"),85,0,388,349,48},
    {TXT("I"),73,7,440,349,48},
    {TXT("O"),79,0,492,349,48},
    {TXT("P"),80,0,544,349,48},
    {TXT("["),219,0,596,349,48},
    {TXT("]"),221,0,648,349,48},
    {TXT("A"),65,21,61,391,48},
    {TXT("S"),83,20,113,391,48},
    {TXT("D"),68,22,165,391,48},
    {TXT("F"),70,0,217,391,48},
    {TXT("G"),71,0,269,391,48},
    {TXT("H"),72,0,321,391,48},
    {TXT("J"),74,9,373,391,48},
    {TXT("K"),75,8,425,391,48},
    {TXT("L"),76,10,477,391,48},
    {TXT(";"),186,0,529,391,48},
    {TXT("'"),222,0,581,391,48},
    {TXT("Enter"),13,4,633,391,72},
    {TXT("Z"),90,1,84,433,48},
    {TXT("X"),88,2,136,433,48},
    {TXT("C"),67,0,188,433,48},
    {TXT("V"),86,0,240,433,48},
    {TXT("B"),66,0,292,433,48},
    {TXT("N"),78,0,344,433,48},
    {TXT("M"),77,0,396,433,48},
    {TXT(","),188,0,448,433,48},
    {TXT("."),190,0,500,433,48},
    {TXT("/"),191,0,552,433,48},
    {TXT("Up"),38,19,607,433,48},
    {TXT("Space"),32,3,175,475,200},
    {TXT("Left"),37,21,554,475,48},
    {TXT("Down"),40,20,607,475,48},
    {TXT("Right"),39,22,660,475,48},
    {TXT("Back"),8,0,663,307,39},
    {TXT("Tab"),9,0,31,349,41},
    {TXT("Shift"),160,0,31,433,49},
    {TXT("Ctrl"),162,0,31,475,53},
    {TXT("Alt"),164,0,92,475,53}
};
enum { KEY_COUNT=sizeof keyboard_keys/sizeof keyboard_keys[0], KEY_ACTION_COUNT=23 };
static unsigned char keyboard_map[KEY_COUNT];
static uint16_t key_file[32768],key_temp[32768];
static void keyboard_defaults(void){
    for(unsigned i=0;i<KEY_COUNT;i++)keyboard_map[i]=keyboard_keys[i].initial;
}
static void keyboard_read(void){
    keyboard_defaults();
    if(!sibling_path(key_file,TXT("Gauntlet-Keyboard.cfg")) ||
       !sibling_path(key_temp,TXT("Gauntlet-Keyboard.tmp")))return;
    H f=CreateFileW(key_file,0x80000000,1,0,3,0x80,0);
    if(f==(H)(intptr_t)-1)return;
    unsigned char data[4+KEY_COUNT];D got=0;
    int ok=ReadFile(f,data,sizeof data,&got,0);CloseHandle(f);
    if(!ok || got!=sizeof data || __builtin_memcmp(data,"KEY1",4))return;
    for(unsigned i=0;i<KEY_COUNT;i++)if(data[i+4]>=KEY_ACTION_COUNT)return;
    __builtin_memcpy(keyboard_map,data+4,KEY_COUNT);
}
static void keyboard_save(H win){
    unsigned char data[4+KEY_COUNT]={'K','E','Y','1'};D written=0;
    __builtin_memcpy(data+4,keyboard_map,KEY_COUNT);
    H f=CreateFileW(key_temp,0x40000000,0,0,2,0x80,0);
    int ok=0;
    if(f!=(H)(intptr_t)-1){
        ok=WriteFile(f,data,sizeof data,&written,0) && written==sizeof data && FlushFileBuffers(f);
        CloseHandle(f);
        if(ok)ok=MoveFileExW(key_temp,key_file,9);
    }
    if(!ok)MessageBoxW(win,TXT("The keyboard mapping works for this session. Windows could not save it."),TXT("Keyboard settings"),48);
}
static void keyboard_input(D *buttons,int *x,int *y){
    unsigned active=0;
    for(unsigned i=0;i<KEY_COUNT;i++){
        unsigned action=keyboard_map[i];
        if(action && GetAsyncKeyState(keyboard_keys[i].vk)<0)active|=1u<<action;
    }
    for(unsigned i=1;i<=18;i++)if(active&(1u<<i))*buttons|=masks[i];
    int up=!!(active&((1u<<15)|(1u<<19))),down=!!(active&((1u<<16)|(1u<<20)));
    int left=!!(active&((1u<<17)|(1u<<21))),right=!!(active&((1u<<18)|(1u<<22)));
    if(left||right)*x=80*(right-left);
    if(up||down)*y=80*(up-down);
}
