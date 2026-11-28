"""Generate resumable native instructions for Towerbound's interrupt-timed game host.

Usage: python generate.py ROM N64Recomp
N64Recomp must have the supplied Gauntlet patch applied to its pinned release.
Generated code and the synthetic ROM image are private build intermediates.
"""
from pathlib import Path
from collections import defaultdict
import hashlib
import re
import struct
import subprocess
import sys
import tomllib
import tempfile

from make_code_bin import make, SHA256

HERE = Path(__file__).resolve().parent
OUTPUT = HERE.parent / 'native'
PAGE = 512
ROM_HASH = '1cc9cb70686b4785c46fff337ded30a2e1f83f2d63768ad8041e978ad0379ec9'
COMMENTS = re.compile(r'^\s*// 0x([A-Fa-f0-9]{8}):\s+(\w+)[^\n]*\n([ \t]*[^\n]*)', re.M)
SIMPLE = {'lui', 'addiu', 'addu', 'subu', 'or', 'ori', 'and', 'andi', 'xor', 'xori'}
ASSIGN = re.compile(r'ctx->r\d+ = [\w\s(),&^|<>x+\-]+;')
HEADER = '''#include "../cpu_state.h"
#include "../lift.inc"
#define S32(x) ((S)(x))
#define ADD32(a,b) sx((U)(a) + (U)(b))
#define SUB32(a,b) sx((U)(a) - (U)(b))
'''


def code_word(data, offset):
    return struct.unpack_from('>I', data, offset)[0]


def rows(values):
    return '\n'.join(','.join(map(str, values[i:i+20])) + ',' for i in range(0, len(values), 20))


def generate(rom_path, recompiler):
    rom = rom_path.read_bytes()
    if hashlib.sha256(rom).hexdigest() != ROM_HASH:
        raise ValueError('Use Gauntlet Legends (USA), big-endian .z64')
    image = make(rom)
    if hashlib.sha256(image).hexdigest() != SHA256:
        raise ValueError('Unexpected decompressed module image')
    sections = tomllib.loads((HERE / 'gauntlet.syms.toml').read_text())['section']
    words = {}
    simple = {}
    # Include the ignored system routines as well. Their hardware interactions
    # use the Towerbound bus and its TLB and exception state.
    for section in sections:
        for func in section['functions']:
            for pc in range(func['vram'], func['vram'] + func['size'], 4):
                offset = section['rom'] + pc - section['vram']
                words[pc] = code_word(image, offset)

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        (root/'gauntlet_code.bin').write_bytes(image)
        for name in ['gauntlet.toml','gauntlet.syms.toml','audio_ucode.toml']:
            (root/name).write_bytes((HERE/name).read_bytes())
        subprocess.run([str(recompiler.resolve()), 'gauntlet.toml'], cwd=root, check=True)
        (root/'rsp').mkdir()
        rsp_compiler = recompiler.resolve().with_name('RSPRecomp')
        subprocess.run([str(rsp_compiler), 'audio_ucode.toml'], cwd=root, check=True)
        (HERE.parent/'rsp'/'audio_ucode.cpp').write_bytes((root/'rsp'/'audio_ucode.cpp').read_bytes())
        for path in sorted((root/'RecompiledFuncs').glob('*.c')):
            for addr, mnemonic, statement in COMMENTS.findall(path.read_text()):
                if mnemonic not in SIMPLE:
                    continue
                statement = statement.strip()
                if not ASSIGN.fullmatch(statement):
                    continue
                statement = re.sub(r'ctx->r(\d+)', r'reg[\1]', statement)
                if re.search(r'\b(?!ADD32|SUB32|S32)[A-Z]+\d*\(', statement):
                    continue
                simple[words[int(addr,16)]] = statement

    for line in (HERE/'boot_ranges.txt').read_text().splitlines():
        start,end = (int(field,16) for field in line.split())
        for offset in range(start,end,4):
            words[0x80200000 + offset] = code_word(rom, 0x1000 + offset)

    unique = sorted(set(words.values()))
    if len(unique) >= 65535:
        raise ValueError('Native operation indices require a wider type')
    ids = {w:i+1 for i,w in enumerate(unique)}
    OUTPUT.mkdir(exist_ok=True)
    for old in OUTPUT.glob('group_*.c'):
        old.unlink()
    batches = [([None]+unique)[i:i+PAGE] for i in range(0,len(unique)+1,PAGE)]
    for i,batch in enumerate(batches):
        funcs, entries = [], []
        for j,w in enumerate(batch):
            if w is None:
                entries.append('{0,0}')
                continue
            if w in simple:
                body = 'delay_slot=0; pc=next; next+=4; ' + simple[w] + ' reg[0]=0;'
            else:
                body = f'execute_word(at,0x{w:08x}u);'
            funcs.append(f'static void op_{j}(U at){{ {body} }}')
            signature = ((w ^ 0xa7c59463) * 0x9e3779b1) & 0xffffffff
            entries.append(f'{{0x{signature:08x}u,op_{j}}}')
        table = f'const NativeOp native_group_{i}[]={{\n' + ',\n'.join(entries)+'\n};\n'
        (OUTPUT/f'group_{i:03}.c').write_text(HEADER+'\n'.join(funcs)+'\n'+table)

    pages = defaultdict(lambda:[0]*1024)
    for pc,w in words.items():
        pages[pc>>12][(pc>>2)&1023] = ids[w]
    lookup = [f'extern const NativeOp native_group_{i}[];' for i in range(len(batches))]
    lookup += ['static const NativeOp *const groups[]={'+','.join(f'native_group_{i}' for i in range(len(batches)))+'};']
    for tag, page in sorted(pages.items()):
        lookup.append(f'static const uint16_t page_{tag:x}[1024]={{\n'+rows(page)+'\n};')
    lookup.append('static const uint16_t *native_page(U addr){switch(addr>>12){')
    lookup.extend(f'case 0x{tag:x}:return page_{tag:x};' for tag in sorted(pages))
    lookup.append('default:return 0;}}')
    # An exception vector or a copied routine can execute from a different
    # address. Resolve it to the same precompiled operation by exact signature.
    slots = 1 << (len(unique)*2).bit_length()
    by_signature = [0]*slots
    for word, op_id in ids.items():
        signature = ((word ^ 0xa7c59463)*0x9e3779b1) & 0xffffffff
        slot = (signature ^ (signature >> 16)) & (slots-1)
        while by_signature[slot]:
            slot=(slot+1) & (slots-1)
        by_signature[slot]=op_id
    lookup.append(f'static const uint16_t copied_ops[{slots}]={{\n'+rows(by_signature)+'\n};')
    lookup.append('''
U native_steps, copied_steps, tower_steps, missing_steps;
static int static_execute(U at, U word){
    U addr=at;
    if ((at & 0xffe00000u) == 0x80000000u) addr=0xe0000000u | (at & 0x1fffffu);
    /* The guest can rewrite code or remap a virtual address. Match both the
       address and the fetched word before reusing a compiled operation. */
    static struct CachedOp { U addr, word; const NativeOp *op; unsigned copied; } cache[4096];
    struct CachedOp *entry=&cache[(addr>>2)&4095];
    if(entry->addr==addr && entry->word==word && entry->op){
        const NativeOp *hit=entry->op;
        copied_steps+=entry->copied;
        hit->run(at);
        native_steps++;
        if(addr>=0xe0860000u && addr<0xe0870000u)tower_steps++;
        return 1;
    }
    unsigned copied=0;
    const uint16_t *page=native_page(addr);
    unsigned id=page ? page[(addr>>2)&1023] : 0;
    U signature=(word^0xa7c59463u)*0x9e3779b1u;
    const NativeOp *op=&groups[id/512][id%512];
    if (!id || op->signature!=signature){
        const U mask=sizeof(copied_ops)/sizeof(*copied_ops)-1;
        U slot=(signature^(signature>>16)) & mask;
        for(;;){
            id=copied_ops[slot];
            if(!id) return 0;
            op=&groups[id/512][id%512];
            if(op->signature==signature) break;
            slot=(slot+1)&mask;
        }
        copied_steps++;
        copied=1;
    }
    *entry=(struct CachedOp){addr,word,op,copied};
    op->run(at);
    native_steps++;
    if (addr>=0xe0860000u && addr<0xe0870000u) tower_steps++;
    return 1;
}
''')
    (HERE.parent/'static_lookup.inc').write_text('\n'.join(lookup))
    print(f'{len(words)} mapped instruction slots, {len(unique)} unique native translations, '
          f'{len(simple)} scalar translations adapted from N64Recomp.')

if __name__ == '__main__':
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
