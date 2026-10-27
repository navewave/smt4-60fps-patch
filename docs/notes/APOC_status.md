# SMT IV: Apocalypse (USA, 000400000019A200): port status

Files (private, gitignored): `Shin Megami Tensei IV - Apocalypse/` (`code.bin` decompressed, `exheader.bin` from the decrypted .3ds).
Build: `SMT4_REGION=apoc python tools/build_release.py phase8` (first test build, made before builds 9-12); core-only Azahar test: add `SMT4_CFLAGS=-DNO_EMU_CHECK` and use `*_code_hw.ips`.

## Boot crash in the first test build (fixed 2026-10-26)
- Azahar log: Undefined Instruction at PC 0x2E63A8, LR 0x525945 (`frame_c`, first `GameAlloc` call). The window search had mapped three called functions into the middle of other code: GameAlloc 0x2E63A8 (Thumb code), CamUpdateStereo 0x17DAF8, CamSetLookAt 0x152464.
- Real addresses (MANUAL in tools/port.py): GameAlloc 0x163060, CamUpdateStereo 0x1524CC, CamSetLookAt 0x14F0DC. Status DrawUp owner range dropped (start mapped into data).
- a2core booted (menus smooth at 60 Hz) but the 3D world stayed at 30 Hz: GetGameCamera had also mapped into another function (0x145070); the real getter is 0x152F54 = `*(*(0x59DBC0+4)+0x80)`.
- port.py now notes any function entry (push or bl target) that maps to a non-matching word; other regions show none.
- Test builds a3core (`SMT4_CFLAGS=-DNO_EMU_CHECK`, `_code_hw.ips`, installed in Azahar) and a3 (full + exheader) from the build 12 sources: `test_builds/3_core_cam`, `4_full_cam`.

## a3core result (2026-10-26): menus and camera smooth, player stutters
- SetPosA has 50 callers in SMT IV but 5 in Apocalypse (all effect/summon code). The "new setter 0x36BB20" is a rotation setter (degrees * pi/180 -> node helper 0x44B3C8), not position.
- New character class: setter 0x3FD76C (vec3*, 69 callers) / 0x3FD7B4 (s0-s2, 19 callers) stores the position at obj+0x1B4; where it reaches the node is not found statically (+0x1B4 is a common offset, mostly 2D).
- Probe: `python tools/gdb/apoc_player.py` while walking in the field (talks RSP directly; finds the node near the look-at point, watches +0x58, logs writer pc/lr to tools/gdb/apoc_player.log). Not run yet.

### Probe results (a6, field, 2026-10-26)
- Game camera object 0x5F10CC (.bss); look-at = player + (0, 170, 0). STATE rec[] empty: the SetPos hooks never fire for characters.
- Character objects (3, same position, word 0x557B20 at +0: sparse table, not a normal vtable): position +0x1B4 (setter 0x3FD76C), scale +0x1F0. No pointer from them reaches a node holding the position, and nothing in the 0x3F8000-0x404000 module reads +0x1B4.
- Structs whose +0x4C 3x4 matrix carries the player position (5 of them, e.g. 0x8633F54; +0x0 = 1, +0x7C/+0xAC camera-like matrices, +0xE0 xyz copy, +0x11C scale): +0x88 is a float here, so not SMT IV's node layout. Plus moving bone-like matrices around 0x8689xxx. Written once per logic tick only (L1).
- Azahar with CPU JIT on reports watchpoint stops at the end of the JIT block: pc/lr scattered (0x17F178, 0x180058, 0x16A240, ...), no store at the pc. Next: CPU JIT off, then `python tools/gdb/apoc_player.py` (search + watch) or `--watch <x addr>`.

### Found with CPU JIT off: h3d model system (a7)
- Writer of the bone world matrices: matrix multiply 0x199EA0 from the NW4C skeleton updater 0x17EFA4(skeleton, root matrix), called by the per-model update 0x156994(model, r1, anim step s0), called by task "h3dModelCalcProc" 0x145F94 (registered at 0x13B3D0; manager *(0x56FCC0), model vector +0x18..+0x1C, 23 models in the field). Name hash 0x5F7: gated, so it ran at 30 Hz.
- h3d model (+0 = 0x557B20): position +0x1B4 (setter 0x3FD76C/0x3FD7B4), scale +0x1F0, anim step +0x1FC (1.0), parent +0x1B0 (attached models carry the same absolute position). Player 0x8633E00 in that run.
- a7: gate whitelists hash 0x5F7 (cmnne #0x19, REGION_APOC); bl at 0x146020 -> hk_h3d_model: step * g_dt, h3d_pre (midpoint of +0x1B4 on logic frames, HRec[32] at STATE+0x200), update, h3d_post (true value back). Apocalypse-only addresses live in regions.py (native_syms / native_sites), not in the port map. Core 2932/4056. Not yet run.

### a7: player smooth (confirmed). Battle still crashed, same signature (obj+0x34 = 0xE5801010)
- mif_swap (SMT IV field 2D tiles, *(0x447A78) -> 0x58103C, 128 records of 0x194 at +0x1D0, 60 floats each) has no counterpart: Apocalypse never builds a 0x194 stride (SMT IV: ~200 `mov rX,#404`). It blended floats over ~51 KB past that object. Off for Apocalypse, with bg_swap (COMBG, draw hooks already absent) and the WDEF player-position watch (navi only).
- Build a8 in `test_builds/8_full_battle2`, installed in Azahar. If battles still crash: bisect with a core-only build (a7core) or JIT-off breakpoint at 0x16BCA8 when r2 < 0x08000000.

## Right-stick camera (a4, 2026-10-26)
- Field controller 0x33F9F4 (pool: cam ptr 0x5806A4, lock flags 0x5F15FC): snap byte +0xB4 (was +0xB0), +0xB5, cool-down s16 +0xB6 (was +0xB2), one more lock byte 0x5F15FC+0x21; &cam is in r10 at the L/R step (was r8).
- Sites: yaw 0x33FDE4, pitch 0x36A548 (window search said 0x36A54C), mode-0 yaw 0x339D8C (r9 = 0x5806A0), free-roam view 0x324B7C. cam_stick.c: REGION_APOC macros; build_release passes -DREGION_* to the camera compile. USA camera page unchanged.
- a4 failed to boot: `Failed to load ROM (Error 12)`. Azahar patches a buffer of code.bin size + bss rounded up to a page (loader/ncch.cpp LoadExec) and its IPS applier rejects writes past it. USA: 0x477000 + 0x7E000 = 0x4F5000 covers the ext6 page by rounding luck; Apocalypse: 0x5C5000 + 0x8C000 = 0x651000 < 0x651B94. build_release now raises bss until the buffer covers the last IPS write (USA stays +0x4000, Apocalypse +0x5000).
- Build a5 (same code.ips, fixed exheader) in `test_builds/5_full_stick`: boots, right stick works; player still stutters; crash on entering a battle (unmapped reads through instruction words at 0x16BC60..0x16BCD8, an object's +0x34 overwritten).
- Battle crash cause: scene camera array 0x464B10 had mapped to 0x63FFD0 (1 vote); the right one is scene manager 0x5B3BF4 + 0x20 = 0x5B3C14 (same store at 0x143480 as USA 0x1079D8). dc_swap calls a vfunc through it and writes node fields when a scene flag is set (battles). Also off for Apocalypse: WDEF navi watches (0x44CDF0) and InfoWin (0x43E128, ambiguous 0x57F574/0x57F578): they write through their pointers.
- Build a6 in `test_builds/6_full_battlefix`, installed in Azahar.
- Uncommitted from an earlier session: CI_OWN SEARCH2D range in interp_ext.c (+ port maps): USA full IPS differs from build 12 only by that.

## How close is it to SMT IV
- Same engine and task system (214 of SMT IV's task names exist), but only ~43 % of SMT IV's code survives as identical instruction runs: recompiled and reordered (functions moved by up to MBs). `tools/port.py` uses a whole-file window search for it (`glob`).
- Layout: text 0x100000-0x525020 (cave ~4 KB), rodata 0x526000, data 0x568000, .bss end 0x64C11C -> STATE 0x64C120, patch pages from 0x64D000.

## Ported (hook sites checked by the build, runtime hook words by verify_region.py)
Main loop gate (input, fades, play time, sound, task gate, run-end), boot vsync (setter 0x13A630 identical, DISP = 0x63FD88), anim UpdateFrame, AsdModelCalcProc (0x1400F8 -> ModelCalc 0x14E2B8), SetPosA 0x32C5C4 / SetPosB 0x36BAFC, sprite animator 0x163AA4 (+ float arg s0) and variants, crossfade (step in s2: hk_blend_s2), XFADE / ENCEFF counters, CellPut family (0x1835B4, 0x164B5C, 0x1849BC, 0x185DD4), quad 0x186244, event portraits (EVE_2dTex 0x2897CC, EVE_BustupTex 0x26C568: struct offsets NOT verified).

## Left out (absent in tools/port.py MANUAL)
- CAMPDL camp-menu update/draw split (rewritten): camp background may animate at double speed.
- Right-stick camera (yaw hook context differs).
- SetScale (folded into node helpers 0x44Bxxx): model zoom smoothing off.
- COMBG background draw calls, nav map (no NAVI task in that form).
- 5 of 9 cell-smoothing UI owner ranges (compendium, talk panel, button bar differ).

## Risks to check in play
- New position setter 0x36BB20 goes through node helper 0x44B3C8, bypassing SetPos hooks: some characters may move at 30 Hz.
- Extension features read game structs that may have changed (event work, scene manager 0x5B3BF4, task list 0x640118): the full build is the riskier one.
- New systems not handled: partner attacks, h3dModelDraw renderer, EvtSnowNoise / EvtAlart / EvtFilter.

## Next: bring builds 9-12 over (SMT IV is at build 12, 2026-10-14)
- Re-run `python tools/port.py apoc`: build 12 added `CurveEval` (FUN_00235794, cutscene camera cut test) to the game symbols; it needs an apoc mapping (or a MANUAL entry).
- Cutscene cameras: check that Apocalypse also drives them with NW4C "CameraAnimation" (`SMT4_CFLAGS=-DCAMLOG` build + `python tools/gdb/cam_dump.py --anims` during a cutscene), then the teleport-style cut test applies as is.
- Actual 3DS: `SMT4_REGION=apoc python tools/build_plugin.py <name>` should work (it diffs the Azahar and hardware IPS, finds the gate words by pattern); check the cave tail has 8 free bytes for the gate trampoline and that STATE/page addresses are translated (subst_source).
- Test order that worked for SMT IV: Azahar first (whichbuild.py, GDB probes), then the N3DS; read Luma crash dumps from SD:/luma/dumps/arm11/.
