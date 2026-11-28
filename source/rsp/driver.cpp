#include "librecomp/rsp.hpp"
// This freestanding executable reports a non-BREAK exit through its runtime log.
#define printf(...) ((void)0)
#include "audio_ucode.cpp"
#undef printf
alignas(16) uint8_t dmem[4096];
// Gauntlet's audio program does not execute reciprocal or reciprocal-square-root ops.
uint16_t rspReciprocals[512],rspInverseSquareRoots[512];
extern "C" int native_audio_task(uint8_t *ram,const uint8_t *descriptor){
    for(unsigned i=0;i<64;i++)dmem[0xfc0+i]=descriptor[i];
    uint32_t data=RSP_MEM_W_LOAD(0x18,0xfc0);
    audio_dma(ram,0,data,0xf80-1,false);
    return gauntlet_audio_ucode(ram,RSP_MEM_W_LOAD(0x10,0xfc0))==RspExitReason::Broke;
}
