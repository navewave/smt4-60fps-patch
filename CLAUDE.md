# SMT IV 60 FPS patch: guide for Claude Code

This project is a 60 fps patch for **Shin Megami Tensei IV** (3DS, USA, title `00040000000E5C00`).

- Game logic stays at 30 Hz, so the original speed is kept. The patch renders 60 Hz and interpolates the in-between frames.
- The patch is a binary IPS against the decompressed `.code`. Patch code is C plus ARM asm, compiled with clang/lld for ARMv6K + VFPv2 (hard float).
- The owner tests every build in **Azahar** (Windows) and on a **New 3DS (Luma3DS)**.
- Last stable: **build 7d** (tag `v7d-stable`). Build 8 = public v1.0 assets as first uploaded (Azahar full + core-only Real_3DS).
- Current: **build 12** = 8 + the full extension on an actual 3DS (3GX plugin, `src/plugin_main.c`), frame-time logic gate (hardware), clean cutscene camera cuts (ca_step). Tested: Azahar USA 1.0, New 3DS USA 1.0. Other versions build but are untested.
- Versions: `tools/regions.py` (title IDs, files). Region game files live in `Other Regions Builds/` (private, gitignored).

## Directory map
| Path | What |
|---|---|
| `patch/phase3b.S` | Core asm hooks: logic gate, task gate, dt scaling, frame begin, sprite animator wrappers. HW-safe. |
| `patch/interp_arm.c`, `patch/interp_thumb.c`, `patch/interp.h` | Core C (camera/node/sprite interpolation). Lives in the 2.5 KB text cave and is nearly full (2516/2520 bytes). HW-safe. |
| `patch/interp_ext.c` | **Extension** (Azahar: full code.ips; actual 3DS: written by the plugin). Hooks installed at runtime, generic 2D quad smoothing, field tiles, rotation, scale, material-anim classification, battle detection, diorama cameras (dc_*), event portraits (pt_*). |
| `patch/history/` | Older snapshots of interp_ext.c: 5t, 6b, and rejected experiments (5w_af, 5x, 5s_rejected, 5z4). |
| `src/cam_stick.c` | Right-stick camera mod; lives in the extra page at 0x4F1000. |
| `src/plugin_main.c`, `src/plugin_gate.c` | Luma3DS 3GX plugin for an actual 3DS: writes (Azahar code.ips minus HW code.ips) at boot, sets S->emu + EXT_HW, installs the frame-time logic gate. |
| `tools/build_plugin.py NAME` | Builds `out/NAME_plugin.3gx` and `out/NAME_exheader_hw.bin` (bss +0x5000) from `out/NAME_code.ips` / `_code_hw.ips`. |
| `tools/build.py` | Build library: compile, link, emit IPS. `SMT4_VER=1.1` selects version 1.1. |
| `tools/build_release.py NAME` | Builds `out/NAME_code.ips`, `NAME_exheader.bin`, `NAME_60fps_only.ips` and `NAME_code_hw.ips`. |
| `tools/build_phase1.py` | Hook site table (SITES/BOOT). build_release execs its top part. |
| `tools/old/` | Historical phase builders and scan scripts (reference only). |
| `v11/` | 1.0→1.1 address porting: `port.py`, `mapnew.py --write`, `addrmap.json`. |
| `game/v10`, `game/v11` | The owner's own decompressed code.bin + exheader.bin. **Do not publish** (copyrighted game code). |
| `re/` | Ghidra project, exported decompilation (`dump/decomp.c`, 24 MB), disassembly (`dump/text.asm`), Ghidra scripts. Not for publishing either. |
| `releases/` | Release zips (local only, gitignored; published on GitHub Releases). |
| `docs/notes/` | Detailed research notes. Start with `SMT4_60fps_findings.md`. |
| `docs/RE_SETUP.md` | How to connect Ghidra (MCP) and Azahar's GDB stub for runtime debugging. |
| `tools/gdb/` | GDB probe scripts for Azahar's stub (`sh tools/gdb/gdb.sh tools/gdb/X.gdb`): ping, tasks (live task list), watch/log probes; `whichbuild.py` reports which out/*_code.ips is running. |
| `tools/cadence.py` | Per-region 30/60 Hz analysis of an OBS clip. |
| `re/dump/taskfuncs.txt` | Task function -> task name map (from the registration calls). |

## Build
Requirements: Python 3, LLVM/clang with `clang`, `ld.lld`, `llvm-objcopy`, `llvm-objdump` and `llvm-nm` on PATH.
```
python3 tools/build_release.py phase10                      # USA 1.0
SMT4_REGION=us11 python3 tools/build_release.py phase10     # also eu, jp, kr, tw -> out/phase10_<region>_*
python3 tools/build_plugin.py phase10                       # actual-3DS plugin + exheader (per region, after build_release)
python3 tools/port.py <region>                              # regenerate ports/<region>.json after adding addresses
python3 tools/verify_region.py                              # check runtime hook points in every version
python3 tools/package.py 10 --release 1.0                   # releases/SMT4_60fps_v1.0_<VERSION>.zip: Azahar_full, Actual_3DS (luma/ SD layout), cheats, README
# release zips are not committed: they are attached to GitHub Releases (notes: docs/RELEASE_NOTES_v1.0.md)
```
- New game addresses in the sources must be USA 1.0 addresses; re-run `tools/port.py` for every region afterwards.
- Patch memory moves per version: STATE = .bss end + 4 (JPN 0x4F0D18, TWN 0x4F0D38), pages 0x4F0000.. shift +0x1000 for JPN/TWN. Core cave: EUR has only 2400 bytes (core is 2376).
- Toolchain: native Windows works (clang 22.1.8). It does **not** reproduce the 6b release bytes (smaller code; hook sites identical). Builds from 7a on are the clang 22 baseline.
- The build prints its region sizes. Hard limits:
  - core ≤ 2520 bytes (1.0)
  - ext ≤ 3840, ext3 ≤ 2304, ext4 ≤ 3584, ext5 ≤ 3072
  - Data pages are fixed: see the memory map.

## Install (for testing)
- **Azahar:** `Azahar/load/mods/00040000000E5C00/exheader.bin` + `.../exefs/code.ips`.
  - The full IPS **requires** the matching exheader; it raises bss by 0x3000 (since 7b; 6b used 0x2000).
  - Without it Azahar logs `Failed to load ROM (Error 12)`.
- **Actual 3DS:** `/luma/titles/<tid>/code.ips` (`_code_hw.ips`) + `/luma/titles/<tid>/exheader.bin` (`_exheader_hw.bin`) + `/luma/plugins/<tid>/smt4_60fps.3gx` (one .3gx only). Luma "Enable game patching" on, Rosalina Plugin Loader on. The owner's console (New 3DS) runs **1.0** code. Crash dumps: `SD:/luma/dumps/arm11/*.dmp` (header 40 bytes, then regs r0-r15, cpsr, dfsr, ifsr, far, ...; process name at the end).

## Architecture in brief
- **Logic gate:** frame parity is set at the top of the main loop (logic frames are even). Input, fades, clock and sound run only on logic frames. The task scheduler skips logic tasks (prio < 0x65, and 0x67) on in-between frames; draw tasks run every frame. AsdModelCalcProc and AsdMdlManCalcProc are whitelisted to run every frame.
- **g_dt** (STATE+8) is 1.0 in 30 Hz tasks and 0.5 in every-frame tasks. Per-frame steps (anim UpdateFrame, crossfade) are multiplied by it.
- **Interpolation scheme:** on a logic frame the scene is drawn at the midpoint between the previous and the current tick; the true values are restored at scheduler run-end (0x1031F4). On in-between frames the true current state is drawn.
- **Phases:**
  - phase 0 = BeforeDraw1L (0x103B58, prio 0x65), before AsdModelCalcProc (0x67)
  - phase 1 = CAMPDL draw (0x108A64)
  - run-end restore = 0x1031F4
- **Save states** hold the whole memory, patch included: a save state made with an older build runs the older patch. Verify the loaded build (`python tools/gdb/whichbuild.py`) before judging a test.
- **IMPORTANT RULE:** `ext_watch_swap` (phase 0) runs **only on logic frames**, because the core `spr_swap` returns early otherwise. Ext features must record their "previous" value on the logic frame and blend between the last logic-frame value and the current one. Five past attempts failed because they recorded on in-between frames, which never run there. Hooks on draw/quad functions do run every frame.
- **Core vs. extension:**
  - The core sits in the text cave and works on hardware.
  - The extension uses memory beyond the original code buffer (0x4F0000+, the extra pages from the exheader) and runtime-patched branches (svc 0x93 icache flush).
  - Azahar: the full code.ips writes it. Actual 3DS: Luma cannot IPS past the code image and enforces execute-never, so the 3GX plugin (Luma's loader maps it at 0x07000000 and sets the process RWX) writes the same bytes at boot. Plugin sets EXT_HW (STATE+0x1C4): the extension then uses svc 0x92 + 0x94 (whole-cache) when patching code. Never by-address cache svcs on hardware: Luma runs 0x91/0x93 on every core and faults in the kernel (crash in "qtm").
  - Frame-time logic gate (plugin only): logic runs when two vblanks of real time have passed, so dropped frames do not slow the game; every-frame dt comes from STATE+0x1C0.
  - `S->emu` comes from svcGetSystemInfo(0x20000).
  - `CAMPAGE==1` means magic 'CCAM' was found at 0x4F1000, i.e. the exheader is present.

## Memory map (1.0)
| Address | Use |
|---|---|
| 0x3FE628–0x3FF000 | core code cave |
| 0x4EFE38 | STATE (`State` in interp.h): counter, logic, dt, camera prev/cur/true, node recs, … |
| 0x4F0000–0x4F0F00 | ext code |
| 0x4F0F00 | ext data |
| 0x4F1000 | camera page: magic, cfg, cam code, ext3 code 0x4F1580–0x4F1E80, cam state 0x4F1F00 |
| 0x4F2000–0x4F2E00 | ext4 code |
| 0x4F1E80 | diorama camera records (DcRec[2]) |
| 0x4F2E00 | scale records (SclRec[12]); SCL_ANY at 0x4F2FF0 |
| 0x4F3000–0x4F3C00 | ext5 code (dc_swap, pt_swap) |
| 0x4F3C00 | event portrait records (PtRec[12], 80 bytes each) |
| 0x4EFFF8 | every-frame dt word for the frame-time gate (plugin; STATE+0x1C0) |
| 0x4EFFFC | EXT_HW byte: extension running on hardware (plugin; STATE+0x1C4) |
| 0x4F3FCC / 0x4F3FD0 | camera log / anim-name table heap pointers (CAMLOG builds only) |
| 0x4F3FD4 | CAMANIM_TICK: last tick a cutscene CameraAnimation stepped |
| 0x4F3FD8 | CutRec (cam_cut history, 40 bytes, up to 0x4F4000) |
| 0x4F2FB0 | CaRec (ca_step: held step, cut cache, 16 bytes) |

Ext data words:
| Address | Use |
|---|---|
| 0x4F0F00 | BgRec |
| 0x4F0FA0 | SUBPX |
| 0x4F0FA8 | MIF heap |
| 0x4F0FB0 | SUBPX_ON |
| 0x4F0FB4 | Q heap |
| 0x4F0FB8 | NOGEN |
| 0x4F0FC0 | SH heap |
| 0x4F0FC4 | CAMPAGE |
| 0x4F0FC8 | X3 heap |
| 0x4F0FCC | ROT_ANY |
| 0x4F0FD8 | FovRec |
| 0x4F0FF0 | material-class cache heap |
| 0x4F0FF8 | IN_BATTLE |

## Key game facts (USA 1.0 addresses)
- Engine: NW4C (nw::gfx) under Atlus AsdSDK. Code is mostly ARM; VFP hard-float args.
- Task scheduler:
  - Run loop at 0x103070.
  - Task struct: +0x18 name hash, +0x20 prio, +0x28 handle (`id | prio<<20`).
  - Register with FUN_002C5978 / FUN_002C4C00.
  - TaskAlive: FUN_002BF41C.
- Game camera: FUN_002C5218 = `*(*(0x44EA58+4)+0x80)` → +4 pos, +0x10 target, +0x1C up, +0x28 fov.
  - SetLookAt FUN_002C20A8, CamSetProj FUN_002C1FFC(cam, 1, fov, aspect, near, far), stereo FUN_002BCBB0.
- NW4C node (model+0x2C): local 3x4 matrix at +0x4C (translation +0x58/+0x68/+0x78), scale at +0x7C, flags at +0x88 (0x800 = dirty).
  - SetPos FUN_0028C8A8 / FUN_0029A2B0, SetScale FUN_0025C0F0.
- Anim UpdateFrame 0x36880C: frame +0x38, prev +0x3C, step +0x40, re-evaluate flag +0x58, ResAnim +0x54.
  - vtables 0x420D6C / 0x4210B0 / 0x4211B8.
  - Curve evaluator FUN_00235794; member evaluator FUN_003E8E64 (type 7 = texture pattern).
- 2D: every quad goes through FUN_002B1590 (x s0, y s1, w s2, h s3, uv s4–s7). Sprite animator FUN_002BF9F0.
- Battle work: *(0x435D90). Task handle at +0, teardown flag at +0x60.

## Feature status (6b)
- **Smooth:**
  - 3D characters and camera, rotation, model scale (stats/load zoom)
  - menus and slides (generic quad), map, COMBG background
  - field 2D tiles
  - battle material effects (heat haze)
  - right-stick camera
- **Whole frames on purpose, i.e. original 30 Hz look:**
  - material/visibility anims outside battle (keeps torches and fire billboards correct)
  - flipbook-like materials in battle
  - EPL particle positions
- **Camera cuts (12):** cutscene cameras are NW4C CameraAnimation ("Ca") on the scene camera, written inside the scene update (ModelCalc), after cam_swap/dc_swap, so no blend of ours shows: they are smooth only through half anim steps. At a cut the k+0.5 sample is a camera between two shots. updf_class class 2 -> ca_step: on a logic frame whose sample touches a cut (curves evaluated with FUN_00235794 at whole frames k-2..k+3; jump > 2 and > 6x its neighbours) the step is 0 and the next in-between frame takes the whole step. Build 10 gave them whole frames instead: cutscenes dropped to 30 Hz (reverted). Diagnostic: `SMT4_CFLAGS=-DCAMLOG` + `tools/gdb/cam_dump.py` (--anims lists half-stepped anims).
- **Fixed in 7a–7d** (see `docs/notes/SMT4_7d_review.md`): diorama backdrops (camera tween FUN_00341C74), event portraits (EVE_2dTex, EVE_BustupTex) incl. Burroughs, expression crossfades.
- **Not fixable:** pre-rendered moflex videos (`Move` task).
- **Open candidates** from the 7d review: battle background following the battle camera, player lights, 2D zooms / vertex-colour fades outside the sprite animators.
- **Open items (after build 12, 2026-10-14):**
  - Stats screen bars end early, show a random frame and loop on the N3DS (not investigated; check Azahar first, then the 30 Hz detector / anim classes).
  - N3DS consistency: "steady frame-rate" mode in plugin_gate.c (lock vsync to 30 fps on repeated drops, retry 60 every ~2 s). Measurement build (CPU vs GPU) not done. Busy menus (Y menu over the field) fall to 30 fps on N3DS: performance, not missing smoothing (detector found nothing in Azahar).
  - README: warn that the USA 1.1 zip is only for consoles with the eShop update (the owner's N3DS crashed at boot with the 1.1 files on a 1.0 game: same title ID, wrong code.ips).
  - Actual-3DS builds of EUR/JPN/KOR/TWN/USA 1.1 are untested on hardware; Azahar tested on USA 1.0 only.
  - Apocalypse port: docs/notes/APOC_status.md.

## Working with the owner
- The owner tests and reports by eye ("30 Hz", "jumpy", "flicker"). Short 60 fps clips are recorded with Azahar → Tools → Dump Video or OBS. The clips are AV1: decode with `ffmpeg -c:v libdav1d`. Frame-difference per screen region shows what updates only every 2nd frame.
- Keep a known-good release to fall back on, and ship one test build at a time with a clear "what to check" list.
- Diagnostic probe builds (deliberately visible effects such as freezing something or jumping it) answer "is this the right code?" quickly.
- Never repeat a fix built on an assumption that has not been confirmed: if a fix has no visible effect, probe first.
