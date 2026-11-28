static int load_selected_rom(void){
    H file=CreateFileW(path,0x80000000,1,0,3,0x80,0);
    if(file==(H)(intptr_t)-1)return 0;
    D high=0,got=0;
    int valid=GetFileSize(file,&high)==sizeof rom && !high &&
        ReadFile(file,rom,sizeof rom,&got,0) && got==sizeof rom && !validate_rom();
    CloseHandle(file);
    return valid;
}

static int recall_rom(W config){
    H file=CreateFileW(config,0x80000000,7,0,3,0x80,0);
    if(file==(H)(intptr_t)-1)return 0;
    D high=0,got=0,bytes=GetFileSize(file,&high);
    int ok=!high && bytes>=8 && bytes<=sizeof path && !(bytes&1) &&
        ReadFile(file,path,bytes,&got,0) && got==bytes && path[bytes/2-1]==0;
    CloseHandle(file);
    if(!ok)return 0;
    for(D i=0;i<bytes/2-1;i++)if(!path[i])return 0;
    return (path[1]==':' && (path[2]==92 || path[2]==47)) || (path[0]==92 && path[1]==92);
}

extern int DeleteFileW(W);
static int remember_rom(W config){
    static uint16_t temporary[32768];
    uint16_t leaf[]=L"Gauntlet-ROM.00000000.tmp";
    D pid=GetCurrentProcessId();
    for(unsigned i=0;i<8;i++)leaf[13+i]="0123456789ABCDEF"[(pid>>(28-4*i))&15];
    if(!sibling_path(temporary,leaf))return 0;
    H file=CreateFileW(temporary,0x40000000,0,0,2,0x80,0);
    if(file==(H)(intptr_t)-1)return 0;
    D length=0,written=0;
    while(path[length])length++;
    D bytes=(length+1)*sizeof path[0];
    int ok=WriteFile(file,path,bytes,&written,0) && written==bytes && FlushFileBuffers(file);
    CloseHandle(file);
    if(ok && MoveFileExW(temporary,config,9))return 1;
    DeleteFileW(temporary);
    return 0;
}

static uint16_t config[32768], previous_rom[32768];
static int has_config;
static int browse_rom(H owner){
    unsigned n=0;while((previous_rom[n]=path[n]))n++;
    OpenFile dlg={0};dlg.owner=owner;dlg.size=sizeof dlg;dlg.file=path;dlg.maxfile=32768;
    dlg.filter=TXT("N64 ROM (*.z64)\0*.z64\0All files\0*.*\0");
    dlg.title=TXT("Choose Gauntlet Legends (USA) - this location will be remembered");
    dlg.flags=0x1808;
    for(;;){
        path[0]=0;
        if(!GetOpenFileNameW(&dlg)){
            for(n=0;(path[n]=previous_rom[n]);n++);
            if(CommDlgExtendedError())MessageBoxW(owner,TXT("Windows could not open the ROM picker."),TXT("Gauntlet Launcher"),16);
            return 0;
        }
        if(load_selected_rom())break;
        MessageBoxW(owner,TXT("Choose the original 16 MB Gauntlet Legends (USA) .z64 file."),TXT("Gauntlet Launcher"),48);
    }
    if(!has_config || !remember_rom(config))
        MessageBoxW(owner,TXT("The game can start, but the ROM location could not be saved. Use a writable folder for the launcher."),TXT("Gauntlet Launcher"),48);
    return 1;
}
