#ifndef TOWERBOUND_CPU_STATE_H
#define TOWERBOUND_CPU_STATE_H
#include <stdint.h>
typedef uint32_t U;
typedef int32_t S;
typedef uint64_t Q;
typedef struct { U hi,lo[2],mask; } TlbEntry;
typedef struct { U tag,base; } PageEntry;
extern Q reg[32],hi,lo;
extern U dispatches;
extern U cp0[32],pc,next,fp[32],reservation,fcsr,stop_address;
extern TlbEntry tlb[32];
extern PageEntry pages[128];
extern int delay_slot,status;
enum { RUN, OPCODE, BUS, LIMIT, ALIGNMENT };
U physical(U addr);
U access_bus(U addr,unsigned size,U value,int write);
void exception(U cause,U extra);
void floating(U fmt,U t,U d,U k,U f);
static inline Q sx(U value){ return (Q)(int64_t)(S)value; }
typedef struct { U signature; void (*run)(U at); } NativeOp;
#endif
