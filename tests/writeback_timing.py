"""Check cache maintenance boundaries and guest state against the previous PE."""
from pe_host import *
from struct import pack
entry=0xe0099500
physical=0x99500
page=entry>>12
pages=(C.c_uint32*256).from_address(symbols['pages'])
regs=(C.c_uint64*32).from_address(symbols['reg'])
cp=(C.c_uint32*32).from_address(symbols['cp0'])
def set_u(name,n):C.c_uint32.from_address(symbols[name]).value=n
code=pack('>6I',0xbd010000,0x0109082b,0x1420fffd,0x25080010,0x03e00008,0)
C.memmove(symbols['ram']+physical,code,len(code))
digest=hashlib.sha256()
for tick,compare,cursor,budget in ((2,0,0,1),(2,0,0,4),(3,0,0,501),
                                   (2010,0,128,280),(2000,7,0,256),(21,9,0x1fc0,12),
                                   (2046,24,0x1fe0,15),(2,88,0,2047)):
    for name,n in [('pc',entry),('next',entry+4),('delay_slot',0),('instructions',tick),
                   ('native_steps',0),('status',0),('line',0),('mi_pending',0),
                   ('dp_end',tick+27),('mi_mask',0)]:
        set_u(name,n)
    for i in range(32):regs[i]=0;cp[i]=0
    regs[8]=0xffffffff80000000+cursor
    regs[9]=0xffffffff80001ff0
    regs[31]=entry
    cp[11]=compare
    pages[(page&127)*2]=page+1
    pages[(page&127)*2+1]=physical&~4095
    assert call('cart_slice',budget)==0
    digest.update(bytes(cp));digest.update(bytes(regs))
    for name in ('pc','next','instructions','native_steps','status','line','mi_pending','dp_end',
                 'stop_pc','stop_word','delay_slot'):
        digest.update(pack('<I',value(name)))
print('Guest registers, timer and interrupts',digest.hexdigest())
