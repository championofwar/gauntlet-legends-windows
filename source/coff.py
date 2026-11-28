import ctypes as C,re,sys
from pathlib import Path
lib=C.CDLL('libLLVM.so.19.1')
P=C.c_void_p; S=C.c_char_p; I=C.c_int; out=C.POINTER(P)
def api(name,ret,*args):
 f=getattr(lib,'LLVM'+name);f.restype=ret;f.argtypes=args;return f
for unit in ['TargetInfo','Target','TargetMC','AsmPrinter','AsmParser']:
 api('InitializeX86'+unit,None)()
tr=b'x86_64-w64-windows-gnu'
m=api('ModuleCreateWithName',P,S)(b'game')
api('SetTarget',None,P,S)(m,tr)
s=Path(sys.argv[1]).read_text()
s=re.sub(r'^\s*\.(file|size|type|ident|local|hidden)\b[^\n]*','',s,flags=re.M)
s=re.sub(r'^[ \t]*\.section\s+\.note[^\n]*','',s,flags=re.M)
s=re.sub(r'^[ \t]*\.section\s+\.rodata[^\n]*','\t.section .rdata,"dr"',s,flags=re.M)
s=re.sub(r'^[ \t]*\.section\s+\.data\.rel\.ro[^\n]*', '\t.section .rdata,"dr"', s, flags=re.M)
s=s.replace('@PLT','').replace('.string','.asciz')
# Keep each initialized object in its original section. Emit common storage
# together at the end so a declaration cannot redirect later data into code.
common=[]
def reserve(m):
 common.append('.balign '+m[3]+'\n'+m[1]+': .space '+m[2])
 return ''
s=re.sub(r'^[ \t]*\.comm[ \t]+([^,]+),(\d+),(\d+)',reserve,s,flags=re.M)
s+='\n.bss\n'+'\n'.join(common)+'\n'
s=s.encode()
api('SetModuleInlineAsm2',None,P,S,C.c_size_t)(m,s,len(s))
t=P();err=P()
assert not api('GetTargetFromTriple',I,S,out,out)(tr,C.byref(t),C.byref(err))
tm=api('CreateTargetMachine',P,P,S,S,S,I,I,I)(t,tr,b'x86-64',b'',2,1,0)
assert not api('TargetMachineEmitToFile',I,P,P,S,I,out)(tm,m,sys.argv[2].encode(),1,C.byref(err))
