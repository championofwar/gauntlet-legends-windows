#include "vk_small.h"
#include "gpu_layout.h"
extern void audio_log(const char*);
#ifdef GPU_LINUX
#include <dlfcn.h>
static void *loader(void){return dlopen("libvulkan.so.1",RTLD_NOW);}
static void *symbol(void *h,const char *n){return dlsym(h,n);}
static void unload(void *h){dlclose(h);}
#else
extern Handle LoadLibraryW(const uint16_t*);
extern void *GetProcAddress(Handle,const char*);
extern int FreeLibrary(Handle);
static Handle loader(void){return LoadLibraryW((const uint16_t*)L"vulkan-1.dll");}
#define symbol GetProcAddress
#define unload FreeLibrary
#endif
typedef Fn (*ProcAddr)(Handle,const char*);
#define VK_FUNCTIONS(X) \
 X(CreateInstance,Result,(const InstanceInfo*,void*,Handle*)) \
 X(DestroyInstance,void,(Handle,void*)) \
 X(EnumeratePhysicalDevices,Result,(Handle,U*,Handle*)) \
 X(GetPhysicalDeviceProperties,void,(Handle,void*)) \
 X(GetPhysicalDeviceQueueFamilyProperties,void,(Handle,U*,QueueFamily*)) \
 X(GetPhysicalDeviceMemoryProperties,void,(Handle,MemoryProperties*)) \
 X(CreateDevice,Result,(Handle,const DeviceInfo*,void*,Handle*)) \
 X(DestroyDevice,void,(Handle,void*)) \
 X(DeviceWaitIdle,Result,(Handle)) \
 X(GetDeviceQueue,void,(Handle,U,U,Handle*)) \
 X(CreateBuffer,Result,(Handle,const BufferInfo*,void*,Handle*)) \
 X(DestroyBuffer,void,(Handle,Handle,void*)) \
 X(GetBufferMemoryRequirements,void,(Handle,Handle,Requirements*)) \
 X(AllocateMemory,Result,(Handle,const AllocInfo*,void*,Handle*)) \
 X(FreeMemory,void,(Handle,Handle,void*)) \
 X(BindBufferMemory,Result,(Handle,Handle,Handle,Size)) \
 X(MapMemory,Result,(Handle,Handle,Size,Size,U,void**)) \
 X(UnmapMemory,void,(Handle,Handle)) \
 X(CreateDescriptorSetLayout,Result,(Handle,const SetLayoutInfo*,void*,Handle*)) \
 X(DestroyDescriptorSetLayout,void,(Handle,Handle,void*)) \
 X(CreatePipelineLayout,Result,(Handle,const LayoutInfo*,void*,Handle*)) \
 X(DestroyPipelineLayout,void,(Handle,Handle,void*)) \
 X(CreateShaderModule,Result,(Handle,const ShaderInfo*,void*,Handle*)) \
 X(DestroyShaderModule,void,(Handle,Handle,void*)) \
 X(CreateComputePipelines,Result,(Handle,Handle,U,const ComputeInfo*,void*,Handle*)) \
 X(DestroyPipeline,void,(Handle,Handle,void*)) \
 X(CreateDescriptorPool,Result,(Handle,const PoolInfo*,void*,Handle*)) \
 X(DestroyDescriptorPool,void,(Handle,Handle,void*)) \
 X(AllocateDescriptorSets,Result,(Handle,const SetAlloc*,Handle*)) \
 X(UpdateDescriptorSets,void,(Handle,U,const Write*,U,const void*)) \
 X(CreateCommandPool,Result,(Handle,const CommandPool*,void*,Handle*)) \
 X(DestroyCommandPool,void,(Handle,Handle,void*)) \
 X(AllocateCommandBuffers,Result,(Handle,const CommandAlloc*,Handle*)) \
 X(ResetCommandBuffer,Result,(Handle,U)) \
 X(BeginCommandBuffer,Result,(Handle,const Begin*)) \
 X(EndCommandBuffer,Result,(Handle)) \
 X(CmdCopyBuffer,void,(Handle,Handle,Handle,U,const Copy*)) \
 X(CmdPipelineBarrier,void,(Handle,U,U,U,U,const Barrier*,U,const void*,U,const void*)) \
 X(CmdBindPipeline,void,(Handle,U,Handle)) \
 X(CmdBindDescriptorSets,void,(Handle,U,Handle,U,U,const Handle*,U,const U*)) \
 X(CmdPushConstants,void,(Handle,Handle,U,U,U,const void*)) \
 X(CmdDispatch,void,(Handle,U,U,U)) \
 X(CreateFence,Result,(Handle,const Fence*,void*,Handle*)) \
 X(DestroyFence,void,(Handle,Handle,void*)) \
 X(GetFenceStatus,Result,(Handle,Handle)) \
 X(ResetFences,Result,(Handle,U,const Handle*)) \
 X(QueueSubmit,Result,(Handle,U,const Submit*,Handle))
#define DECLARE(name,result,args) static result (*name)args;
VK_FUNCTIONS(DECLARE)
static const U spirv[]={
#include "gpu_spv.inc"
};
static Handle dll,instance,device,queue,hostBuf,deviceBuf,hostMem,deviceMem;
static Handle setLayout,layout,module,pipeline,pool,set,cmdPool,cmd,fence;
static GpuStorage *mapping;
static U family,submittedWidth,submittedScale,submittedHeight;
static int usable,pending;
U gpu_submissions,gpu_deferred,gpu_texture_bytes;
static U viewW,viewH,returnedW,returnedH,submittedReadW,submittedReadH;
void gpu_viewport(U w,U h){viewW=w;viewH=h;}
U gpu_read_width(void){return returnedW;}
U gpu_read_height(void){return returnedH;}

void gpu_invalidate(void){submittedScale=0;}
int gpu_ready(void){return usable;}
void gpu_close(void){
    usable=0;
    if(device){
        DeviceWaitIdle(device);
        if(mapping)UnmapMemory(device,hostMem);
        if(fence)DestroyFence(device,fence,0);
        if(cmdPool)DestroyCommandPool(device,cmdPool,0);
        if(pool)DestroyDescriptorPool(device,pool,0);
        if(pipeline)DestroyPipeline(device,pipeline,0);
        if(module)DestroyShaderModule(device,module,0);
        if(layout)DestroyPipelineLayout(device,layout,0);
        if(setLayout)DestroyDescriptorSetLayout(device,setLayout,0);
        if(hostBuf)DestroyBuffer(device,hostBuf,0);
        if(deviceBuf)DestroyBuffer(device,deviceBuf,0);
        if(hostMem)FreeMemory(device,hostMem,0);
        if(deviceMem)FreeMemory(device,deviceMem,0);
        DestroyDevice(device,0);device=0;
    }
    if(instance && DestroyInstance)DestroyInstance(instance,0);
    if(dll)unload(dll);
    instance=dll=0;mapping=0;
}
static int buffer(const MemoryProperties *m,U usage,U required,U preferred,Handle *buf,Handle *mem){
    BufferInfo info={.type=12,.size=sizeof(GpuStorage),.usage=usage};
    if(CreateBuffer(device,&info,0,buf))return 0;
    Requirements req;GetBufferMemoryRequirements(device,*buf,&req);
    U chosen=~0u;
    for(U i=0;i<m->count;i++){
        U flags=m->types[i].flags;
        if((req.mask&(1u<<i)) && (flags&required)==required){
            if(chosen==~0u)chosen=i;
            if((flags&preferred)==preferred){chosen=i;break;}
        }
    }
    if(chosen==~0u)return 0;
    AllocInfo alloc={.type=5,.size=req.size,.index=chosen};
    return !AllocateMemory(device,&alloc,0,mem) && !BindBufferMemory(device,*buf,*mem,0);
}
static char selected[256];
const char *gpu_device_name(void){return selected;}
static int initialize(void){
    dll=loader();if(!dll)return 0;
    ProcAddr get=(ProcAddr)symbol(dll,"vkGetInstanceProcAddr");
    if(!get)return 0;
    CreateInstance=(void*)get(0,"vkCreateInstance");
    App app={.type=0,.app="Gauntlet graphics",.api=1u<<22};
    InstanceInfo ii={.type=1,.app=&app};
    if(!CreateInstance || CreateInstance(&ii,0,&instance))return 0;
#define LOAD(name,result,args) name=(void*)get(instance,"vk" #name);if(!name)return 0;
    VK_FUNCTIONS(LOAD)
    U count=16;Handle physical[16],chosen=0;int rank=0;
    Result enumeration=EnumeratePhysicalDevices(instance,&count,physical);
    if(enumeration!=0 && enumeration!=5)return 0;
    selected[0]=0;
    for(U i=0;i<count;i++){
        /* VkPhysicalDeviceProperties begins with five uint32_t and deviceName[256]. */
        union {uint64_t align;unsigned char data[4096];} prop;
        GetPhysicalDeviceProperties(physical[i],&prop);
        U type;__builtin_memcpy(&type,prop.data+16,4);
        int score=type==2?3:type==1?2:1;
#ifndef GPU_LINUX
        if(type==4)continue;
#endif
        if(score<=rank)continue;
        U n=32;QueueFamily families[32];
        GetPhysicalDeviceQueueFamilyProperties(physical[i],&n,families);
        for(U k=0;k<n;k++)if(families[k].count && (families[k].flags&2)){
            rank=score;chosen=physical[i];family=k;
            for(U j=0;j<255;j++)selected[j]=prop.data[20+j];
            break;
        }
    }
    if(!chosen)return 0;
    float priority=1;
    QueueInfo qi={.type=2,.family=family,.count=1,.priorities=&priority};
    DeviceInfo di={.type=3,.queues=1,.queueInfo=&qi};
    if(CreateDevice(chosen,&di,0,&device))return 0;
    GetDeviceQueue(device,family,0,&queue);
    MemoryProperties mem;GetPhysicalDeviceMemoryProperties(chosen,&mem);
    if(!buffer(&mem,3,6,8,&hostBuf,&hostMem) || !buffer(&mem,0x23,1,1,&deviceBuf,&deviceMem))return 0;
    if(MapMemory(device,hostMem,0,sizeof(GpuStorage),0,(void**)&mapping))return 0;
    LayoutBinding binding={0,7,1,32,0};
    SetLayoutInfo si={.type=32,.count=1,.bindings=&binding};
    if(CreateDescriptorSetLayout(device,&si,0,&setLayout))return 0;
    PushRange push={32,0,32};
    LayoutInfo li={.type=30,.setCount=1,.layouts=&setLayout,.rangeCount=1,.ranges=&push};
    if(CreatePipelineLayout(device,&li,0,&layout))return 0;
    ShaderInfo shader={.type=16,.size=sizeof(spirv),.code=spirv};
    if(CreateShaderModule(device,&shader,0,&module))return 0;
    ComputeInfo ci={.type=29,.stage={.type=18,.stage=32,.module=module,.entry="main"},.layout=layout,.index=-1};
    if(CreateComputePipelines(device,0,1,&ci,0,&pipeline))return 0;
    PoolSize ps={7,1};PoolInfo pi={.type=33,.maxSets=1,.count=1,.sizes=&ps};
    if(CreateDescriptorPool(device,&pi,0,&pool))return 0;
    SetAlloc sa={.type=34,.pool=pool,.count=1,.layouts=&setLayout};
    if(AllocateDescriptorSets(device,&sa,&set))return 0;
    BufferBinding bb={deviceBuf,0,sizeof(GpuStorage)};
    Write write={.type=35,.set=set,.count=1,.typeId=7,.buffers=&bb};
    UpdateDescriptorSets(device,1,&write,0,0);
    CommandPool cp={.type=39,.flags=2,.family=family};
    if(CreateCommandPool(device,&cp,0,&cmdPool))return 0;
    CommandAlloc ca={.type=40,.pool=cmdPool,.count=1};
    if(AllocateCommandBuffers(device,&ca,&cmd))return 0;
    Fence fi={.type=8};
    if(CreateFence(device,&fi,0,&fence))return 0;
    audio_log("Vulkan graphics device: ");audio_log(selected);audio_log("\r\n");
    return 1;
}
int gpu_open(void){
    usable=initialize();
    if(!usable){audio_log("Vulkan unavailable, using software scaling\r\n");gpu_close();}
    return usable;
}
static void barrier(U from,U to,U read,U write){
    Barrier b={.type=46,.src=read,.dst=write};
    CmdPipelineBarrier(cmd,from,to,0,1,&b,0,0,0,0);
}
static void transfer(Handle src,Handle dst,Size offset,Size bytes){
    if(bytes){Copy c={offset,offset,bytes};CmdCopyBuffer(cmd,src,dst,1,&c);}
}
static void copybytes(void *d,const void *s,size_t bytes){
    unsigned char *dst=d;const unsigned char *src=s;
    for(size_t i=0;i<bytes;i++)dst[i]=src[i];
}
static int disable(void){
    usable=0;audio_log("Vulkan frame failed; switched to software scaling\r\n");return -1;
}
int gpu_collect(U scale,U renderW,U renderH,U *dest){
    int presented=0;
    if(pending){
        Result r=GetFenceStatus(device,fence);
        if(r==1)return 0; /* GPU still busy. The Windows audio pump keeps running. */
        if(r || ResetFences(device,1,&fence))return disable();
        if(submittedScale==scale && submittedWidth==renderW && submittedHeight==renderH){
            copybytes(dest,mapping->output,submittedReadW*submittedReadH*4);
            returnedW=submittedReadW;returnedH=submittedReadH;presented=scale;
        }
        pending=0;
    }
    return presented;
}
enum { RAM_PAGE=16384, RAM_PAGES=0x800000/RAM_PAGE };
static U axis_last(int extent,U mask,U mode){
    U maximum=extent>0?(U)(extent-1):0;
    if(!mask)return maximum;
    U wrapped=(1u<<mask)-1;
    return (mode&2) && maximum<wrapped?maximum:wrapped;
}
static void mark_texture(const HdTriangle *t,unsigned char *pages){
    if(!t->format || t->source>=0x800000)return;
    U x=axis_last(t->width,t->axes&15,(t->axes>>8)&3);
    U y=axis_last(t->height,(t->axes>>4)&15,(t->axes>>10)&3);
    uint64_t last=(uint64_t)t->source+(uint64_t)y*t->stride+(((uint64_t)x<<(t->format>>8))>>1)+3;
    if(last>=0x800000)last=0x7fffff;
    for(U i=t->source/RAM_PAGE;i<=last/RAM_PAGE;i++)pages[i]=1;
}
int gpu_frame(U width,U scale,U aa,U smooth,const U *colour,const uint16_t *owner,U *dest,
              U outW,U outH,U af,U hud,const HdTriangle *tri,const U *pal,const unsigned char *ram,const U *tone){
    U renderW=outH?outW:width*scale,renderH=outH?outH:GPU_HEIGHT*scale;
    if(!renderW || !renderH || (uint64_t)renderW*renderH>HD_MAX_PIXELS)return -1;
    if(!usable)return -1;
    int presented=0;
    presented=gpu_collect(scale,renderW,renderH,dest);
    if(pending)gpu_deferred++;
    if(presented<0 || pending)return presented;
    Begin begin={.type=42,.flags=1};
    if(ResetCommandBuffer(cmd,0) || BeginCommandBuffer(cmd,&begin))return disable();
    copybytes(mapping->native,colour,width*GPU_HEIGHT*4);
    copybytes(mapping->owner,owner,width*GPU_HEIGHT*2);
    transfer(hostBuf,deviceBuf,0,width*GPU_HEIGHT*4);
    transfer(hostBuf,deviceBuf,offsetof(GpuStorage,owner),width*GPU_HEIGHT*2);
    if(outH){
        /* The VI can show a buffer several display lists behind the CPU. Keep
           the entire ID space, but upload only pages referenced by this frame. */
        unsigned char triangles_used[HD_TRIS/256]={0},palettes_used[HD_PALETTES/16]={0};
        static U visited[HD_TRIS],generation;
        unsigned char ram_pages[RAM_PAGES]={0};
        if(!++generation){for(U j=0;j<HD_TRIS;j++)visited[j]=0;generation=1;}
        for(U i=0;i<width*GPU_HEIGHT;i++){
            U id=owner[i];if(!id || visited[id]==generation)continue;
            visited[id]=generation;mark_texture(tri+id,ram_pages);
            triangles_used[id/256]=1;
            if(((tri[id].format>>4)&15)==2)palettes_used[(tri[id].palette%HD_PALETTES)/16]=1;
        }
        for(U i=0;i<HD_TRIS/256;i++)if(triangles_used[i]){
            size_t offset=i*256*sizeof(HdTriangle),bytes=256*sizeof(HdTriangle);
            copybytes(mapping->triangles+i*256,tri+i*256,bytes);
            transfer(hostBuf,deviceBuf,offsetof(GpuStorage,triangles)+offset,bytes);
        }
        for(U i=0;i<HD_PALETTES/16;i++)if(palettes_used[i]){
            size_t offset=i*4096*sizeof(U),bytes=4096*sizeof(U);
            copybytes(mapping->palette+i*4096,pal+i*4096,bytes);
            transfer(hostBuf,deviceBuf,offsetof(GpuStorage,palette)+offset,bytes);
        }
        gpu_texture_bytes=0;
        for(U i=0;i<RAM_PAGES;){
            if(!ram_pages[i]){i++;continue;}
            U first=i;while(i<RAM_PAGES && ram_pages[i])i++;
            U offset=first*RAM_PAGE,bytes=(i-first)*RAM_PAGE;
            copybytes((unsigned char*)mapping->memory+offset,ram+offset,bytes);
            transfer(hostBuf,deviceBuf,offsetof(GpuStorage,memory)+offset,bytes);
            gpu_texture_bytes+=bytes;
        }
        copybytes(mapping->tone,tone,sizeof(mapping->tone));
        transfer(hostBuf,deviceBuf,offsetof(GpuStorage,tone),sizeof(mapping->tone));
    }
    barrier(0x1000,0x800,0x1000,0x20);
    CmdBindPipeline(cmd,1,pipeline);
    CmdBindDescriptorSets(cmd,1,layout,0,1,&set,0,0);
    U opts[8]={scale,aa,smooth,width<<1,outW,outH,af,hud};
    if(outH){
        opts[2]=(U)-1;CmdPushConstants(cmd,layout,32,0,32,opts);
        CmdDispatch(cmd,(width+7)/8,(GPU_HEIGHT+7)/8,1);
        barrier(0x800,0x800,0x40,0x20);opts[2]=smooth;
    }
    CmdPushConstants(cmd,layout,32,0,32,opts);
    U x=(renderW+7)/8,y=(renderH+7)/8;
    CmdDispatch(cmd,x,y,1);
    if(aa){
        barrier(0x800,0x800,0x40,0x20);opts[3]|=1;
        CmdPushConstants(cmd,layout,32,0,32,opts);CmdDispatch(cmd,x,y,1);
    }
    U readW=renderW,readH=renderH;
    size_t sourceOffset=offsetof(GpuStorage,output);
    if(outH && viewW && viewH && (viewW<renderW || viewH<renderH)){
        if((uint64_t)viewW*renderH<(uint64_t)viewH*renderW){
            readW=viewW;readH=((uint64_t)viewW*renderH+renderW/2)/renderW;
        }else{readH=viewH;readW=((uint64_t)viewH*renderW+renderH/2)/renderH;}
        if(!readW)readW=1;if(!readH)readH=1;
        barrier(0x800,0x800,0x40,0x60);
        opts[0]=readW;opts[1]=readH;opts[2]=(U)-2;
        CmdPushConstants(cmd,layout,32,0,32,opts);
        CmdDispatch(cmd,(readW+7)/8,(readH+7)/8,1);
        sourceOffset=offsetof(GpuStorage,scene);
    }
    barrier(0x800,0x1000,0x40,0x800);
    Copy readback={sourceOffset,offsetof(GpuStorage,output),readW*readH*4};
    CmdCopyBuffer(cmd,deviceBuf,hostBuf,1,&readback);
    submittedReadW=readW;submittedReadH=readH;
    barrier(0x1000,0x4000,0x1000,0x2000);
    if(EndCommandBuffer(cmd))return disable();
    Submit sub={.type=4,.count=1,.cmd=&cmd};
    if(QueueSubmit(queue,1,&sub,fence))return disable();
    submittedWidth=renderW;submittedHeight=renderH;submittedScale=scale;pending=1;gpu_submissions++;
    return presented;
}

int gpu_draw(U width,U scale,U aa,U smooth,const U *colour,const uint16_t *owner,U *dest){
    return gpu_frame(width,scale,aa,smooth,colour,owner,dest,0,0,0,0,0,0,0,0);
}
