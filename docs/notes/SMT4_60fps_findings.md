# SMT IV (USA, 00040000000E5C00): 60 FPS research notes (reference)

## Binary layout (decompressed code.bin, 1.0)
| Segment | VA | Size |
|---|---|---|
| text | 0x100000 | 0x2FE624 |
| rodata | 0x3FF000 | 0x2BB78 |
| data | 0x42B000 | 0x4B644 |
| bss | 0x476644 | 0x797F0 (to 0x4EFE34) |

- Code cave: zero padding 0x3FE624–0x3FF000 in the last text page (core at 0x3FE628). STATE block at 0x4EFE38.
- 1.1 layout: text ends 0x3FE3F8; rodata 0x3FF000+0x2BF30; data/bss are the same.
- Engine: NW4C (nw::gfx) under Atlus AsdSDK. Toolchain armcc, mostly ARM, VFP hard-float args.
- The exheader override (mods/<tid>/exheader.bin) raises bss by 0x2000, which creates pages 0x4F1000 and 0x4F2000. Azahar rejects an IPS with records past the code buffer.

## Frame pacing
- Display manager at 0x4E39E0:
  - +0x1C0 vsync interval (0 = 60 Hz, 1 = 30 Hz; 0x4E3BA0 is the community cheat address)
  - +0x1C4 frame time float (µs)
  - +0x1BC vblanks elapsed
- FUN_00105DF4(n) sets the interval; it is called at boot (0x1016C0 `mov r0,#1`).
- FUN_0010527C: end-of-frame vsync wait. FUN_00105BAC: overload path.
- No delta time anywhere: all logic is fixed-step per frame.

## Main loop (0x101B80)
| Address | Call |
|---|---|
| 0x101B8C | input FUN_0010B578 (hold counters FUN_001078D8, touch FUN_00108B10) |
| 0x101E0C | screen fades FUN_00102438 |
| 0x101E10 | SE delays, play time, in-game clock |
| 0x101E84 | task scheduler FUN_00102F18 |
| 0x101E98 | sound/stream management |
| 0x10201C | vsync wait |

## Task scheduler
- Manager at *0x4E3CF4. Register with FUN_002C5978 → FUN_00118488(name, prio, startDelay, killDelay, calc, destroy, user); child draw tasks via FUN_002C4C00.
- Task struct:
  - +0x18 name hash (sum of chars)
  - +0x20 priority
  - +0x28 handle (`id & 0xFFFFF | prio << 20`)
  - +0x34 start delay
  - +0x3C calc fn
  - +0x4C next
- Run loop at 0x103070. TaskAlive(handle) = FUN_002BF41C.
- Priorities:
  - 1–3: logic (MAP C, BTLMAIN C, MESS_C, menus, CamManagerProc = 3)
  - ≥ 0x65: draw
  - 0x67: AsdModelCalcProc / AsdMdlManCalcProc
- Draw order:
  - top-left 0x65–0x85 (BeforeDraw1L 0x65)
  - top-right 0xCC–0xE4
  - bottom 0xE5+

## Animation / models (NW4C)
- UpdateFrame 0x36880C: step +0x40, frame +0x38 = policy(+0x44 start, +0x48 end, frame + step), prev +0x3C, re-evaluate flag +0x58, ResAnim +0x54, bound group +0x08.
  - vtables 0x420D6C / 0x4210B0 / 0x4211B8.
- Model manager FUN_00124714: crossfade `+= 1.0` at 0x12477C.
- AsdModelCalcProc = FUN_0010AED4 → FUN_00115B74 (camera sync FUN_00140B2C at 0x140BB4, scene update FUN_002C2B34).
- Atlus model: NW4C node at model+0x2C.
  - Node: +0x4C local 3x4 matrix (translation +0x58/+0x68/+0x78), scale +0x7C..0x84, flags +0x88 (0x800 = local dirty).
- Setters: SetPos FUN_0028C8A8(model, _, vec3*), SetPos FUN_0029A2B0(model, s0, s1, s2), SetScale FUN_0025C0F0(holder, s0..s2).
- Game camera FUN_002C5218 → *(*(0x44EA58)+4)+0x80: +4 pos, +0x10 target, +0x1C up, +0x28 fov, +0x2C aspect, +0x30 near, +0x34 far.
  - SetLookAt FUN_002C20A8 (stores vectors verbatim), CamSetProj FUN_002C1FFC(cam, 1, fov, aspect, near, far), stereo FUN_002BCBB0.
  - FUN_00298FEC(cam) makes a camera the game camera.

## 2D
- Every 2D quad goes through FUN_002B1590 (x s0, y s1, w s2, h s3, u s4, v s5, du s6, dv s7). Texture at ctx(*0x44EA00)+0x11E4.
- Sprite object (0x8C bytes):
  - +0x0C/+0x10 pos (20.12)
  - +0x20 alpha
  - +0x24 scale track, copied to +0x28/+0x2C
  - +0x4C.. pos keyframes
  - +0x70.. cell flipbook
- Animator FUN_002BF9F0 (297 callers). Variants: FUN_002B9220 slide-in, FUN_0029538C, FUN_00296528. Full sprite draw FUN_002B9708. Cell draw FUN_002BF460.
- COMBG background *(0x43DC24): calc FUN_001A090C, draw FUN_001ECC7C (cells at int pixels; bl sites 0x1ECE64 / 0x1ECF54).
- Navi map *(0x44CDF0): draw bl FUN_0039F238 at 0x1FC4AC. Scroll centre +0x38, slide +0x1A4. Player position *(0x447E88)+0x1C.
- Field 2D tiles (MIF3DL 0x6F, FUN_001E83FC): tile e = *(0x447A78) + i*0x194 + 0x1D0 (128 tiles, active byte at e), 60 vertex floats at e+0x90.
- InfoWin *(0x43E128): text offsets +0x18/+0x1C are ints.

## Effects
- EPL particles: update FUN_001929BC; draws FUN_001AAC60 (3D) / FUN_001AA5A0 (2D); lists at 0x4C5830 (5 × 0x10, next at node+0x20).
- The draws transform the 4 corners at +0x388 in place when (+0x130 & 1) == 0 && +0x380. The template is at +0x26C.

## Status / load screen zoom
- Status Calc 0x18F560. Circle pad held = FUN_002C17D4(0xE/0xF).
- The zoom calls SetScale on the model (FUN_002940AC) and sets the camera pos via FUN_00282F00 on camera *(0x44E178). FUN_002A0DE8 does the SetLookAt.

## Platform research
- Azahar does not check execute permission, so code runs from data/bss/heap. A real 3DS enforces XN, so the core must stay in the text cave.
- Azahar has no svcControlProcessMemory.
- Emulator detection: svcGetSystemInfo(0x20000, 0) returns r1 = 1 (Citra) / 2 (Azahar).
- svc 0x93 = InvalidateInstructionCacheRange, used for runtime hooks.
- Do not reuse "dead" game code: every candidate turned out live.

## Build history (short)
| Build | Change |
|---|---|
| 1 | logic gate |
| 2a–2c | model/camera interpolation |
| 3b | sprite state swapping |
| 4x | animator variants, watch list |
| 5a | extension page |
| 5d | COMBG, field tiles |
| 5e | generic 2D quad hook |
| 5h | camera-mod merge, exheader |
| 5i | node_veto |
| 5j | rotation |
| 5k | ext4 page |
| 5n | 5i quad rules restored (menu stability) |
| 5p | torch fix |
| 5t | stats zoom (SetScale) |
| 5y | cleanup and phase-0 bug fix |
| 6b | battle-only material smoothing (stable) |
