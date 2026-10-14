"""Release build: 60 fps + right-stick camera, for game 1.0 or 1.1.
    python3 build_release.py NAME            (1.0)
    SMT4_REGION=us11|eu|jp|kr|tw python3 build_release.py NAME   (see tools/regions.py)
Outputs in out/ (suffix _v11 for 1.1):
  NAME_code.ips         60 fps + camera (needs NAME_exheader.bin as mods/<tid>/exheader.bin)
  NAME_exheader.bin     bss +0x4000 (camera page 0x4F1000, extension pages 0x4F2000-0x4F4000), ir:rst added
  NAME_60fps_only.ips   60 fps without camera / camera-page features (no exheader needed)
  NAME_code_hw.ips      real-hardware subset (core only)
"""
import sys, struct, subprocess, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib; B = importlib.import_module('build')
NAME = sys.argv[1]
SFX = B.SUFFIX
B.GAME_SYMS.update({'UpdateFrame_cont':0x368810,'SetPosA_cont':0x28C8B0,'SetPosB_cont':0x29A2B8,
    'GetGameCamera':0x2C5218,'ModelCalc':0x115B74,'CamSetLookAt_abs':0x2C20A8,'CamUpdateStereo_abs':0x2BCBB0,'CamSetProj_abs':0x2C1FFC,
    'GameAlloc_abs':0x2C4B84,'Anim_cont':0x2BF9F4,'AnimSlide_cont':0x2B9224,'AnimV2_cont':0x295390,
    'AnimV3_cont':0x29652C,'BeforeDraw_cont':0x103B5C,'CellPut':0x2BF460,'QuadFn_cont':0x2B1594,'SetScale_cont':0x25C0F4,'TaskAlive':0x2BF41C,
    'CellA_cont':0x2BF464,'CellB_cont':0x2BF55C,'CellC_cont':0x2B06A4,'CellW_cont':0x2B090C})
ns = {}
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build_phase1.py')).read().split('patches, syms')[0], ns)
SITES = ns['SITES'] + [
  dict(addr=0x36880C, kind='b',  to='hk_updframe', expect=0xED900A10),
  dict(addr=0x12477C, kind='bl', to='hk_blend',    expect=0xEE300A29),
  dict(addr=0x10AED8, kind='bl', to='hk_modelcalc',expect=0xEB002B25),
  dict(addr=0x28C8AC, kind='b',  to='hk_setpos_a', expect=0xE5923000),
  dict(addr=0x29A2B4, kind='b',  to='hk_setpos_b', expect=0xED800A16),
  dict(addr=0x2BF9F0, kind='b',  to='hk_anim_entry', expect=0xE92D41F0),
  dict(addr=0x2B9220, kind='b',  to='hk_anim_slide', expect=0xE92D4070),
  dict(addr=0x29538C, kind='b',  to='hk_anim_v2',    expect=0xE92D41F0),
  dict(addr=0x296528, kind='b',  to='hk_anim_v3',    expect=0xE92D4070),
  dict(addr=0x103B58, kind='b',  to='hk_beforedraw', expect=0xE3A00000),
  dict(addr=0x108A64, kind='bl', to='hk_campdraw',   expect=0xE3A00001),
  dict(addr=0x1031F4, kind='bl', to='hk_runend',     expect=0xE59D0000),
]
patches, syms, cave = B.build([f'{B.ROOT}/patch/phase3b.S', f'{B.ROOT}/patch/interp_arm.c',
                               f'{B.ROOT}/patch/interp_thumb.c', f'{B.ROOT}/patch/interp_ext.c'],
                              SITES + ns['BOOT'], NAME)
ROOT = B.ROOT
B.write_ips([p for p in patches if p[0] < B.P(0x4F1000)], f'{ROOT}/out/{NAME}{SFX}_60fps_only.ips')
hw = B.hw_patches(patches)
B.write_ips(hw, f'{ROOT}/out/{NAME}{SFX}_code_hw.ips')
print('hw patches', len(hw), 'of', len(patches))

# ---- right-stick camera at page 0x4F1000 ----
PAGE, CODE_AT, STATE_AT = B.P(0x4F1000), B.P(0x4F1020), B.P(0x4F1F00)
bd = f'{ROOT}/build/cam_{NAME}{SFX}'; os.makedirs(bd, exist_ok=True)
sh = lambda *a: subprocess.check_call(list(a))
sh('clang', '--target=armv6k-none-eabi', '-marm', '-mfloat-abi=hard', '-mfpu=vfpv2', '-Oz', '-ffreestanding',
   '-fno-builtin', '-fno-math-errno', '-nostdlib', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
   '-ffunction-sections', '-fdata-sections', '-Wall', '-c', B.subst_source(f'{ROOT}/src/cam_stick.c'),
   '-o', f'{bd}/cam.o', f'-DCAM_STATE=0x{STATE_AT:X}u', '-DCAM_FLAGS=4')
open(f'{bd}/cam.ld', 'w').write('SECTIONS {\n'
    f'  . = 0x{PAGE:X}; .cammagic : {{ *(.cam.magic) }}\n'
    f'  . = 0x{PAGE+4:X}; .camcfg : {{ *(.cam.cfg) }}\n'
    f'  . = 0x{CODE_AT:X}; .camcode : {{ *(.cam.text*) *(.cam.rodata*) *(.rodata*) *(.text*) }}\n'
    '  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) *(.llvm_addrsig) }\n}\n')
sh(B.LLD, '-o', f'{bd}/cam.elf', '-T', f'{bd}/cam.ld', '-e', '0', '--no-rosegment', '-N', f'{bd}/cam.o',
   '--defsym', f'SrvGetServiceHandle=0x{B.P(0x12954C):X}', '--defsym', f'ShmBlockMap=0x{B.P(0x13903C):X}')
csyms = {p[2]: int(p[0], 16) for p in (l.split() for l in subprocess.check_output(['llvm-nm', f'{bd}/cam.elf']).decode().splitlines()) if len(p) == 3}
def section(n):
    sh('llvm-objcopy', '-O', 'binary', f'--only-section={n}', f'{bd}/cam.elf', f'{bd}/{n[1:]}.bin')
    return open(f'{bd}/{n[1:]}.bin', 'rb').read()
magic, cfg, code = section('.cammagic'), section('.camcfg'), section('.camcode')
assert struct.unpack('<I', magic)[0] == 0x4D414343 and len(cfg) == 20
assert CODE_AT + len(code) <= B.EXT3, f'camera code {len(code)} overlaps ext3'
orig = open(B.ORIG, 'rb').read()
CSITES = [(0x2645CC, 0xE3550000, 'cam_yaw_hook'), (0x266284, 0xED840A19, 'cam_pitch_hook'),
          (0x2D22FC, 0xE5990004, 'cam_yaw2_hook'), (0x25C484, 0xEC810A03, 'cam_view_hook')]
cam = [(PAGE, magic + cfg + bytes(CODE_AT - PAGE - 4 - len(cfg))), (CODE_AT, code)]
for site10, expect, sym in CSITES:
    site = B.P(site10)
    w = struct.unpack_from('<I', orig, site - B.BASE)[0]
    assert B.masked_same(expect, w), f'camera site {site:#x}: {w:#010x}'
    for a, r in patches: assert not (a <= site < a + len(r)), f'camera hook {site:#x} collides'
    cam.append((site, struct.pack('<I', B.encode_branch(site, csyms[sym], True))))
B.write_ips(patches + cam, f'{ROOT}/out/{NAME}{SFX}_code.ips')

exh = bytearray(open(B.EXH, 'rb').read())
bss = struct.unpack_from('<I', exh, 0x3C)[0]; assert bss == B.R['bss'], hex(bss)
struct.pack_into('<I', exh, 0x3C, bss + 0x4000)   # pages 0x4F1000 .. 0x4F4000
names = [bytes(exh[0x250 + i * 8:0x258 + i * 8]).rstrip(b'\0') for i in range(34)]
if b'ir:rst' not in names:
    slot = names.index(b''); exh[0x250 + slot * 8:0x258 + slot * 8] = b'ir:rst\0\0'
open(f'{ROOT}/out/{NAME}{SFX}_exheader.bin', 'wb').write(exh)
print(f'version {B.R["name"]} ({B.REGION}, {B.R["tid"]}): camera {len(code)} bytes; outputs out/{NAME}{SFX}_*')
