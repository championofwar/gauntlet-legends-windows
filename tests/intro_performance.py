"""Replay the opening cinematic with widescreen geometry and compare CPU time and scanouts."""
from pe_host import *
from time import process_time,perf_counter
from statistics import median,quantiles
scan_intervals=[]

call('cart_aspect',1920,1080)
C.c_uint32.from_address(symbols['hd_height']).value=1440
for target in range(60,1861,60):
    start=process_time()
    last=perf_counter()
    while value('scan_id')<target:
        scan=value('scan_id')
        confirm=any(first<=scan<first+5 for first in (90,190,300))
        lib.input(4096 if confirm else 0,0,0)
        call('controller',1)
        if call('cart_slice',1000000):
            raise RuntimeError(f'Stopped during intro at scan {scan}')
        if value('scan_id')!=scan:
            now=perf_counter()
            if 1320<=scan<1860:
                scan_intervals.append(1000*(now-last))
            last=now
    elapsed=process_time()-start
    pixels=value('capture_width')*220
    digest=hashlib.sha256()
    for name,count in [('enhanced',pixels*4),('screen_owner',pixels*2)]:
        digest.update(C.string_at(symbols[name],count))
    print(target,round(elapsed,4),digest.hexdigest()[:16],flush=True)
print('audio',hex(C.c_uint64.in_dll(lib,'pcm_hash').value),'missing',value('missing_steps'))

print('Intro scan intervals (wall ms):',len(scan_intervals),'total',round(sum(scan_intervals)),
      'median',round(median(scan_intervals),2),'p95',round(quantiles(scan_intervals,n=100)[94],2),
      'p99',round(quantiles(scan_intervals,n=100)[98],2),'max',round(max(scan_intervals),2))
if '--two-threads' in sys.argv:
    print('Split triangles',value('split_triangles'))
    assert value('split_triangles')>0
