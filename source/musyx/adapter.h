/* Big-endian RDRAM adapter for the Gauntlet cartridge runtime. */
struct hle_t {uint8_t *ram;uint32_t task,count;};
#define HleVerboseMessage(...) ((void)0)
#define HleWarnMessage(...) ((void)0)
static uint8_t dram_u8(struct hle_t *h,uint32_t p){return h->ram[p&0x7fffff];}
static uint16_t dram_u16(struct hle_t *h,uint32_t p){return dram_u8(h,p)*256+dram_u8(h,p+1);}
static uint32_t dram_u32(struct hle_t *h,uint32_t p){return (uint32_t)dram_u16(h,p)<<16|dram_u16(h,p+2);}
static void put16(struct hle_t *h,uint32_t p,uint16_t v){h->ram[p&0x7fffff]=v>>8;h->ram[(p+1)&0x7fffff]=v;}
static void dram_load_u8(struct hle_t *h,uint8_t*d,uint32_t p,size_t n){for(size_t i=0;i<n;i++)d[i]=dram_u8(h,p+i);}
static void dram_load_u16(struct hle_t *h,uint16_t*d,uint32_t p,size_t n){for(size_t i=0;i<n;i++)d[i]=dram_u16(h,p+i*2);}
static void dram_load_u32(struct hle_t *h,uint32_t*d,uint32_t p,size_t n){for(size_t i=0;i<n;i++)d[i]=dram_u32(h,p+i*4);}
static void dram_store_u16(struct hle_t *h,const uint16_t*d,uint32_t p,size_t n){for(size_t i=0;i<n;i++)put16(h,p+i*2,d[i]);}
static unsigned align(unsigned x,unsigned n){return (x+n-1)&~(n-1);}
