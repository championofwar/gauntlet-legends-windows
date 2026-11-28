#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "rsp_vu.hpp"

// Towerbound stores both guest RAM and signal-processor DMEM in cartridge byte order.
extern uint8_t dmem[4096];
extern uint16_t rspReciprocals[512], rspInverseSquareRoots[512];
enum class RspExitReason { Broke, ImemOverrun, UnhandledJumpTarget };
#define RSP_MEM_BU(offset, addr) dmem[((offset)+(addr))&4095u]
#define RSP_MEM_B(offset, addr) reinterpret_cast<int8_t&>(RSP_MEM_BU(offset,addr))
static inline uint32_t RSP_MEM_HU_LOAD(uint32_t o,uint32_t a){
    return uint32_t(RSP_MEM_BU(o,a))<<8 | RSP_MEM_BU(o+1,a);
}
static inline uint32_t RSP_MEM_W_LOAD(uint32_t o,uint32_t a){
    return RSP_MEM_HU_LOAD(o,a)<<16 | RSP_MEM_HU_LOAD(o+2,a);
}
static inline void RSP_MEM_H_STORE(uint32_t o,uint32_t a,uint32_t v){
    RSP_MEM_BU(o,a)=v>>8; RSP_MEM_BU(o+1,a)=v;
}
static inline void RSP_MEM_W_STORE(uint32_t o,uint32_t a,uint32_t v){
    RSP_MEM_H_STORE(o,a,v>>16); RSP_MEM_H_STORE(o+2,a,v);
}
#define RSP_ADD32(a,b) ((uint32_t)(a)+(uint32_t)(b))
#define RSP_SUB32(a,b) ((uint32_t)(a)-(uint32_t)(b))
#define RSP_SIGNED(v) ((int32_t)(v))
#define SET_DMA_MEM(v) dma_mem_address=(v)
#define SET_DMA_DRAM(v) dma_dram_address=(v)
static inline void audio_dma(uint8_t *ram,uint32_t mem,uint32_t dram, uint32_t control,bool write){
    mem &= 0xff8; dram &= 0x7ffff8;
    uint32_t len=((control & 4095)|7)+1, count=((control>>12)&255)+1, skip=control>>20;
    for(uint32_t row=0;row<count;row++){
        for(uint32_t i=0;i<len;i++){
            if(write)ram[(dram+i)&0x7fffff]=dmem[(mem+i)&4095];
            else dmem[(mem+i)&4095]=ram[(dram+i)&0x7fffff];
        }
        mem+=len; dram+=len+skip;
    }
}
#define DO_DMA_READ(v) audio_dma(rdram,dma_mem_address,dma_dram_address,(v),false)
#define DO_DMA_WRITE(v) audio_dma(rdram,dma_mem_address,dma_dram_address,(v),true)
#define S32(v) ((int32_t)(v))
#define U32(v) ((uint32_t)(v))
