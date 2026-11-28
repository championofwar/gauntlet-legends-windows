from pe_host import *

pic = (C.c_uint32*(832*220*9))()
shown=0
last_dump=-1
targets={30,300,800,1070,1200,1380,1500,1620,1700,1740,1780,1850,1940,2020,2100}
targets.update({2160,2260,2360,2460,2560,2600,2640,2680,2720,2800,2900,3000,3100,3200})
pending=iter(sorted(targets))
target=next(pending)
while target is not None:
    press=240<=shown<1580 and shown%60<5
    start=(shown//60)%3==0
    if 2110<=shown<2540 and shown%45<5:
        press=True
        start=False
    axis=0 if neutral else (55 if 1700<=shown<1780 else -45 if 1850<=shown<1940 else 75 if 2600<=shown<2740 else -70 if 2820<=shown<2920 else 0)
    if native:
        lib.input((16 if start else 4096) if press else 0,axis*400,0)
        call('controller',1)
    else:
        call('cart_pad',(0x1000 if start else 0x8000) if press else 0,axis,0)
    result=call('cart_slice',1000000)
    if result:raise RuntimeError(f'Runtime stop {result} at {value("stop_pc"):08x}')
    shown=value('scan_id')
    if shown<target:continue
    target=next(pending,None)
    last_dump=shown
    call('cart_picture',C.addressof(pic))
    width=call('cart_picture_width')
    pixels=C.string_at(pic,width*220*4)
    path=out/f'{shown:04}.png'
    Image.frombytes('RGB',(width,220),pixels,'raw','BGRX').save(path)
    digest=hashlib.sha256(pixels).hexdigest()[:16]
    print(shown,value('instructions'),digest,flush=True)
print('audio',C.c_uint.in_dll(lib,'submissions').value,C.c_uint.in_dll(lib,'nonzero_buffers').value,
      hex(C.c_uint64.in_dll(lib,'pcm_hash').value))
print('boot',value('entered'),value('main_entered'),hex(value('module_crc')))
if native:
    print('dispatch',value('native_steps'),value('copied_steps'),value('missing_steps'),value('tower_steps'))
