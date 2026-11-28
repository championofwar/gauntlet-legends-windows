from pe_host import *

# Re-enter the exact game routine after the original boot has installed its TLB.
while value('scan_id')<30:
    assert call('cart_slice',1000000)==0
regs=(C.c_uint64*32).from_address(symbols['reg'])
cp0=(C.c_uint32*32).from_address(symbols['cp0'])
cp0[12]&=~1
for name,val in [('pc',0xe00977d0),('next',0xe00977d4),('delay_slot',0)]:
    C.c_uint32.from_address(symbols[name]).value=val
for i in range(1,32):regs[i]=(0x349172d*i)&0xffffffff
src,dst=0x00640000,0x00641000
inputs=bytes.fromhex('7fff80000000000167891111ffffc000')
C.memmove(symbols['ram']+src,inputs,len(inputs))
regs[9]=0x80000000|src
regs[8]=0x80000000|dst
regs[11]=2
regs[31]=0x80000040
for step in range(1000):
    if value('pc')==regs[31]:break
    result=call('cart_slice',1)
    if result:
        if '--expect-stop' in sys.argv:
            assert value('stop_pc')==0xe00977d0 and value('stop_word')==0x852a0000 and value('missing_steps')==1
            print("Reproduced Playtest 01 stop at E00977D0")
            sys.exit(0)
        raise RuntimeError(f"Unexpected stop at {value('stop_pc'):08x}")
else:raise RuntimeError('Routine did not return')
assert '--expect-stop' not in sys.argv
assert (regs[8] & 0xffffffff)==0x80641006 and (regs[9] & 0xffffffff)==0x80640006
print('returned',step,hex(value('pc')),C.string_at(symbols['ram']+dst,6).hex(),
      hashlib.sha256(bytes(regs)).hexdigest())
if native:assert value('missing_steps')==0
