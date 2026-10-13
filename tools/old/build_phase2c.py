import sys; sys.path.insert(0,'/home/claude/smt4/tools')
import importlib; B = importlib.import_module('build')
B.GAME_SYMS.update({'UpdateFrame_cont':0x368810,'SetPosA_cont':0x28C8B0,'SetPosB_cont':0x29A2B8,
                    'GetGameCamera':0x2C5218,'ModelCalc':0x115B74,
                    'CamSetLookAt':0x2C20A8,'CamUpdateStereo':0x2BCBB0})
ns = {}
exec(open('/home/claude/smt4/tools/build_phase1.py').read().split('patches, syms')[0], ns)
SITES = ns['SITES'] + [
  dict(addr=0x36880C, kind='b',  to='hk_updframe', expect=0xED900A10),  # vldr s0,[r0,#64]
  dict(addr=0x12477C, kind='bl', to='hk_blend',    expect=0xEE300A29),  # vadd.f32 s0,s0,s19
  # phase 2b: interpolation
  dict(addr=0x10AED8, kind='bl', to='hk_modelcalc',expect=0xEB002B25),  # bl FUN_00115B74
  dict(addr=0x28C8AC, kind='b',  to='hk_setpos_a', expect=0xE5923000),  # ldr r3,[r2]
  dict(addr=0x29A2B4, kind='b',  to='hk_setpos_b', expect=0xED800A16),  # vstr s0,[r0,#0x58]
]
patches, syms, cave = B.build([f'{B.ROOT}/patch/phase2b.S', f'{B.ROOT}/patch/interp.c'], SITES + ns['BOOT'], 'phase2c')
for k in sorted(syms, key=syms.get): print(f'{syms[k]:08X} {k}')
print('cave bytes', len(cave), 'end', hex(B.CAVE+len(cave)))
B.write_ips(patches, f'{B.ROOT}/out/phase2c_code.ips')
B.write_cheat([p for p in patches if p[0] != 0x1016C0], f'{B.ROOT}/out/phase2c_cheat.txt', '60FPS Phase 2c (animation + camera/object interpolation)', [])
