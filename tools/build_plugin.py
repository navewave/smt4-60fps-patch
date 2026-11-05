"""Real-3DS build of the full patch: Luma3DS 3GX plugin + exheader (see src/plugin_main.c).
    python tools/build_plugin.py NAME          (USA 1.0; uses out/NAME_code.ips and out/NAME_code_hw.ips
                                                from tools/build_release.py NAME)
Outputs:
  out/NAME_plugin.3gx       ->  SD:/luma/plugins/<title id>/smt4_60fps.3gx
  out/NAME_exheader_hw.bin  ->  SD:/luma/titles/<title id>/exheader.bin   (bss +0x5000)
  plus out/NAME_code_hw.ips ->  SD:/luma/titles/<title id>/code.ips
--no-ext / --no-gate: test builds without the extension / the frame-time logic gate."""
import sys, os, struct, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib; B = importlib.import_module('build')

ROOT, SFX = B.ROOT, B.SUFFIX
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
NAME = ARGS[0] if ARGS else 'phase9'
NO_EXT, NO_GATE = '--no-ext' in sys.argv, '--no-gate' in sys.argv
TAG = ('_noext' if NO_EXT else '') + ('_nogate' if NO_GATE else '')
FULL_IPS, HW_IPS = f'{ROOT}/out/{NAME}{SFX}_code.ips', f'{ROOT}/out/{NAME}{SFX}_code_hw.ips'
LOAD = 0x07000000                 # plugin memory; PluginHeader (0x100) is written there by the loader
ENTRY = LOAD + 0x100              # .3gx code is loaded here and called
FRAME_BEGIN = 0x101B8C            # the core's "bl hk_frame_begin"
BSS_ADD = 0x5000                  # pages up to 0x4F5000 (ext6 at 0x4F4000 needs the fifth); +0x1000 with a core page

def ips_records(path):
    """[(address, bytes)]; RLE records expanded"""
    d = open(path, 'rb').read(); assert d[:5] == b'PATCH', path
    i, out = 5, []
    while d[i:i + 3] != b'EOF':
        off = int.from_bytes(d[i:i + 3], 'big'); n = int.from_bytes(d[i + 3:i + 5], 'big'); i += 5
        if n == 0: n = int.from_bytes(d[i:i + 2], 'big'); raw = d[i + 2:i + 3] * n; i += 3
        else: raw = d[i:i + n]; i += n
        out.append((B.BASE + off, raw))
    return out

orig = open(B.ORIG, 'rb').read()
def image(path):
    m = bytearray(orig) + bytes(B.MEM_END - B.BASE - len(orig))
    for a, r in ips_records(path): m[a - B.BASE:a - B.BASE + len(r)] = r
    return m
hw, full = image(HW_IPS), image(FULL_IPS)
w_at = lambda m, a: struct.unpack_from('<I', m, a - B.BASE)[0]
def arm_branch(frm, to, link):
    return (0xEB000000 if link else 0xEA000000) | (((to - frm - 8) >> 2) & 0xFFFFFF)
def arm_target(frm, w):
    return frm + 8 + (((w & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4

# ---- 1. extension: what the Azahar code.ips adds to the Real_3DS one -------------
runs, i, n = [], 0, len(hw)
while i < n:
    if hw[i] == full[i]: i += 1; continue
    j = i
    while j < n and (hw[j] != full[j] or any(hw[k] != full[k] for k in range(j, min(j + 16, n)))): j += 1
    runs.append((B.BASE + i, B.BASE + j)); i = j
table = []
for a, e in runs:
    new, exp = bytes(full[a - B.BASE:e - B.BASE]), bytes(hw[a - B.BASE:e - B.BASE])
    pad = (-len(new)) % 4
    table += [a, len(new)] + list(struct.unpack(f'<{(len(new) + pad) // 4}I', new + bytes(pad))) \
                           + list(struct.unpack(f'<{(len(new) + pad) // 4}I', exp + bytes(pad)))
table.append(0)
ext_lo = min(a for a, e in runs if a >= B.BASE + len(orig))
ext_hi = max(e for a, e in runs)
MAP_FIRST, MAP_LAST = ext_lo & ~0xFFF, (ext_hi - 1) & ~0xFFF
text_runs = [(a, e) for a, e in runs if a < B.BASE + len(orig)]
print(f'{B.R["name"]}: extension diff {len(runs)} runs, {sum(e - a for a, e in runs)} bytes; '
      f'pages {MAP_FIRST:#x}-{MAP_LAST + 0x1000:#x}; game-code words: ' + ', '.join(f'{a:#x}+{e - a}' for a, e in text_runs))

# ---- 2. frame-time logic gate ------------------------------------------------------
# The trampoline always sits in the text cave (mapped on every boot).  With a core page
# (regions.py core_page) the core is part of the extension diff: plg_main writes it before it
# patches the gate word, so the gate is looked up in the full image there.
cave_at, cave_end = B.CAVE, B.CAVE_END
TRAMP = cave_end - 8
used = max([a + len(r) for a, r in ips_records(HW_IPS) if cave_at <= a < cave_end], default=cave_at)
assert used <= TRAMP and full[TRAMP - B.BASE:cave_end - B.BASE] == bytes(8), 'no room for the gate trampoline'
core_img, core_at, core_end = (full, B.CORE, B.CORE_END) if B.CORE_IN_PAGE else (hw, cave_at, used)
fb = B.P(FRAME_BEGIN)
HK_FRAME_BEGIN = arm_target(fb, w_at(hw, fb))
assert core_at <= HK_FRAME_BEGIN < core_end
gate = [a for a in range(core_at, core_end - 7, 4) if (w_at(core_img, a), w_at(core_img, a + 4)) == (0xE3A0243F, 0xE5812008)]
assert len(gate) == 1, gate           # hk_task_gate: "mov r2, #0x3F000000 ; str r2, [r1, #8]" (r1 = STATE)
for a, e in runs:
    assert not any(a <= x < e for x in (fb, TRAMP)), 'extension diff touches the gate'
    assert B.CORE_IN_PAGE or not a <= gate[0] < e, 'extension diff touches the gate'
if B.CORE_IN_PAGE:
    BSS_ADD = max(BSS_ADD, (MAP_LAST + 0x1000 - B.R['bss_end'] + 0xFFF) & ~0xFFF)
    assert MAP_FIRST <= B.CORE and B.CORE_END - 0x1000 <= MAP_LAST, 'core page not in the plugin map check'

# ---- config header, compile, link -------------------------------------------------
bd = f'{ROOT}/build/plugin{SFX}'; os.makedirs(bd, exist_ok=True)
words = lambda v: ',\n'.join(', '.join(f'0x{x:08X}u' for x in v[k:k + 8]) for k in range(0, len(v), 8))
open(f'{bd}/plg_cfg.h', 'w').write(
    f'#define EXT_MAP_FIRST 0x{MAP_FIRST:08X}u\n#define EXT_MAP_LAST 0x{MAP_LAST:08X}u\n'
    f'#define PLG_TRAMP 0x{TRAMP:08X}u\n'
    f'#define GATE_CALL 0x{(1 if NO_GATE else fb):08X}u\n#define GATE_CALL_OLD 0x{w_at(hw, fb):08X}u\n'
    f'#define GATE_CALL_NEW 0x{arm_branch(fb, TRAMP, True):08X}u\n'
    f'#define GATE_DT 0x{gate[0]:08X}u\n#define GATE_DT_OLD 0xE3A0243Fu\n#define GATE_DT_NEW 0xE59121C0u\n'
    f'static const u32 EXT_PATCH[] = {{\n{words([0] if NO_EXT else table)}\n}};\n')
sh = lambda *a: subprocess.check_call(list(a))
CC = ['clang', '--target=armv6k-none-eabi', '-marm', '-mfloat-abi=hard', '-mfpu=vfpv2', '-Oz', '-ffreestanding',
      '-fno-builtin', '-fno-math-errno', '-nostdlib', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
      '-ffunction-sections', '-fdata-sections', '-Wall', '-c']
sh(*CC, B.subst_source(f'{ROOT}/src/plugin_main.c'), '-I', bd, '-o', f'{bd}/main.o')
sh(*CC, B.subst_source(f'{ROOT}/src/plugin_gate.c'), '-o', f'{bd}/gate.o')
open(f'{bd}/plg.ld', 'w').write('SECTIONS {\n'
    f'  . = 0x{ENTRY:X}; .plg : {{ *(.plg.start) *(.plg.entry) *(.plg.gate) *(.text*)'
    ' *(.plg.*rodata*) *(.rodata*) *(.data*) *(.plg.*bss*) *(.bss*) *(COMMON) _plg_end = .; }\n'
    '  . = 0x08000000; .plgexe : { *(.plg.exeload) }\n'
    '  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) *(.llvm_addrsig) }\n}\n')
sh(B.LLD, '-o', f'{bd}/plg.elf', '-T', f'{bd}/plg.ld', '-e', 'plg_start', '--no-rosegment', '-N',
   f'{bd}/main.o', f'{bd}/gate.o', '--defsym', f'HkFrameBegin=0x{HK_FRAME_BEGIN:X}')
syms = {p[2]: int(p[0], 16) for p in (l.split() for l in subprocess.check_output(['llvm-nm', f'{bd}/plg.elf']).decode().splitlines()) if len(p) == 3}
assert syms['plg_start'] == ENTRY, hex(syms['plg_start'])
def section(n):
    sh('llvm-objcopy', '-O', 'binary', f'--only-section={n}', f'{bd}/plg.elf', f'{bd}/{n[1:]}.bin')
    return open(f'{bd}/{n[1:]}.bin', 'rb').read()
code, exeload = section('.plg'), section('.plgexe')
code += bytes(syms['_plg_end'] - ENTRY - len(code) + 0x40)    # bss, zeroed in the file
assert exeload[-4:] == struct.pack('<I', 0xE320F000) and len(exeload) <= 32 * 4
exeload += struct.pack('<I', 0xE320F000) * (32 - len(exeload) // 4)   # the loader reads 32 words

# ---- .3gx (Luma3DS sysmodules/rosalina/include/plugin/3gx.h, magic "3GX$0002") ----
tid = int(B.R['tid'], 16)
author, title = b'smt4-60fps\0', b'SMT IV 60 fps (real 3DS)\0'
HDR = 148
off_author = HDR
off_title = off_author + len(author)
off_targets = (off_title + len(title) + 3) & ~3
off_exe = off_targets + 4
off_code = off_exe + len(exeload)
end = off_code + len(code)
flags = (1 << 0) | (1 << 2) | (2 << 4) | (1 << 6) | (1 << 7)
#        exeLoad   2 MiB     console+emu eventsSelfManaged swapNotNeeded
infos = struct.pack('<8I', len(author), off_author, len(title), off_title, 0, 0, 0, 0)
infos += struct.pack('<II', flags, 0) + bytes(16) + bytes(16)                    # flags, exeLoadChecksum, args
exe = struct.pack('<10I', off_code, end, end, len(code), 0, 0, 0, off_exe, 0, 0)
hdr = struct.pack('<QII', 0x3230303024584733, 1, 0) + infos + exe + struct.pack('<II', 1, off_targets) + bytes(12)
assert len(hdr) == HDR
blob = bytearray(hdr) + author + title
blob += bytes(off_targets - len(blob)) + struct.pack('<I', tid & 0xFFFFFFFF) + exeload + code
assert len(blob) == end
out = f'{ROOT}/out/{NAME}{SFX}_plugin{TAG}.3gx'
open(out, 'wb').write(blob)

# ---- exheader for Luma: bss covers the extension pages ---------------------------
exh = bytearray(open(B.EXH, 'rb').read())
bss = struct.unpack_from('<I', exh, 0x3C)[0]; assert bss == B.R['bss'], hex(bss)
struct.pack_into('<I', exh, 0x3C, bss + BSS_ADD)
assert B.R['bss_end'] + BSS_ADD >= MAP_LAST + 0x1000 - 0xFFF, 'exheader bss does not cover the extension pages'
names = [bytes(exh[0x250 + i * 8:0x258 + i * 8]).rstrip(b'\0') for i in range(34)]
if b'ir:rst' not in names:
    slot = names.index(b''); exh[0x250 + slot * 8:0x258 + slot * 8] = b'ir:rst\0\0'
open(f'{ROOT}/out/{NAME}{SFX}_exheader_hw.bin', 'wb').write(exh)
print(f'{out}: {len(blob)} bytes (entry {ENTRY:#x}, ends {syms["_plg_end"]:#x}); '
      f'gate {fb:#x} -> {TRAMP:#x}, dt load {gate[0]:#x}; exheader bss +{BSS_ADD:#x} -> out/{NAME}{SFX}_exheader_hw.bin')
