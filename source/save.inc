extern unsigned char pak_data[32768];
extern int pak_ready,pak_dirty;
extern void pak_format(void);
extern D GetLastError(void);
extern int MoveFileExW(W,W,D);
static uint16_t pak_name[32768],pak_temp[32768],pak_lock_name[32768];
static H pak_lock;
static int save_failed;
static int sibling_path(uint16_t *dst,W name){
    D n=GetModuleFileNameW(0,dst,32768);
    if(!n||n>=32768)return 0;
    while(n && dst[n-1]!=92 && dst[n-1]!=47)n--;
    while(*name && n<32767)dst[n++]=*name++;
    dst[n]=0;return !*name;
}
static int pak_flush(void){
    if(!pak_dirty)return 1;
    H h=CreateFileW(pak_temp,0x40000000,0,0,2,0x80,0);
    D bytes=0;int ok=0;
    if(h!=(H)(intptr_t)-1){
        ok=WriteFile(h,pak_data,sizeof(pak_data),&bytes,0)&&bytes==sizeof(pak_data)&&FlushFileBuffers(h);
        CloseHandle(h);
        if(ok)ok=MoveFileExW(pak_temp,pak_name,9);
    }
    if(ok){pak_dirty=0;save_failed=0;return 1;}
    if(!save_failed){save_failed=1;startup_note("Controller Pak write failed\r\n");
        MessageBoxW(0,TXT("Windows could not save Gauntlet-Player1.mpk. Check free space and folder permissions."),TXT("Controller Pak"),48);}
    return 0;
}
static int pak_open(void){
    if(!sibling_path(pak_name,TXT("Gauntlet-Player1.mpk")) ||
       !sibling_path(pak_temp,TXT("Gauntlet-Player1.mpk.tmp")) ||
       !sibling_path(pak_lock_name,TXT("Gauntlet-Player1.lock")))return 0;
    pak_lock=CreateFileW(pak_lock_name,0x40010000,0,0,4,0x04000100,0);
    if(pak_lock==(H)(intptr_t)-1)return 0;
    H h=CreateFileW(pak_name,0x80000000,1,0,3,0x80,0);
    if(h==(H)(intptr_t)-1){
        if(GetLastError()!=2)return 0;
        pak_format();pak_dirty=1;if(!pak_flush())return 0;
    }else{
        D bytes=0,high=0;
        int ok=GetFileSize(h,&high)==sizeof(pak_data)&&!high&&ReadFile(h,pak_data,sizeof(pak_data),&bytes,0)&&bytes==sizeof(pak_data);
        CloseHandle(h);if(!ok)return 0;
    }
    pak_ready=1;startup_note("Player 1 Controller Pak mounted\r\n");return 1;
}
