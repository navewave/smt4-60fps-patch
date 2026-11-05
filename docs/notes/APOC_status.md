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
- mif_swap was turned off here on a wrong reading: the stride IS 0x194 (built as `mov #101` + `lsl #2`), only the array starts 12 bytes later (+0x1DC). Fixed in a14. bg_swap (COMBG, draw hooks already absent) and the WDEF player-position watch stay off.
- Build a8 in `test_builds/8_full_battle2`, installed in Azahar. If battles still crash: bisect with a core-only build (a7core) or JIT-off breakpoint at 0x16BCA8 when r2 < 0x08000000.

## Build b13 (2026-11-03): cleanup and actual-3DS files, uncommitted
- s15 play report: "hard to tell"; stopping point for this round.
- Cleanup: bg_swap compiled out on Apocalypse (the only warning); QRec colour fields SMT IV only (Apocalypse QTab stays 36 KB); port maps regenerated, unchanged. ZPROBE is gone (battle rule permanent).
- EUR: CAM_LIM is plain THR2 again. The wide battle limit cost 24 bytes and left the 2400-byte cave full, so build_plugin had no room for the gate trampoline (last 8 bytes). EUR core 2376/2400.
- Outputs: every version b13 (verify_region OK; Apocalypse features off where absent: SetScale, COMBG, nav map), plugins for us10, us11, eu, jp, kr, tw, apoc. SMT IV USA b13 == s15 byte for byte; Apocalypse b13 == s13/s14 Apocalypse (a19 + FOV rule rewrite).
- Apocalypse actual 3DS: `test_builds/b13_actual_3ds` (luma/titles/000400000019A200/code.ips + exheader.bin, luma/plugins/000400000019A200/smt4_60fps.3gx); exheader bss +0x5000 to 0x65111C (pages up to 0x652000), same as the Azahar one. Plugin gate 0x13643C -> 0x525FF8. Untested on hardware.
- Other Apocalypse versions (apoc_eu, apoc_jp, apoc_jp11): added by the "Apocalypse versions sync" session (regions.py family 'apoc', import_game.py, APOC_versions.md); need decrypted game files.

## Build s15 (SMT IV), installed in Azahar (USA 1.0), tested ("hard to tell")
- s14 clip `2026-11-03 15-04-51.mp4` (25.6 s, 212 of 1535 frames repeated on both screens = Azahar frame drops; scratchpad det30.py skips those).
- Result panel with s14: smooth after the start (153, 120, 88, 58, 28 per frame); the start still holds (378 x3, 223 x2) and the first moving tick is a quarter step, also on the way out. Its cells take positions through the sprite getter, so a hook on the full sprite draw 0x2B9708 would not cover it.
- Still 30 Hz, all alpha: Effects panel dim (scene alternates 7.4/0.7 ...) and fade-out, banner fades ("The enemy strikes first!" 80,80,72,72,67,67). The BTLFADE colour blend (s14) changed nothing and is removed. btlUI's dim sprite (+0x7EC0, alpha +0x20 via the main animator, 37 animated sprites per tick in btlUI Calc, so no table exhaustion) should be blended by spr_swap but is not; cause unknown.
- s15: q_col (ext5, SMT IV only): quad_interp blends uniform vertex colours (r0-r3 of FUN_002B1590) per byte on logic frames; bytes jumping > 0x60 and colours blended upstream (last tick's logic-frame input != in-between) are left alone. QRec + col/lcol (44 bytes, QTab ~45 KB). fov_swap/fov_restore moved to ext5 on SMT IV (ext4 was full).
- 30 Hz on purpose or not fixable: battle/encounter particles (Fang Breaker, Hama, encounter cubes: EPL positions), portrait hit flipbooks (ice shards, "?" ring), damage number shake.
- Not explained: top-screen brightening after the post-battle fade (frames 1494-1505 pairs, after the screen fade has finished; bottom fade is smooth).

## s14 play report: relic exit cut is in the original game; no visible change on the result screen or Effects panel
- Azahar log: SMT IV booted at about 14:54:15 with the s14 code.ips (installed 14:52:43), so s14 was loaded.
- Expected effect of the guard on a 4-tick slide: its first moving tick is still a quarter step (last tick's logic frame equals its in-between frame while the sprite was at rest), only ticks 3-4 change. Too small to see on the result panel. A stronger rule (no generic blend inside a sprite that spr_swap already swapped) needs a hook on the full sprite draw 0x2B9708. Needs an s14 clip first.
- Azahar logs (s13 and s14 sessions): "unmapped Write8 @ 0x00000E95 at PC 0x00108308" every frame for about a minute. HDRCL (task 0x108300, prio 0x6A) writes [ctx + 0xE95] with ctx = FUN_002C0AC8() == 0. Harmless in Azahar, a crash on hardware if it happens there. Not investigated: unknown whether build 12 or the unpatched game does it.

## Build s14 (SMT IV, 2026-11-03), installed in Azahar (USA 1.0)
- Owner clips of s13 (AV1, OBS: top 1330x798 at 0,132, bottom 590x443 at 1330,487): relic `2026-11-03 14-41-24.mp4`, battle `14-41-49.mp4`.
- Relic exit: the panel goes (frame 239), the close-up camera pans for 3 ticks at 60 Hz (6 distinct frames), then a clean cut to the field (frame 247), no in-between frame. Probably the game's own move; not changed.
- Battle result panel: in and out move -17/-53, -16/-49 ... and -25/-75 per frame pair, i.e. a quarter step on logic frames: sprite midpoint (spr_swap) blended again by quad_interp. Same for the skill name panel sliding in (bottom screen, frames 216-222). s14: the Apocalypse double-blend guard (QRec tickL/lx/ly, "source already 60 Hz") now in every region. QTab is 36 KB instead of 24 KB on SMT IV.
- Effects panel (X): slide-in is 60 Hz (10 px per frame; first position held 2 frames). The background dim on open and the panel's fade-out on close step in pairs (30 Hz). BTLFADE (draw 0x1E49CC, prio 0x80) fills the screen with the colour word at battle work +0x4A0C (offset checked in every version): s14 btlfade_swap / btlfade_restore blend it per byte on logic frames (jumps > 0x60 per byte left alone), data 0x4F2FE0-0x4F2FF0. Not confirmed to be the dim. btlUI's own dim sprite (work +0x7EC0, cell 0x106, alpha +0x20 from the main animator) should already be blended. The panel's own fade-out is not identified.
- Fade-in after battle steps irregularly on both screens at once, same as the frame drops during the attack effect: Azahar dropping frames, not judged.

## s13 play report (SMT IV): relic scan good; open items
- Weird camera cut when the relic scan returns to normal play. s13 changes nothing for the field camera (CAM_WIDE only in battle), so likely older: the core blends any camera jump under 150 per axis outside cutscene cameras, which would draw one frame between the scan view and the field view. Not measured.
- Battle "X Effects" panel slides up at 30 Hz. Not identified: btlEffect Calc (0x1E4990 -> 0x3DD3D0) is the effect loader, Status Calc is another screen.
- Battle end screen slides left/right at 30 Hz. Result CALC 0x18BFEC slides sprite +0x20 with variant 0x296528 (start 320, 75 px per tick, -5 per tick) and six sprites +0xAC + i*0x8C with 0x2B9220; draws 0x1A15B0 / 0x1C8874 read sprites at draw time. On paper only the second tick of the 0x296528 slide is vetoed by spr_bigmove_fix (-70 after a +245 placement), so the 30 Hz look is not explained yet.
- Next: 60 fps clips of the three (as for InfoWin).

## SMT IV ports, build s13 (2026-11-03), installed in Azahar (USA 1.0); relic scan confirmed
- a18 and a19 confirmed in play (partner line, critical zoom with the probe, equip rotation). The zoom probe is now the battle rule in every region (no `-DZPROBE`): in battle cam_cut sets CAM_WIDE and skips the cut test, fov_swap takes any fov step. Real battle shot changes can show one in-between frame; none reported.
- SMT IV (all regions): CAM_WIDE = State byte +0x06 (padding), CAM_LIM in the core for every region (core 2376 -> 2400; EUR is now 2400/2400, full). ext5_install (called from ext_install, CAMPAGE == 1) hooks the relic draw FUN_0020DC34 (bl at 0x20DD60 / 0x20DD8C / 0x20DDC8 / 0x20E200 / 0x20E384, counter +0x44; fade hook uses r12, not r0) and replaces "bl hk_fade" at 0x101E0C with ext_fade (records 0x4D9758). State at 0x4F2FC0-0x4F2FE0. Sites checked in every region's code.bin.
- Not ported: h3d rotation (SMT IV models are NW4C nodes: rot_swap already), InfoWin (SMT IV has it), UI views / MIF changes / double-blend guard (Apocalypse code only).
- port.py: Apocalypse addresses in comments added to NOT_ADDR; port maps regenerated (only the new SMT IV entries changed).
- Apocalypse built from the same sources (s13_apoc) behaves as a19; a19 stays installed. SMT IV's earlier Azahar mod (qdscan) backed up to the session scratchpad.

## Build a19 (a18 + h3d rotation, `-DZPROBE` kept), confirmed in play
- a18 play report: "looks pretty good" (partner line); equip menu 3D character stutters a little when rotated.
- h3d models keep rotation as a 3x4 at +0x1C0 (rows 0x10 apart; setter at ~0x1AB7xx copies a whole matrix; 0x16A130 builds root = scale +0x1F0 x rot +0x1C0, then position +0x1B4). Nothing blended it, so any model turn (equip turntable, probably characters turning in the field) was 30 Hz. Not confirmed that the equip turntable writes +0x1C0 (the only rotation field of an h3d model).
- h3d_rot (ext5, heap table, pointer at ext data 0x4F0FBC, 16 records): called by the core's h3d_pre / h3d_post when S->emu and CAMPAGE == 1. Row lerp with each row put back to its true length; snap above ~45 deg per tick or on a row-length change (rot_swap's rules).

## Build a18 (InfoWin fix + zoom probe, `-DZPROBE`), confirmed in play
- Owner clip of a17 (OBS 60 fps, h264, `2026-11-03 13-14-12_10mb.mp4`; top screen crop 1330x798 at 0,141). Frame pairs repeat for the partner line text and for the critical zoom; the 3D world is 60 Hz.
- Partner / info line = **InfoWin** (calc 0x156478, draw 0x155CA8, work `*(0x57F574)`). State 2 slides sprite w+0x16C with 0x167458 and copies its x into w+0x18 (w+0x1C = second copy for the ticker scroll) in logic; the draw adds w+0x18 to the text x. Clip steps 55, 50, 45, ... px per two frames. SMT IV has the same fix as WDEF `{0x43E128, 0x18, 2 ints}`; port.py maps 0x43E128 to 0x57F578 (Result CALC's global), which is why it was off here. a18: WDEF `{0x57F574, 0x18, 2, 2}` for Apocalypse.
- Critical zoom in the clip: 3 large steps (frames 566, 568, 570), each held 2 frames, so no blend at all, even with a17's CAM_WIDE. Either the cut test drops it, or the zoom is written where cam_swap / fov_swap do not see it. Probe: in battle cam_cut returns before the cut test with CAM_WIDE = 1, and fov_swap takes any fov step. Smooth zoom = the camera path (then make the battle rule exact); unchanged = look elsewhere (battle effect cam_zoom, another camera object). Side effect: real battle shot changes get one in-between frame.

## After a17: open items (a17 installed; owner declined GDB stub runs, so a18d was not run)
- Stub-free route: 60 fps clip + tools/cadence.py.
- USA/Apocalypse comparison, dialogue. New in Apocalypse: ComWin (calc 0x156230) states 5 and 6 and draw modes 2 / 6 with a slide sprite +0x154; MESS_C type 0xB fourth sprite +0x18AC and type 0x14; message tag sprite +0x1820 (flag +0x191, in/out 0x3AF9F8 / 0x3B61B4, anim 0x1D). EVE_2dTex calc 0x2897CC gains a from/to tween on +0x10 / +0x14 (+0x70..+0x84) and +0x1C decay 0.3 instead of 0.7 while fast forwarding (0x203D74). All of these end in sprites or fields already smoothed, so none explains in 30 Hz / out 60 Hz on paper. Slide-in animator 0x167458 matches USA 0x2B9220 and is hooked: x starts at +320, then -55 per tick, -5 per step. It is used on the way in, the main animator on the way out (MESS_C cases 8 and 7), which fits the report; a slide sprite whose x is copied in logic would explain it, as the UI views did. PartnerTopic is a menu, ruled out.
- Comparison, zoom. Vsync setter 0x13A630 runs at boot only, no slow motion strings, so no logic rate change found. The a17 change altered what the owner saw, so the zoom likely does pass through the core camera blend (inference, not measured). The blend cannot change speed; a tick that accelerates past the 3x test stays unblended, so a fast eased zoom mixes blended and full steps.

## Detector a18d (a17 + -DQDIAG -DCAMLOG), not run
- a17 play report: critical zoom needs another pass, may run at double speed (owner unsure); Smirk fine, leave it; top-screen companion dialogue slides in from the right at 30 Hz, out at 60 Hz.
- Critical zoom, static: battle camera work *(0x571508), task btlCamera Calc prio 4 (logic only), so 0x4F2C40 cannot double step. Mode +0x38: 1 tweens, 3 .bch camera. Action commands reach it through 0x369258 -> 0x40DE54 -> 0x40D274 (switch on cmd +0x4C). `cam_zoom`, `cam_zoom02`, `en_zoom`, `hatu_atc` are battle effect files (table 0x528220, battle/effect/%s.eft), not camera tweens, so the zoom may not be the game camera at all. Not resolved.
- Companion dialogue, static: not identified. Candidates: EVE_BustupTex (0x26C568), TalkBustUp (draw 0x321544, sprites +0x20 stride 0x8C), PartnerTopic (0x35D080), PARTNER (0x2C3328). pt_swap and spr_swap show no in/out difference that explains it.
- a18d: cam_log records fast camera moves only (any axis > 8 per tick, +1 tick), 512 records, adds CAM_WIDE, "camera is battle work +0x6C", tween flags +0x64C, mode +0x38. `SMT4_REGION=apoc python tools/gdb/cam_dump.py`, `SMT4_REGION=apoc python tools/gdb/qd_dump.py`. EXT5_END for Apocalypse now 0x4F3FC0 (portrait records moved out). In `test_builds/18d_detector`, installed.

### Cross-reference with SMT IV USA
- Relic scan: same draw, FUN_0020DC34 (from SEARCH3D 0x2CA650). Work fields 0x34 lower: size code +0x40, counter +0x44. Sites: `ldr r6,[r4,#0x44]` 0x20DD60, fade vcvt 0x20DD8C and 0x20DDC8, rise vcvt 0x20E200 and 0x20E384; same registers (s16 size, s20 layer). Not fixed there. Port: runtime hooks from the extension, ext5 has 1932 bytes free; relic_begin takes the counter offset. Writers not checked on USA.
- Battle camera: same function, FUN_003D7630 (btlCamera Calc 0x1E4928, prio 2), same effect names at 0x400730. Same 150-per-axis limit in cam_swap. Wait for the Apocalypse zoom cause. CAM_LIM needs core room: USA 2376/2520, EUR 2376/2400.

## Build a17
- a16 play report: diorama fades almost perfect, Compendium fade fine; one portrait x move at 30 Hz; critical-hit zoom looks 30 Hz; asked to check Smirk and battle sprite positions.
- EVE_BustupTex (calc 0x26C568, draw 0x281428): x = +0x34 + +0x58 + +0x90 + shake, y = +0x38 + +0x94 + shake. +0x90 / +0x94 are new, tweened in mode 3 with targets +0x98 / +0x9C. Also +0x2C (mode 9 alpha). EVE_2dTex: y slide +0x14 was missing too. PT_DEF extended, PT moved to S+0xA80 (10 records, 104 bytes). Which field the reported portrait used is not confirmed.
- Shake (EVE_BustupShake 0x2AC078, 0x3D8F24 -> work+0x90 / +0x94 of the EVE_2dTex root) is random per tick: left alone.
- Critical zoom: not traced to one data entry. Battle camera 0x4F2C40 tweens pos, target, fov and dist from tables ("dist start", "fovy start"...) in logic, and the core drops the blend above 150 per axis, so a fast zoom gets none. cam_cut sets CAM_WIDE when the move continues last tick's (same test as a cut), and cam_swap then allows 3000 per axis. Applies in the field too, and cuts still stop the blend. Assumes the zoom moves more than 150 per axis: not measured.
- Smirk: debug labels niyari r/g/b/a/frame/lowrate/100 (0x571E20 table) point at a timed colour flash, not a position move; the renderer was not traced. Battle UI (0x28F740, 0x2AE5CC) draws from sprites through getters, so positions go through spr_swap. No separate 30 Hz position path found; if one remains, run the detector in a battle (it can now be read against decomp2.c).
- btlUnit DrawUp 0x2987C4 steps counter +0x1EDC inside the draw (0..0x1D), so it runs at double speed with the patch. What it drives is not traced.
- Fast forward: not changed. Page text changes per logic tick; there is no in-between to draw.

## Build a16, untested
- a15 play report: "so far so good", banner slide and pop almost perfect, one bad frame just before it starts when walking up to it.
- Tile states set a new draw type (e+0xC) and rebuild the quads in the same tick (type 0 hidden, 4 idle, 5 opening, 6 open), so a midpoint between two looks was drawn. mif_swap: no blend on a tick where the type changes. Not confirmed in play.

## Build a15 (2026-11-01): after the a14 play report
a14 report: banner slides at 60 Hz, but "Enter" sliding and "A" popping looked 30 Hz; map and Compendium slides, relic scan 30 Hz. All a15 findings are static; none is confirmed in play yet.
- Decompile for Apocalypse: Ghidra 12.1.4 headless (G:\BombermanOnline-Port\ghidra_12.1.4_PUBLIC, JDK 25) on raw code.bin, base 0x100000, ARM:LE:32:v7, re/ghidra_scripts/SetupApoc.java (blocks, bl targets as seeds) then FixFuncs.java (task functions + push-lr entries), ExportAll.java. Output build/apoc/decomp2.c (not tracked), helper build/apoc/f.py ADDR. The whole run takes about 10 minutes. tasks2.json rows: [name, prio, fn, registration site].
- Generic quads never had the right identity: the bound texture moved from ctx+0x11E4 to +0x11F0 (0x2C1D3C vs 0x16C4B8). CTX_TEX.
- Double blend: quad_interp also blended what a sprite or watch had already blended (logic frames drew 1/4 of the step: 1/4, 1, 1 1/4, 2, which reads as 30 Hz). That is why a14's map watch showed nothing, and it may hide other fixes. Apocalypse guard: QRec keeps the logic-frame input; if last tick's logic and in-between inputs differ, the source is already 60 Hz. USA unchanged.
- UI views: new module 0x484000-0x4A8000 (map, Compendium) draws through {x, y, ox, oy, bank} objects. Logic copies a sliding sprite into one (sprite animator, getters 0x1654D8 / 0x1654E4, ViewSetXY 0x4935B0), helpers 0x493344-0x49368C read it, so spr_swap's midpoint never shows. Map root is obj+0x68+0x4D0 (x runs to -320), Compendium panel +0xC8 (0x4A24A8 draw, 0x4A2404 slide). view_rec / view_swap (hook on ViewSetXY, 32 records at S+0x700) replace the a14 watch. Compendium list alpha is the float +0x850: not done.
- Slide speed from 0x167458 (= USA 0x2B9220): 55 px per tick, minus 5 per tick, from +/-320.
- Banner: tile update is 0x29AA60 (MAPIF C). Besides the vertices it steps e+0x180 (right u of the +0xC0 quad: the text wipe) and the low byte of e+0x80 (fade-in). mif_swap now blends both (MIF_NF 61, MifRec.col). Tile code moved to ext5 (ext4 full).
- Head tile at mif+0x48 (draw 0x29E3A0-0x29EA60) steps its own counters inside the draw, so it runs at double speed with the patch. Not fixed: not on the list, and it is smooth.
- Relic scan: rise and fade both come from int counter +0x78 (layer height = counter * layer, alpha = cos(counter - 15)), size from a table on +0x74. relic_begin + hk_relic_* (native sites in the draw): logic frame draws counter - 0.5 and the midpoint of the last two sizes. Writers 0x299xxx step +0x74 to 6, then +0x78 to 30.
- Camera cuts: SMT IV .bccam became .bch (map cam%03d_%02d, terminal_cam, en%03d_camera, %s_cam); left as .bccam only event/BustUpEvent, event/oc, fountain. So updf_class / ca_step never fire. AsdAnimCamera eval 0x3F9C6C steps frame cam+0x38 by a constant in logic (MAP C 0x292B84, terminal 0x27E0D8 / 0x328838, battle 0x4F2C40), whole frames, so a cut is only the core's blend. ext_animcam sets CAMANIM_TICK there and cam_cut stops the blend.
- USA 1.0 output is byte-identical to build 12 (SEARCH2D line left out for the compare).

## Code audit against SMT IV, build a14 (2026-11-01), untested
- Task priorities shifted: logic 1/2 -> 3/4, draws +2..+5 (tools in build/apoc: tasks.py; not committed). The 30 Hz gate is safe (logic < 0x65, BeforeDraw1L still 0x65). pt_swap walked list 2: it never ran here. Now PT_PRIO 4. Portrait fields match: EVE_BustupTex calc offsets identical, EVE_2dTex state words moved by 4.
- Field tiles / "Enter A" banner: MIF3DL draw 0x29D7F8, task prio 0x71: base *(0x58103C), array +0x1DC, same stride and vertex layout. mif_swap back on, guarded like the draw (*(0x5F15FC+0x600)) and off in battle.
- Map slide: draw arg = *(0x587F28)+0x68, slide floats at arg+0x4D0, so WDEF {0x587F28, +0x538} (replaced in a15 by view_swap). Scroll centre is +0x60 (getter 0x492B84), not added: SMT IV blends it together with the player position watch, whose Apocalypse field is not found.
- Screen fades: new, also 30 Hz in SMT IV. Records 0x6357D8 (2 x 0x14: alpha, counter, total, mode +0x10). hk_fade -> fade_step: midpoint on logic frames (Apocalypse only).
- CI owners rebuilt from real matches (regions.py native_own). The a11-a13 experiments are gone (float y, larger tables, panel and map-screen owners); the "Enter A" and panel reports were not map data: 0x2E7A8C is PPANEL DDRAW. tools/gdb/qd_dump.py keeps its Apocalypse mode and QD_WALK fixes.
- Stats zoom: USA 0x2940AC = 0x3E5558 -> setter 0x3FD784 on h3d models, so a9's +0x1F0 blend is the right target. Still no verdict from play.
- Scene loop 0x1664E4 is identical to USA 0x125174 (flags 0x5B3BFC, cameras 0x5B3C14), so dc_swap's inputs look right; the tween writer has no direct match (flag setter 0x32C5AC, callers in 0x3BA560).
- Camera cuts: UpdateFrame and CurveEval match, but most cameras are not NW4C CameraAnimation any more (see a15).
- Relic scan: draw 0x2DD40C (SEARCH3D task 0x329D64), work *(0x5806C4). +0x88 is a rotation set once, +0x7C.. the position. Rise and fade: see a15.
- Fast forward: MESS_C stays a logic task in both games: pages advance per logic tick. Left alone. Smirk: no finding beyond btlUI DrawDown owner 0x183BA0 (one report).

## 2D smoothing attempt, builds a9-a13 (2026-11-01): no visible gain, stopped
- a9: h3d scale +0x1F0 blended (model matrix 0x16A130 = scale +0x1F0, rotation +0x1C0, position +0x1B4); EVE_2dTex state words moved by 4 (+0x40, +0x44, +0x4C). The stats zoom verdict is still open. +0x28 is the fade step, as in SMT IV.
- Detector (-DQDIAG, `SMT4_REGION=apoc python tools/gdb/qd_dump.py`, reports in tools/gdb/apoc_qd): "Enter A" banner drew nothing it can see. Reported tasks: 0x2E7A8C (bottom panel), 0x2E169C (bottom bar), 0x4A7260 / 0x4A7588 (Compendium), 0x2BA754 (map screen). QD_WALK sites fixed in port.py (cell renderer frame 104 words here).
- a11-a13 tried CI_OWN owners (regions.py native_own), float y for kind 1, and for a13 larger, probed occurrence tables. Owner counters (tools/gdb/apoc_ci.py): panel owners blended 0, nearly all rejected as "already 60 Hz". Owner sites stayed on the report and a13 added a glitch when the map returns. The collision theory is unproven; a13 counters were never read.
- Installed build is back to a9 (`test_builds/9_full_zoom`). The a11-a13 source changes are uncommitted.
- Next: stop inferring from counters. Record what is drawn frame by frame, or measure a clip (tools/cadence.py, top screen only for now), then blend the one source field, as SMT IV did for the navi slide (WDEF).

## Static task names (tools/apoc_tasknames.py, 2026-11-01)
- Names come from pc-relative strings next to function pointers, so neighbours can share a name: treat them as leads. 0x2E7A8C PPANEL DDRAW (party panel, not the map), 0x2E169C ButtonHelp, 0x2BA754 NAVI DOWN, 0x235CE0 NAVI PROC, 0x14F3F4 MESS, 0x4A6E7C BIBLE DEVIL CALC (Compendium, near detector tasks 0x4A7260 / 0x4A7588), 0x329A90 SEARCH2D / SEARCH3D, 0x3581BC SEARCH2D_D, 0x29D7F8 MIF3DL / MAP FADE, 0x315D34 MAPIF U / MAP FADE.
- Map: NAVI tasks call 0x492088 / 0x4920D4 with *(0x587F28)+0x68; the module around 0x484000-0x4A8000 is new. SMT IV's slide step (+0x1A4 += +0x22A4, USA 0x1FD6F8) has no direct match there; the scan found only per-tick counters plus float steps at 0x486530 (+4, +8) and 0x488F74 / 0x48CB8C (+0x40).

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

## b14 main-menu fix (2026-11-04; confirmed on a New 3DS for Apocalypse, 2026-11-05)
- Report: b13 works on a New 3DS (right stick too); main-menu items stay 30 Hz in both games.
- The camp (main) menu draws its items through the CellPut family, and its owners were not in CI_OWN. Tasks: Apocalypse "Camp DDRAW" 0x227418, "Camp DBaseDraw" 0x25EC04, "Camp DrawUpLeft/Right" 0x202450, Calc 0x3D56DC (registration at 0x2C3424). SMT IV registers the same set at 0x28A718 under the names "TMD CALC" 0x32CB08 / "Camp DrawUp" 0x18B1A4 / "TMD DDRAW" 0x1A001C.
- b14 adds owners. SMT IV CI_OWN: 0x1A001C-0x1A06D4 and 0x197054-0x197D2C. Apocalypse native_own: 0x21775C-0x218640, 0x288538-0x288B20 and 0x227294-0x227418. Only ext6 changed; the HW code.ips is the same as b13. Top-screen camp helpers and depth-2 submenus were left out.
- Test files: test_builds/b14_menu_actual_3ds_{apoc,smt4}. If it fails, go back to b13 (test_builds/b13_actual_3ds).
- A/B on hardware (plugin only swapped): b13 main menu at 30 Hz, b14 at 60 Hz. SMT IV USA b14 on a New 3DS: main menu at 60 Hz (2026-11-05).
