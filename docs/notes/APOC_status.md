# SMT IV: Apocalypse (USA, 000400000019A200): port status

Files (private, gitignored): `Shin Megami Tensei IV - Apocalypse/` (`code.bin` decompressed, `exheader.bin` from the decrypted .3ds).
Build: `SMT4_REGION=apoc python tools/build_release.py phase8` (first test build, made before builds 9-12); core-only Azahar test: add `SMT4_CFLAGS=-DNO_EMU_CHECK` and use `*_code_hw.ips`.

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
