import sys; sys.path.insert(0,'/home/claude/smt4/tools')
import importlib; B = importlib.import_module('build')
B.GAME_SYMS['UpdateFrame_cont'] = 0x368810
ns = {}
exec(open('/home/claude/smt4/tools/build_phase1.py').read().split('patches, syms')[0], ns)
SITES2 = ns['SITES'] + [
  dict(addr=0x36880C, kind='b',  to='hk_updframe', expect=0xED900A10),  # vldr s0,[r0,#64]
  dict(addr=0x12477C, kind='bl', to='hk_blend',    expect=0xEE300A29),  # vadd.f32 s0,s0,s19
]
patches, syms, cave = B.build(f'{B.ROOT}/patch/phase2a.S', SITES2 + ns['BOOT'], 'phase2a')
for k in sorted(syms, key=syms.get): print(f'{syms[k]:08X} {k}')
print('cave bytes', len(cave), 'end', hex(B.CAVE+len(cave)))
B.write_ips(patches, f'{B.ROOT}/out/phase2a_code.ips')
B.write_cheat([p for p in patches if p[0] != 0x1016C0], f'{B.ROOT}/out/phase2a_cheat.txt', '60FPS Phase 2a (smooth animation)', [])
