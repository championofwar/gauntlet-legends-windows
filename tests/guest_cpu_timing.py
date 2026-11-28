"""Verify F5 edge handling and a timer interrupt due in a guest delay slot."""
from pe_host import *

assert call('cart_extra_cpu_enabled') == 1
for key_flags, expected in ((0,0),(1<<30,0),(0,1),(1<<30,1)):
    call('window_proc',1,0x100,0x74,key_flags)
    assert call('cart_extra_cpu_enabled') == expected

# The delay slot must finish first, then the extra instruction takes the interrupt.
branch = 0x80001000
for name,number in (('pc',branch+4),('next',branch),('delay_slot',1),('instructions',4)):
    C.c_uint32.from_address(symbols[name]).value=number
cp=(C.c_uint32*32).from_address(symbols['cp0'])
cp[9]=0
cp[11]=1
cp[12]=0x8001
C.memset(symbols['ram']+0x1004,0,4)
C.memset(symbols['ram']+0x180,0,4)
assert call('cart_slice',1)==0
assert cp[12]&2 and cp[14]==branch and value('pc')==0x80000184
assert cp[9]==1 and value('instructions')==5
print('F5 toggles and ignores repeats. IRQ delivered after delay slot with one hardware tick.')
