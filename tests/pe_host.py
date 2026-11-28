"""Run packaged PE CPU, display, audio and pad functions against simulated devices on Linux x64."""
import ctypes as C
import hashlib
from pathlib import Path
import subprocess
import sys

import pefile
from PIL import Image

exe, rom, out = map(Path, sys.argv[1:4])
neutral = '--neutral' in sys.argv[4:]
out.mkdir(exist_ok=True, parents=True)
lib = C.CDLL(str(Path(__file__).resolve().with_name('bridge.so')))
lib.call_win.argtypes = [C.c_void_p]+[C.c_size_t]*4
lib.call_win.restype = C.c_size_t
pe = pefile.PE(str(exe))
base = pe.OPTIONAL_HEADER.ImageBase
length = pe.OPTIONAL_HEADER.SizeOfImage
libc=C.CDLL(None,use_errno=True)
libc.mmap.argtypes=[C.c_void_p,C.c_size_t,C.c_int,C.c_int,C.c_int,C.c_long]
libc.mmap.restype=C.c_void_p
if libc.mmap(base,length,7,0x100022,-1,0)!=base:
    raise OSError(C.get_errno(),'Cannot map PE at its preferred address')
data = pe.get_memory_mapped_image(max_virtual_address=length)
C.memmove(base,data,len(data))
adapters = {'waveOutOpen':'audio_open', 'waveOutWrite':'audio_write','GetForegroundWindow':'foreground',
            'XInputGetState':'get_pad'}
if '--two-threads' in sys.argv:
    adapters.update(CreateThread='thread_create',CreateEventW='event_create',SetEvent='event_set',
                    WaitForSingleObject='handle_wait',CloseHandle='handle_close')
zero = {'waveOutPrepareHeader','waveOutUnprepareHeader','waveOutPause','waveOutRestart','waveOutClose',
        'waveOutReset','XInputSetState','GetAsyncKeyState','GetTickCount','GetModuleFileNameW'}
for entry in pe.DIRECTORY_ENTRY_IMPORT:
    for imp in entry.imports:
        name=imp.name.decode()
        fn=adapters.get(name,'zero' if name in zero else 'unexpected')
        C.c_void_p.from_address(imp.address).value=C.cast(getattr(lib,fn),C.c_void_p).value

symbols={}
for line in subprocess.check_output(['nm','-an',str(exe)],text=True).splitlines():
    parts=line.split()
    if len(parts)==3:
        symbols[parts[2]]=int(parts[0],16)
def call(name,*args):
    return lib.call_win(symbols[name],*args,*([0]*(4-len(args))))
def value(name):
    return C.c_uint32.from_address(symbols[name]).value
raw = rom.read_bytes()
if len(raw)!=1<<24:raise ValueError('Expected a 16 MiB ROM')
C.memmove(symbols['rom'],raw,len(raw))
assert call('validate_rom')==0
byte = C.c_ubyte.from_address(symbols['rom']+65535)
byte.value ^= 1
assert call('validate_rom')!=0
byte.value ^= 1
native = 'native_steps' in symbols
if native:call('read_controls')
call('cart_start',symbols['rom'])

if '--two-threads' in sys.argv:
    import atexit
    assert call('raster_worker_start')==1
    atexit.register(call,'raster_worker_stop')
