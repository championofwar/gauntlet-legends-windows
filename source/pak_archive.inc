extern int DeleteFileW(W);
/* The game and the launcher share the same exclusive lock for Pak operations. */
static int read_archive(W path,unsigned char *bytes){
    H f=CreateFileW(path,0x80000000,1,0,3,0x80,0);
    if(f==(H)(intptr_t)-1)return GetLastError()==2?-1:0;
    D high=0,got=0;
    int ok=GetFileSize(f,&high)==32768 && !high && ReadFile(f,bytes,32768,&got,0) && got==32768;
    CloseHandle(f);return ok;
}
static int valid_pak_id(const unsigned char *data){
    const int blocks[]={1,3,4,6};
    for(int i=0;i<4;i++){
        const unsigned char *id=data+blocks[i]*32;
        unsigned sum=0;for(int j=0;j<28;j+=2)sum+=256*id[j]+id[j+1];
        if((sum&65535)==(unsigned)(256*id[28]+id[29]) &&
           ((0xfff2-sum)&65535)==(unsigned)(256*id[30]+id[31]))return 1;
    }
    return 0;
}
static int write_archive(W path,const unsigned char *data,D disposition){
    H f=CreateFileW(path,0x40000000,0,0,disposition,0x80,0);
    if(f==(H)(intptr_t)-1)return 0;
    D n=0;int ok=WriteFile(f,data,32768,&n,0) && n==32768 && FlushFileBuffers(f);
    CloseHandle(f);if(!ok)DeleteFileW(path);return ok;
}
static int make_backup(const unsigned char *data){
    static uint16_t dest[32768];
    uint16_t time[8],leaf[100],*p=leaf;
    GetLocalTime(time);p=append_text(p,TXT("Gauntlet-Player1-"));
    const unsigned parts[]={0,1,3,4,5,6,7};
    for(int i=0;i<7;i++){p=append_number(p,time[parts[i]]);*p++='-';}
    for(unsigned seq=0;seq<100;seq++){
        append_text(append_number(p,seq),TXT(".mpk.bak"));
        if(!sibling_path(dest,leaf))return 0;
        if(write_archive(dest,data,1))return 1;
        if(GetLastError()!=80)return 0;
    }
    return 0;
}
static void pak_archive(H win,int restore){
    static uint16_t filename[32768],live[32768],tmp[32768],lockpath[32768];
    static unsigned char current[32768],replacement[32768];
    if(!sibling_path(live,TXT("Gauntlet-Player1.mpk")) ||
       !sibling_path(tmp,TXT("Gauntlet-Player1.restore.tmp")) ||
       !sibling_path(lockpath,TXT("Gauntlet-Player1.lock")))return;
    if(restore){
        static uint16_t folder[32768];sibling_path(folder,TXT(""));filename[0]=0;
        OpenFile dlg={.size=sizeof(dlg),.owner=win,.filter=TXT("Controller Pak backups\0*.mpk.bak;*.mpk\0\0"),
            .file=filename,.maxfile=32768,.initial=folder,.title=TXT("Restore Controller Pak"),.flags=0x1808};
        if(!GetOpenFileNameW(&dlg))return;
        if(read_archive(filename,replacement)!=1 || !valid_pak_id(replacement)){
            MessageBoxW(win,TXT("Choose a 32 KB Controller Pak with a valid Pak ID."),TXT("Restore Pak"),48);return;
        }
        if(MessageBoxW(win,TXT("Restore this Pak? A dated backup of your current Pak will be kept beside the game."),TXT("Restore Pak"),0x124)!=6)return;
    }
    H lock=CreateFileW(lockpath,0x40010000,0,0,4,0x04000100,0);
    if(lock==(H)(intptr_t)-1){MessageBoxW(win,TXT("Close any running copy of Towerbound and try again."),TXT("Controller Pak"),48);return;}
    int have=read_archive(live,current);
    int missing=have<0;
    int ok=have==1?make_backup(current):(restore && missing);
    if(ok && restore){
        ok=write_archive(tmp,replacement,2) && MoveFileExW(tmp,live,9);
        if(!ok)DeleteFileW(tmp);
    }
    CloseHandle(lock);
    MessageBoxW(win,ok?(restore?TXT("Controller Pak restored."):TXT("Dated .mpk.bak saved beside TowerboundRecomp.exe.")):
        TXT("Could not complete the operation. Check the Pak size, free space and folder permissions."),TXT("Controller Pak"),ok?64:48);
}
