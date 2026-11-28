/* Linux host checks for native launcher page visibility, key layout and saved input. */
#include "../source/window.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int visible[512],pressed[256],menu_choice;
static unsigned char saved[KEY_COUNT+4];static D saved_size;
H GetModuleHandleW(W a){return (H)1;}
H CreateWindowExW(D ex,W cls,W name,D style,int x,int y,int w,int h,H parent,H id,H inst,H data){return id;}
H GetDlgItem(H parent,int id){return (H)(uintptr_t)id;}
int ShowWindow(H h,int show){visible[(uintptr_t)h]=show;return 1;}
int InvalidateRect(H h,const Rect *r,int e){return 1;}
LP SendMessageW(H h,D msg,WP a,LP b){return 0;}
int16_t GetAsyncKeyState(int key){return pressed[key]?-32768:0;}
D GetModuleFileNameW(H h,uint16_t *out,D size){out[0]='x';out[1]=0;return 1;}
H CreateFileW(W p,D mode,D share,H sec,D disp,D flags,H temp){return (mode==0x80000000 && !saved_size)?(H)(intptr_t)-1:(H)1;}
int WriteFile(H f,const void *buf,D count,D *done,H o){assert(count==sizeof saved);memcpy(saved,buf,count);*done=saved_size=count;return 1;}
int ReadFile(H f,void *buf,D size,D *done,H o){*done=saved_size;memcpy(buf,saved,saved_size);return 1;}
int FlushFileBuffers(H h){return 1;}
int CloseHandle(H h){return 1;}
int MoveFileExW(W a,W b,D f){return 1;}
int MessageBoxW(H h,W msg,W title,D flags){return 0;}
int GetWindowRect(H h,Rect *r){*r=(Rect){0};return 1;}
H CreatePopupMenu(void){return (H)1;}
int AppendMenuW(H m,D f,WP id,W name){return 1;}
int TrackPopupMenuEx(H m,D f,int x,int y,H win,void *p){return menu_choice;}
int DestroyMenu(H m){return 1;}
int main(void){
    H win=(H)1;assert(make_widgets(win));
    for(int page=0;page<PAGE_COUNT;page++){
        launcher_page=page;page_controls(win,1);
        assert(visible[BROWSE]==(page==PAGE_GAME) && visible[BACKUP]==(page==PAGE_GAME));
        assert(visible[CONTROLS]==(page==PAGE_CONTROLS) && visible[PLAY]);
        for(int i=0;i<14;i++)assert(visible[SETTING+i]==(page==(i<12?PAGE_GRAPHICS:PAGE_AUDIO)));
        for(unsigned i=0;i<KEY_COUNT;i++)assert(visible[KEY_FIRST+i]==(page==PAGE_CONTROLS));
    }
    int wkey=-1;
    for(unsigned i=0;i<KEY_COUNT;i++){
        KeyboardKey a=keyboard_keys[i];if(a.vk=='W')wkey=i;
        assert(a.x>=26 && a.x+a.w<=710);
        for(unsigned j=0;j<i;j++){
            KeyboardKey b=keyboard_keys[j];assert(a.vk!=b.vk);
            assert(a.y!=b.y || a.x+a.w<=b.x || b.x+b.w<=a.x);
        }
    }
    keyboard_read();D buttons=0;int x=0,y=0;
    pressed['W']=pressed['Z']=pressed[13]=1;keyboard_input(&buttons,&x,&y);
    assert(x==0 && y==80 && buttons==0x9000);
    pressed['S']=1;keyboard_input(&buttons,&x,&y);assert(y==0);
    menu_choice=3;assign_key(win,wkey);keyboard_defaults();keyboard_read();
    assert(keyboard_map[wkey]==2);
    memset(pressed,0,sizeof pressed);pressed['W']=1;buttons=0;y=0;x=32;
    keyboard_input(&buttons,&x,&y);assert(buttons==0x4000 && x==32 && y==0);
    saved[4]=255;keyboard_read();assert(keyboard_map[wkey]==19);
    printf("Four launcher pages and %u nonoverlapping keys verified; remap, reload, defaults and gameplay input passed.\n",(unsigned)KEY_COUNT);
}
