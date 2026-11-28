"""Cold boot, choose a Wizard, take the mountain portal and walk in Mountain Valley."""
from pe_host import *

picture = (C.c_uint32*(832*220*9))()
shown=0
def advance(target,x=0,y=0,button=0,join=False):
    global shown
    while shown<target:
        buttons=button
        if join:
            if 240<=shown<1580 and shown%60<5:buttons=0x1000 if shown//60%3==0 else 0x8000
            if 2110<=shown<2540 and shown%45<5:buttons=0x8000
        if native:
            lib.input((16 if buttons&0x1000 else 0)|(4096 if buttons&0x8000 else 0),x*400,y*400)
            call('controller',1)
        else:
            call('cart_pad',buttons,x,y)
        if call('cart_slice',1000000 if join else 500000):
            raise RuntimeError(f"Runtime stopped at frame {shown}, PC {value('stop_pc'):08X}, word {value('stop_word'):08X}")
        shown=value('scan_id')
    call('cart_picture',C.addressof(picture))
    size=(call('cart_picture_width'),220)
    data=C.string_at(picture,size[0]*size[1]*4)
    Image.frombytes('RGB',size,data,'raw','BGRX').save(out/f'{target}.png')
    print(shown,hashlib.sha256(data).hexdigest()[:16],flush=True)

if '--hd-capture' in sys.argv:
    call('cart_aspect',1920,1080)
    C.c_uint32.from_address(symbols['hd_height']).value=1440
advance(2560,join=True)
advance(3160,55,55)
advance(3335,0,80)
advance(3455,-55,-55)
advance(3505,-80,0)
advance(3510,button=0x8000)
advance(3770)
advance(4170)
advance(4175,button=0x8000)
advance(4335,30,65)
advance(4600,0,65,0x8000)
print('audio buffers',C.c_uint.in_dll(lib,'submissions').value,'non-silent',C.c_uint.in_dll(lib,'nonzero_buffers').value)
if native:
    print('missing',value('missing_steps'),'native audio tasks',value('native_audio_tasks'),'other audio tasks',value('legacy_audio_tasks'))
    assert value('missing_steps')==0 and value('native_audio_tasks')>0
