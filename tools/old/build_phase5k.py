"""Phase 5h: 60 fps (5g + fixes) and the right-stick camera in one code.ips.
Outputs:
  out/phase5k_code.ips        60 fps + camera   (needs out/phase5k_exheader.bin as mods/<tid>/exheader.bin)
  out/phase5k_exheader.bin    bss +0x1000 (camera page 0x4F1000), ir:rst added to the service list
  out/phase5k_60fps_only.ips  60 fps without the camera (no exheader needed)
  out/phase5k_code_hw.ips     hardware-safe subset (no Azahar-only parts)
"""
import sys, struct, subprocess, os
sys.path.insert(0, '/home/claude/smt4/tools')
ns = {}
src = open('/home/claude/smt4/tools/build_phase5g.py').read().replace('phase5g', 'phase5k').replace(
    'Phase 5g (fast slides + info panel, Azahar)', 'Phase 5h (60fps, Azahar)')
exec(src, ns)
B, patches = ns['B'], ns['patches']
ROOT = B.ROOT
os.remove(f'{ROOT}/out/phase5k_code.ips')
B.write_ips([p for p in patches if p[0] < 0x4F1000], f'{ROOT}/out/phase5k_60fps_only.ips')   # no camera page
try: os.remove(f'{ROOT}/out/phase5k_cheat.txt')
except FileNotFoundError: pass

# ---- camera at its own page 0x4F1000 ----
PAGE, CODE_AT, STATE_AT = 0x4F1000, 0x4F1020, 0x4F1F00
bd = f'{ROOT}/build/cam5h'; os.makedirs(bd, exist_ok=True)
sh = lambda *a: subprocess.check_call(list(a))
sh('clang', '--target=armv6k-none-eabi', '-marm', '-mfloat-abi=hard', '-mfpu=vfpv2', '-Oz', '-ffreestanding',
   '-fno-builtin', '-fno-math-errno', '-nostdlib', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
   '-ffunction-sections', '-fdata-sections', '-Wall', '-c', f'{ROOT}/src/cam_stick.c', '-o', f'{bd}/cam.o',
   f'-DCAM_STATE=0x{STATE_AT:X}u', '-DCAM_FLAGS=4')
open(f'{bd}/cam.ld', 'w').write('SECTIONS {\n'
    f'  . = 0x{PAGE:X}; .cammagic : {{ *(.cam.magic) }}\n'
    f'  . = 0x{PAGE+4:X}; .camcfg : {{ *(.cam.cfg) }}\n'
    f'  . = 0x{CODE_AT:X}; .camcode : {{ *(.cam.text*) *(.cam.rodata*) *(.rodata*) *(.text*) }}\n'
    '  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) *(.llvm_addrsig) }\n}\n')
sh('ld.lld', '-o', f'{bd}/cam.elf', '-T', f'{bd}/cam.ld', '-e', '0', '--no-rosegment', '-N', f'{bd}/cam.o',
   '--defsym', 'SrvGetServiceHandle=0x12954C', '--defsym', 'ShmBlockMap=0x13903C')
syms = {p[2]: int(p[0], 16) for p in (l.split() for l in subprocess.check_output(['llvm-nm', f'{bd}/cam.elf']).decode().splitlines()) if len(p) == 3}
def section(n):
    sh('llvm-objcopy', '-O', 'binary', f'--only-section={n}', f'{bd}/cam.elf', f'{bd}/{n[1:]}.bin')
    return open(f'{bd}/{n[1:]}.bin', 'rb').read()
magic, cfg, code = section('.cammagic'), section('.camcfg'), section('.camcode')
assert struct.unpack('<I', magic)[0] == 0x4D414343 and len(cfg) == 20
assert CODE_AT + len(code) <= STATE_AT, len(code)
orig = open(B.ORIG, 'rb').read()
SITES = [(0x2645CC, 0xE3550000, 'cam_yaw_hook'), (0x266284, 0xED840A19, 'cam_pitch_hook'),
         (0x2D22FC, 0xE5990004, 'cam_yaw2_hook'), (0x25C484, 0xEC810A03, 'cam_view_hook')]
cam = [(PAGE, magic + cfg + bytes(CODE_AT - PAGE - 4 - len(cfg))), (CODE_AT, code)]
for site, expect, sym in SITES:
    assert struct.unpack_from('<I', orig, site - B.BASE)[0] == expect, hex(site)
    for a, r in patches: assert not (a <= site < a + len(r)), f'camera hook {site:#x} collides with 60fps patch'
    cam.append((site, struct.pack('<I', B.encode_branch(site, syms[sym], True))))
# camera page records first? No: 60 fps first, camera page, camera hooks last
B.write_ips(patches + cam, f'{ROOT}/out/phase5k_code.ips')

# ---- exheader: bss +0x1000, ir:rst ----
exh = bytearray(open(f'{ROOT}/raw/exheader.bin', 'rb').read())
bss = struct.unpack_from('<I', exh, 0x3C)[0]; assert bss == 0x797F0, hex(bss)
struct.pack_into('<I', exh, 0x3C, bss + 0x2000)   # camera page 0x4F1000 + extension page 0x4F2000
names = [bytes(exh[0x250 + i * 8:0x258 + i * 8]).rstrip(b'\0') for i in range(34)]
if b'ir:rst' not in names:
    slot = names.index(b''); exh[0x250 + slot * 8:0x258 + slot * 8] = b'ir:rst\0\0'
open(f'{ROOT}/out/phase5k_exheader.bin', 'wb').write(exh)
print('camera code', len(code), 'bytes; combined IPS + exheader written')
