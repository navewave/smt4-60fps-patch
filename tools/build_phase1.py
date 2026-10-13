import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(globals().get('__file__', sys.argv[0]))))
from build import *
SITES = [
  # main loop
  dict(addr=0x101B8C, kind='bl', to='hk_frame_begin',   expect=0xEB002679),  # bl InputUpdate
  dict(addr=0x101E0C, kind='bl', to='hk_fade',          expect=0xEB000189),  # bl ScreenFadeUpdate
  dict(addr=0x101E10, kind='bl', to='hk_playtime_flag', expect=0xE5D50004),  # ldrb r0,[r5,#4]
  dict(addr=0x101E98, kind='bl', to='hk_sound_flag',    expect=0xE5D40000),  # ldrb r0,[r4]
  # task scheduler
  dict(addr=0x103070, kind='bl', to='hk_task_gate',     expect=0xE1D403F4),  # ldrsh r0,[r4,#0x34]
  # XFADEDL crossfade counter
  dict(addr=0x10946C, kind='bl', to='inc_r0',           expect=0xE2800001),  # add r0,r0,#1
  # ENCEFFDL encounter-transition counters
  dict(addr=0x10A038, kind='bl', to='inc_r2',           expect=0xE2822001),  # add r2,r2,#1
  dict(addr=0x10A03C, kind='bl', to='inc_r0', cond=0xD, expect=0xD2800001),  # addle r0,r0,#1 -> blle
  dict(addr=0x10A04C, kind='bl', to='inc_r0_from_r1',   expect=0xE2810001),  # add r0,r1,#1
  # CAMPDL camp-menu background animation
  dict(addr=0x108878, kind='bl', to='hk_camp',          expect=0xE28F1F7E),  # add r1,pc,#504
]
BOOT = [dict(addr=0x1016C0, kind='raw', word=0xE3A00000, expect=0xE3A00001)]  # mov r0,#1 -> #0 (vsync interval at boot)

patches, syms, cave = build(f'{ROOT}/patch/phase1.S', SITES + BOOT, 'phase1')
for k in sorted(syms, key=syms.get): print(f'{syms[k]:08X} {k}')
print('cave bytes', len(cave), 'end', hex(CAVE+len(cave)))
os.makedirs(f'{ROOT}/out', exist_ok=True)
write_ips(patches, f'{ROOT}/out/phase1_code.ips')
# cheat version: boot patch is useless at runtime (hook pins the interval instead)
write_cheat([p for p in patches if p[0] != 0x1016C0], f'{ROOT}/out/phase1_cheat.txt', '60FPS Logic-Gate Phase 1', [])
