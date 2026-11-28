/* Vulkan 1.0 x64 ABI subset used by the offscreen compute renderer. */
#include <stddef.h>
#include <stdint.h>
typedef uint32_t U;
typedef uint64_t Size;
typedef void *Handle;
typedef int Result;
typedef void (*Fn)(void);
#define HEAD U type; const void *next
typedef struct {HEAD; const char *app;U version;const char *engine;U engineVersion,api;} App;
typedef struct {HEAD;U flags;const App *app;U layers;const char **layerNames;U extensions;const char **extensionNames;} InstanceInfo;
typedef struct {HEAD;U flags,family,count;const float *priorities;} QueueInfo;
typedef struct {HEAD;U flags,queues;const QueueInfo *queueInfo;U layers;const char **layerNames;U extensions;const char **extensionNames;const void *features;} DeviceInfo;
typedef struct {U flags,count,timestamp,granularity[3];} QueueFamily;
typedef struct {U flags,heap;} MemoryType;
typedef struct {Size size;U flags;} Heap;
typedef struct {U count;MemoryType types[32];U heaps;Heap heap[16];} MemoryProperties;
typedef struct {Size size,alignment;U mask;} Requirements;
typedef struct {HEAD;U flags;Size size;U usage,sharing,count;const U *families;} BufferInfo;
typedef struct {HEAD;Size size;U index;} AllocInfo;
typedef struct {U binding,type,count,stages;const Handle *samplers;} LayoutBinding;
typedef struct {HEAD;U flags,count;const LayoutBinding *bindings;} SetLayoutInfo;
typedef struct {U stages,offset,size;} PushRange;
typedef struct {HEAD;U flags,setCount;const Handle *layouts;U rangeCount;const PushRange *ranges;} LayoutInfo;
typedef struct {HEAD;U flags;size_t size;const U *code;} ShaderInfo;
typedef struct {HEAD;U flags,stage;Handle module;const char *entry;const void *specialize;} Stage;
typedef struct {HEAD;U flags;Stage stage;Handle layout,base;int index;} ComputeInfo;
typedef struct {U type,count;} PoolSize;
typedef struct {HEAD;U flags,maxSets,count;const PoolSize *sizes;} PoolInfo;
typedef struct {HEAD;Handle pool;U count;const Handle *layouts;} SetAlloc;
typedef struct {Handle buffer;Size offset,range;} BufferBinding;
typedef struct {HEAD;Handle set;U binding,element,count,typeId;const void *images;const BufferBinding *buffers;const void *views;} Write;
typedef struct {HEAD;U flags,family;} CommandPool;
typedef struct {HEAD;Handle pool;U level,count;} CommandAlloc;
typedef struct {HEAD;U flags;const void *inherit;} Begin;
typedef struct {HEAD;U flags;} Fence;
typedef struct {HEAD;U src,dst;} Barrier;
typedef struct {Size src,dst,size;} Copy;
typedef struct {HEAD;U waits;const Handle *wait;const U *stages;U count;const Handle *cmd;U signals;const Handle *signal;} Submit;
#undef HEAD
