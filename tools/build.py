#!/usr/bin/env python3
"""Build SMT IV 60fps patch: cave code + hook sites -> IPS, Azahar cheat, patched code.bin."""
import struct, subprocess, sys, os, json

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = f'{ROOT}/game/v10/decompressed_code.bin'
BASE = 0x100000
CAVE = 0x3FE628
CAVE_END = 0x3FF000
EXT = 0x4F0000        # Azahar-only extension: last bss page (IPS-patchable on Azahar, never zeroed)
EXT_END = 0x4F0F00    # 0x4F0F00-0x4F1000: extension data (CBREC, diag bytes)
EXT2 = 0x42AB80       # second Azahar-only region: zero tail of the rodata page
EXT2_END = 0x42B000
EXT3 = 0x4F1580       # third region, inside the camera page after the camera code (exists only with the camera exheader)
EXT3_END = 0x4F1E80    # 0x4F1E80-0x4F1F00: BCAM data; 0x4F1F00: camera state
EXT4 = 0x4F2000       # fourth region: second extra page (exheader bss +0x2000)
EXT4_END = 0x4F2E00    # 0x4F2E00-0x4F3000: scale records (SCL)
EXT5 = 0x4F3000       # fifth region: third extra page (exheader bss +0x3000)
EXT5_END = 0x4F3C00    # 0x4F3C00-0x4F4000: ext5 data (portrait records 0x4F3C00-0x4F3F90)
MEM_END = 0x4F4000    # Azahar resizes code.bin to cover the bss pages before applying the IPS
LLD = 'ld.lld.exe' if os.name == 'nt' else 'ld.lld'   # Windows does not add .exe to a name that has a dot

# ---- game version -------------------------------------------------------
# SMT4_VER=1.1 builds for the 1.1 update: every game address in the sources and
# build scripts is translated with v11/addrmap.json (made by v11/mapall.py:
# code matched by masked instruction windows, data by literal-pool votes).
VER = os.environ.get('SMT4_VER', '1.0')
EXH = f'{ROOT}/game/v10/exheader.bin'
ADDRMAP = {}
if VER == '1.1':
    ORIG = f'{ROOT}/game/v11/decompressed_code.bin'
    EXH = f'{ROOT}/game/v11/exheader.bin'
    CAVE = 0x3FE3F8           # 1.1 .text ends at 0x3FE3F8
    EXT2 = 0x42AF30           # 1.1 .rodata ends at 0x42AF30
    ADDRMAP = {int(k, 16): int(v, 16) for k, v in json.load(open(f'{ROOT}/v11/addrmap.json')).items()}
    ADDRMAP.update({0x103B58: 0x103BD4, 0x103B5C: 0x103BD8, 0x2B9438: 0x2B90F8, 0x102438: 0x102438, 0x10B578: 0x10B5E8})
SUFFIX = '' if VER == '1.0' else '_v11'

def P(a):
    """1.0 game address -> address in the selected version"""
    if VER == '1.0' or not (0x100000 <= a < 0x4F0000): return a
    if a in ADDRMAP: return ADDRMAP[a]
    if a >= 0x42B000: return a            # .data/.bss layout is unchanged in 1.1
    raise SystemExit(f'no 1.1 mapping for {a:#x}')

def subst_source(path):
    """copy of a source file with its game addresses translated"""
    if VER == '1.0': return path
    import re
    txt = open(path).read()
    def rep(m):
        v = int(m.group(2), 16)
        if v in ADDRMAP and ADDRMAP[v] != v:
            return m.group(1) + ('%0*X' % (len(m.group(2)), ADDRMAP[v]))
        return m.group(0)
    txt = re.sub(r'(0[xX])([0-9A-Fa-f]{6,8})(?![0-9A-Fa-f])', rep, txt)
    os.makedirs(f'{ROOT}/build/v11src', exist_ok=True)
    out = f'{ROOT}/build/v11src/' + os.path.basename(path)
    open(out, 'w').write(txt)
    return out

def masked_same(w1, w2):
    if (w1 >> 25) & 7 == 5: return (w1 >> 24) == (w2 >> 24)   # B/BL: same kind, target differs
    return w1 == w2

# game functions referenced from the cave
GAME_SYMS = {
    'InputUpdate':      0x10B578,
    'ScreenFadeUpdate': 0x102438,
}

def encode_branch(src, dst, link=True, cond=0xE):
    off = dst - (src + 8)
    assert off % 4 == 0 and -(1 << 25) <= off < (1 << 25), (hex(src), hex(dst))
    return (cond << 28) | (0b101 << 25) | ((1 if link else 0) << 24) | ((off >> 2) & 0xFFFFFF)

def build(phase_src, sites, out_prefix, extra_raw=()):
    os.makedirs(f'{ROOT}/build', exist_ok=True)
    out_prefix += SUFFIX
    elf = f'{ROOT}/build/{out_prefix}.elf'
    srcs = phase_src if isinstance(phase_src, (list, tuple)) else [phase_src]
    objs = []
    if VER != '1.0': subst_source(f'{ROOT}/patch/interp.h')
    for i, src in enumerate(srcs):
        obj = f'{ROOT}/build/{out_prefix}.{i}.o'
        src = subst_source(src)
        cmd = ['clang', '--target=armv6k-none-eabi', '-marm', '-mfloat-abi=hard', '-mfpu=vfpv2', '-c', src, '-o', obj]
        if src.endswith('.c'):
            if src.endswith('_thumb.c'): cmd[cmd.index('-marm')] = '-mthumb'
            cmd[1:1] = os.environ.get('SMT4_CFLAGS', '').split() + ['-Oz', '-ffreestanding', '-fno-builtin', '-fno-math-errno', '-nostdlib', '-fno-unwind-tables',
                        '-fno-asynchronous-unwind-tables', '-ffunction-sections', '-Wall']
        subprocess.check_call(cmd)
        objs.append(obj)
    lds = f'{ROOT}/build/cave.ld'
    open(lds, 'w').write('SECTIONS { . = 0x%X; .text : { *(.text.cave) *(.text*) *(.rodata*) } . = 0x%X; .ext2 : { *(.ext2.text*) *(.ext2.rodata*) } . = 0x%X; .ext : { *(.ext.text*) *(.ext.rodata*) } . = 0x%X; .ext3 : { *(.ext3.text*) *(.ext3.rodata*) } . = 0x%X; .ext4 : { *(.ext4.text*) *(.ext4.rodata*) } . = 0x%X; .ext5 : { *(.ext5.text*) *(.ext5.rodata*) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) } }\n' % (CAVE, EXT2, EXT, EXT3, EXT4, EXT5))
    ld = [ LLD, '-o', elf, '-T', lds, '-e', '0', '--no-rosegment', '-N' ] + objs
    for k, v in GAME_SYMS.items():
        ld += ['--defsym', f'{k}=0x{P(v):X}']
    subprocess.check_call(ld)
    syms = {}
    for line in subprocess.check_output(['llvm-nm', elf]).decode().split('\n'):
        p = line.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.text', elf, f'{ROOT}/build/{out_prefix}.cave.bin'])
    # sanity: no writable data / bss sections may be emitted (cave is in .text)
    secs = subprocess.check_output(['llvm-objdump', '-h', elf]).decode()
    for bad in ('.data', '.bss'):
        for line in secs.split('\n'):
            parts = line.split()
            if len(parts) > 2 and parts[1].startswith(bad) and int(parts[2], 16) != 0:
                raise SystemExit(f'cave build emitted {parts[1]} section')
    cave = open(f'{ROOT}/build/{out_prefix}.cave.bin', 'rb').read()
    assert CAVE + len(cave) <= CAVE_END, 'cave overflow (%d bytes over)' % (CAVE + len(cave) - CAVE_END)
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.ext', elf, f'{ROOT}/build/{out_prefix}.ext.bin'])
    ext = open(f'{ROOT}/build/{out_prefix}.ext.bin', 'rb').read()
    assert EXT + len(ext) <= EXT_END, 'ext overflow (%d bytes over)' % (EXT + len(ext) - EXT_END)
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.ext2', elf, f'{ROOT}/build/{out_prefix}.ext2.bin'])
    ext2 = open(f'{ROOT}/build/{out_prefix}.ext2.bin', 'rb').read() if os.path.exists(f'{ROOT}/build/{out_prefix}.ext2.bin') else b''
    assert EXT2 + len(ext2) <= EXT2_END, 'ext2 overflow (%d bytes over)' % (EXT2 + len(ext2) - EXT2_END)
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.ext3', elf, f'{ROOT}/build/{out_prefix}.ext3.bin'])
    ext3 = open(f'{ROOT}/build/{out_prefix}.ext3.bin', 'rb').read()
    assert EXT3 + len(ext3) <= EXT3_END, 'ext3 overflow'
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.ext4', elf, f'{ROOT}/build/{out_prefix}.ext4.bin'])
    ext4 = open(f'{ROOT}/build/{out_prefix}.ext4.bin', 'rb').read()
    assert EXT4 + len(ext4) <= EXT4_END, 'ext4 overflow'
    subprocess.check_call(['llvm-objcopy', '-O', 'binary', '--only-section=.ext5', elf, f'{ROOT}/build/{out_prefix}.ext5.bin'])
    ext5 = open(f'{ROOT}/build/{out_prefix}.ext5.bin', 'rb').read()
    assert EXT5 + len(ext5) <= EXT5_END, 'ext5 overflow'
    data = bytearray(open(ORIG, 'rb').read())
    orig_len = len(data)
    data += bytes(MEM_END - BASE - len(data))      # bss pages as Azahar's loader sees them
    assert data[CAVE - BASE: CAVE - BASE + len(cave)] == bytes(len(cave)), 'cave not empty'
    assert data[EXT - BASE: EXT - BASE + len(ext)] == bytes(len(ext)), 'ext area not empty'
    assert data[EXT2 - BASE: EXT2 - BASE + len(ext2)] == bytes(len(ext2)), 'ext2 area not empty'
    print(f'core {len(cave)}/{CAVE_END-CAVE} bytes, ext {len(ext)}/{EXT_END-EXT} bytes, ext2 {len(ext2)}/{EXT2_END-EXT2} bytes, ext3 {len(ext3)}/{EXT3_END-EXT3}, ext4 {len(ext4)}/{EXT4_END-EXT4}, ext5 {len(ext5)}/{EXT5_END-EXT5} bytes')

    patches = []  # (addr, bytes)
    patches.append((CAVE, cave))
    emu_addrs = set()
    if ext: patches.append((EXT, ext)); emu_addrs.add(EXT)
    if ext2: patches.append((EXT2, ext2)); emu_addrs.add(EXT2)
    if ext3: patches.append((EXT3, ext3)); emu_addrs.add(EXT3)
    if ext4: patches.append((EXT4, ext4)); emu_addrs.add(EXT4)
    if ext5: patches.append((EXT5, ext5)); emu_addrs.add(EXT5)
    for s in sites:
        addr, kind = P(s['addr']), s['kind']
        orig = struct.unpack_from('<I', data, addr - BASE)[0]
        if VER == '1.0':
            assert orig == s['expect'], f"site {addr:#x}: expected {s['expect']:#010x} got {orig:#010x}"
        else:
            assert masked_same(s['expect'], orig), f"site {s['addr']:#x}->{addr:#x}: 1.0 {s['expect']:#010x} vs 1.1 {orig:#010x}"
        if kind in ('bl', 'b'):
            w = encode_branch(addr, syms[s['to']], kind == 'bl', s.get('cond', 0xE))
        elif kind == 'raw':
            w = s['word']
        patches.append((addr, struct.pack('<I', w)))
        if s.get('emu'): emu_addrs.add(addr)
    for addr, raw in extra_raw:
        patches.append((addr, raw))
    for addr, raw in patches:
        data[addr - BASE: addr - BASE + len(raw)] = raw
    open(f'{ROOT}/build/{out_prefix}_code.bin', 'wb').write(data)
    build.emu_addrs = emu_addrs
    build.orig_len = orig_len
    return patches, syms, cave

def hw_patches(patches):
    """Patches safe for real hardware: no extension region, no emulator-only sites."""
    out = [p for p in patches if p[0] not in build.emu_addrs]
    assert all(a - BASE + len(r) <= build.orig_len for a, r in out)
    return out

def write_ips(patches, path):
    out = bytearray(b'PATCH')
    for addr, raw in patches:
        off = addr - BASE
        for i in range(0, len(raw), 0xFFFF):
            chunk = raw[i:i + 0xFFFF]
            o = off + i
            assert o != 0x454F46  # 'EOF'
            out += struct.pack('>I', o)[1:] + struct.pack('>H', len(chunk)) + chunk
    out += b'EOF'
    open(path, 'wb').write(out)

def write_cheat(patches, path, title, notes, cheat_only=()):
    lines = [f'[{title}]', '*citra_enabled']
    for n in notes:
        lines.append(f'# {n}') if False else None
    lines.append('D3000000 00000000')
    # cave first (E type block), then hook sites (32-bit writes) so hooks never
    # point at code that is not there yet
    for addr, raw in sorted(patches, key=lambda p: (len(p[1]) <= 4, p[0])):
        if len(raw) > 4:
            raw = raw + bytes((-len(raw)) % 8)
            lines.append(f'E{addr:07X} {len(raw):08X}')
            for i in range(0, len(raw), 8):
                a, b = struct.unpack_from('<II', raw, i)
                lines.append(f'{a:08X} {b:08X}')
        else:
            lines.append(f'0{addr:07X} {struct.unpack("<I", raw)[0]:08X}')
    for addr, word in cheat_only:
        lines.append(f'0{addr:07X} {word:08X}')
    lines.append('D2000000 00000000')
    open(path, 'w').write('\n'.join(lines) + '\n')
