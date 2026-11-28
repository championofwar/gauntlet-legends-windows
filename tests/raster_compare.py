"""Pair single/dual rendering trials from an identical intro state on Linux."""
import os
from statistics import median,quantiles
from time import perf_counter
from pe_host import *

reference='raster_worker_start' not in symbols
if not reference:
    assert '--two-threads' in sys.argv
    call('raster_worker_stop')
call('cart_aspect',1920,1080)
C.c_uint.from_address(symbols['hd_height']).value=1440
def advance(budget):
    scan=value('scan_id')
    confirm=any(first<=scan<first+5 for first in (90,190,300))
    lib.input(4096 if confirm else 0,0,0)
    call('controller',1)
    assert not call('cart_slice',budget)
while value('scan_id')<1320:
    advance(1000000)
for mode in ((1,1) if reference else (1,2,2,1)):
    pid=os.fork()
    if pid:
        _,status=os.waitpid(pid,0)
        if status:raise RuntimeError(f'Child failed: {status}')
        continue
    if mode==2:assert call('raster_worker_start')==1
    times=[]
    while value('scan_id')<1500:
        scan=value('scan_id')
        start=perf_counter()
        while value('scan_id')==scan:
            advance(50000)
        assert value('scan_id')==scan+1
        times.append(1000*(perf_counter()-start))
    span=value('capture_width')*220
    img=hashlib.sha256(C.string_at(symbols['enhanced'],span*4)).hexdigest()[:16]
    print(mode,len(times),round(sum(times)),round(median(times),2),
          round(quantiles(times,n=100)[94],2),round(quantiles(times,n=100)[98],2),
          round(max(times),2),img,hex(C.c_uint64.in_dll(lib,'pcm_hash').value),
          0 if reference else value('split_triangles'),flush=True)
    if not reference:call('raster_worker_stop')
    os._exit(0)
