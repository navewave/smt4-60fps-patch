import sys; sys.path.insert(0,'/home/claude/smt4/tools')
import importlib; B = importlib.import_module('build')
B.GAME_SYMS.update({'UpdateFrame_cont':0x368810,'SetPosA_cont':0x28C8B0,'SetPosB_cont':0x29A2B8,
                    'GetGameCamera':0x2C5218,'ModelCalc':0x115B74,
                    'CamSetLookAt_abs':0x2C20A8,'CamUpdateStereo_abs':0x2BCBB0,
                    'GameAlloc_abs':0x2C4B84,'Anim_cont':0x2BF9F4,'AnimSlide_cont':0x2B9224,'AnimV2_cont':0x295390,'AnimV3_cont':0x29652C,'BeforeDraw_cont':0x103B5C})
ns = {}
exec(open('/home/claude/smt4/tools/build_phase1.py').read().split('patches, syms')[0], ns)
SITES = ns['SITES'] + [
  dict(addr=0x36880C, kind='b',  to='hk_updframe', expect=0xED900A10),  # vldr s0,[r0,#64]
  dict(addr=0x12477C, kind='bl', to='hk_blend',    expect=0xEE300A29),  # vadd.f32 s0,s0,s19
  # phase 2b: interpolation
  dict(addr=0x10AED8, kind='bl', to='hk_modelcalc',expect=0xEB002B25),  # bl FUN_00115B74
  dict(addr=0x28C8AC, kind='b',  to='hk_setpos_a', expect=0xE5923000),  # ldr r3,[r2]
  dict(addr=0x29A2B4, kind='b',  to='hk_setpos_b', expect=0xED800A16),  # vstr s0,[r0,#0x58]
  # phase 3b: sprite state swapping
  dict(addr=0x2BF9F0, kind='b',  to='hk_anim_entry', expect=0xE92D41F0),  # push {r4-r8,lr}
  dict(addr=0x2B9220, kind='b',  to='hk_anim_slide', expect=0xE92D4070),  # push {r4,r5,r6,lr}
  dict(addr=0x29538C, kind='b',  to='hk_anim_v2',    expect=0xE92D41F0),  # push {r4-r8,lr}
  dict(addr=0x296528, kind='b',  to='hk_anim_v3',    expect=0xE92D4070),  # push {r4,r5,r6,lr}
  dict(addr=0x103B58, kind='b',  to='hk_beforedraw', expect=0xE3A00000),  # mov r0,#0
  dict(addr=0x108A64, kind='bl', to='hk_campdraw',   expect=0xE3A00001),  # mov r0,#1
  dict(addr=0x1031F4, kind='bl', to='hk_runend',     expect=0xE59D0000),  # ldr r0,[sp]
]
patches, syms, cave = B.build([f'{B.ROOT}/patch/phase3b.S', f'{B.ROOT}/patch/interp_arm.c', f'{B.ROOT}/patch/interp_thumb.c', f'{B.ROOT}/patch/interp_ext.c'], SITES + ns['BOOT'], 'phase4b')
for k in sorted(syms, key=syms.get): print(f'{syms[k]:08X} {k}')
print('cave bytes', len(cave), 'end', hex(B.CAVE+len(cave)))
B.write_ips(patches, f'{B.ROOT}/out/phase4b_code.ips')
B.write_cheat([p for p in patches if p[0] != 0x1016C0], f'{B.ROOT}/out/phase4b_cheat.txt', '60FPS Phase 4b (3D + UI + navi map, Azahar extension)', [])
