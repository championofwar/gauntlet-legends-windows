"""Exercise the renderer's creation, fallback and shutdown paths under the PE host."""
from pe_host import *

assert '--two-threads' in sys.argv
call('raster_worker_stop')
imports={imp.name:imp.address for dll in pe.DIRECTORY_ENTRY_IMPORT for imp in dll.imports}
slot=C.c_void_p.from_address(imports[b'CreateThread'])
real=slot.value
slot.value=C.cast(lib.zero,C.c_void_p).value
assert not call('raster_worker_start')
call('raster_worker_stop')
slot.value=real
for i in range(24):
    assert call('raster_worker_start')==1
    assert call('raster_worker_start')==1
    call('raster_worker_stop')
print('Worker creation failure, restart and repeated shutdown passed')
