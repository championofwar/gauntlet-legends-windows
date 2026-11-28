/* Run in an empty temporary directory. The file adapter uses real local files. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../source/window.c"
static D last_error;static int cancel,block_lock;static const char *choice;
static char *narrow(W name,char *buf){int i=0;while(name[i]){buf[i]=(char)name[i];i++;}buf[i]=0;return buf;}
D GetModuleFileNameW(H a,uint16_t *p,D n){const char *s="./Towerbound.exe";D i=0;while(s[i]){p[i]=s[i];i++;}p[i]=0;return i;}
D GetLastError(void){return last_error;}
H CreateFileW(W name,D access,D share,H a,D disp,D flags,H b){
    char path[512];narrow(name,path);
    if(block_lock && strstr(path,".lock"))return (H)-1;
    FILE *f=fopen(path,disp==1?"wbx":disp==2?"wb":disp==4?"ab":"rb");
    if(!f){last_error=errno==EEXIST?80:errno==ENOENT?2:5;return (H)-1;}return f;
}
int ReadFile(H f,void *p,D n,D *got,H a){*got=fread(p,1,n,f);return !ferror(f);}
int WriteFile(H f,const void *p,D n,D *got,H a){*got=fwrite(p,1,n,f);return !ferror(f);}
int CloseHandle(H f){return fclose(f)==0;}
int FlushFileBuffers(H f){return fflush(f)==0;}
D GetFileSize(H f,D *hi){*hi=0;long pos=ftell(f);fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,pos,SEEK_SET);return n;}
int MoveFileExW(W a,W b,D flags){char x[512],y[512];return rename(narrow(a,x),narrow(b,y))==0;}
int DeleteFileW(W path){char s[512];return !unlink(narrow(path,s));}
void GetLocalTime(uint16_t *t){uint16_t when[]={2026,11,4,26,21,0,0,1};memcpy(t,when,sizeof when);}
int GetOpenFileNameW(OpenFile *o){const char *s=choice;uint16_t *p=o->file;while(*s)*p++=*s++;*p=0;return 1;}
int MessageBoxW(H h,W msg,W title,D mode){return mode==0x124?(cancel?7:6):1;}
/* Check input and overlay through the same routines that handle a live pad and WM_PAINT. */
static int64_t clock_tick;
static uint16_t overlay_line[384];
unsigned picture_version;
unsigned cart_output_width(void){return bitmap.w;}
unsigned cart_output_height(void){return -bitmap.h;}
int QueryPerformanceFrequency(int64_t *n){*n=1000;return 1;}
int QueryPerformanceCounter(int64_t *n){*n=clock_tick;return 1;}
const char *gpu_device_name(void){return "Test Vulkan device";}
int gpu_ready(void){return 1;}
unsigned audio_dropouts(void){return 7;}
int DrawTextW(H dc,W in,int len,Rect *r,D flags){
    unsigned i=0;do{overlay_line[i]=in[i];}while(in[i++]);return 1;
}
H CreateSolidBrush(D c){return (H)1;}
H GetStockObject(int which){return (H)1;}
int FillRect(H dc,const Rect *r,H brush){return 1;}
int DeleteObject(H o){return 1;}
D SetTextColor(H dc,D c){return 0;}
int SetBkMode(H dc,int mode){return 1;}
static void controls_and_overlay(void){
    int x,y;controls=defaults;PadState pad={.lx=4400,.ly=4800};
    mapped_pad(&pad,&x,&y);assert(x==0 && y==12);
    stick_deadzone=0;mapped_pad(&pad,&x,&y);assert(x==11 && y==12);
    stick_deadzone=40;controls.swap=1;pad.rx=-12800;pad.ry=12400;
    mapped_pad(&pad,&x,&y);assert(x==-32 && y==0);
    unsigned char v5[]={'G','F','X',5,1,1,0,0,12,2,2,2,1,2,2};
    FILE *f=fopen("Gauntlet-Graphics.cfg","wb");assert(f);fwrite(v5,1,sizeof v5,f);fclose(f);
    read_graphics();assert(prefs[9]==2 && prefs[10]==2 && prefs[18]==15 && stick_deadzone==15);
    prefs[9]=4;prefs[18]=40;prefs[16]=0;save_graphics();prefs[9]=prefs[18]=prefs[16]=1;read_graphics();
    assert(prefs[9]==4 && prefs[18]==40 && prefs[16]==0);
    Rect screen={0,0,1920,1080};bitmap.w=3840;bitmap.h=-2160;
    for(unsigned i=0;i<65;i++){clock_tick+=20;picture_version++;draw_performance(0,screen);}
    /* A repeat paint has no new image and must not add a graph sample. */
    clock_tick+=9;draw_performance(0,screen);
    char line[384];narrow(overlay_line,line);
    assert(strstr(line,"50 FPS") && strstr(line,"20.0 ms") && strstr(line,"3840 x 2160") &&
           strstr(line,"Test Vulkan device") && strstr(line,"underruns  7"));
    puts("Deadzone, swapped stick, settings migration, round-trip and presentation telemetry passed.");
}
int main(void){
    controls_and_overlay();
    unsigned char one[32768]={0},two[32768],out[32768];one[32+30]=0xff;one[32+31]=0xf2;
    memcpy(two,one,sizeof two);two[2048]=67;
    W live=TXT("Gauntlet-Player1.mpk"), first=TXT("Gauntlet-Player1-2026-11-26-21-0-0-1-0.mpk.bak"), second=TXT("Gauntlet-Player1-2026-11-26-21-0-0-1-1.mpk.bak");
    assert(write_archive(live,one,1));pak_archive(0,0);
    assert(read_archive(first,out)==1 && !memcmp(one,out,sizeof one));
    choice="Gauntlet-Player1-2026-11-26-21-0-0-1-0.mpk.bak";
    assert(write_archive(live,two,2));cancel=1;pak_archive(0,1);cancel=0;
    block_lock=1;pak_archive(0,1);block_lock=0;
    assert(read_archive(live,out)==1 && !memcmp(two,out,sizeof two));
    pak_archive(0,1);assert(read_archive(live,out)==1 && !memcmp(one,out,sizeof one));
    assert(read_archive(second,out)==1 && !memcmp(two,out,sizeof two));
    choice="bad.mpk";FILE *f=fopen(choice,"wb");fputs("invalid",f);fclose(f);pak_archive(0,1);
    assert(read_archive(live,out)==1 && !memcmp(one,out,sizeof one));
    puts("Pak backup, restore, pre-restore backup, cancellation, busy lock and invalid file passed.");
}
