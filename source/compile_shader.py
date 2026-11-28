"""Regenerate gpu_spv.inc after editing gpu.comp using libshaderc."""
from pathlib import Path
import ctypes as C
lib=C.CDLL('libshaderc.so.1')
def function(name, result, *args):
    fn=getattr(lib,'shaderc_'+name)
    fn.restype=result
    fn.argtypes=args
    return fn
P=C.c_void_p
S=C.c_char_p
N=C.c_size_t
compiler=function('compiler_initialize',P)()
options=function('compile_options_initialize',P)()
function('compile_options_set_optimization_level',None,P,C.c_int)(options,2)
source=Path('gpu.comp').read_bytes()
result=function('compile_into_spv',P,P,S,N,C.c_int,S,S,P)(compiler,source,len(source),2,b'gpu.comp',b'main',options)
if function('result_get_compilation_status',C.c_int,P)(result):
    raise SystemExit(function('result_get_error_message',S,P)(result).decode())
size=function('result_get_length',N,P)(result)
data=C.string_at(function('result_get_bytes',P,P)(result),size)
words=[f'0x{int.from_bytes(data[i:i+4],"little"):08x}' for i in range(0,size,4)]
Path('gpu_spv.inc').write_text(',\n'.join(','.join(words[i:i+8]) for i in range(0,len(words),8))+'\n')
