# SMT IV 60 fps: diorama backdrops: PARKED (still 30 Hz)

## Symptom
- Area select (Mikado "Where will you go?"), event scenes over a backdrop, and the zoom behind bust-ups.
- The backdrop moves only every 2nd frame (30 Hz).
- Per-region phase correlation of a video shows parallax, so this is a real 3D camera move.
- Burroughs' hologram scale-in is probably the same mechanism.

## Key finding (scene update loop FUN_00115B74 in AsdModelCalcProc, every frame)
- Every frame it loops all scenes and calls SceneUpdater FUN_002C2B34.
- If `*(DAT_00125528+8) & 1` (scene 0) or `& 2` (scene 1) is clear, it calls FUN_00140B2C, which syncs the NW4C camera from the game camera (the core hooks this at 0x140BB4).
- When the flag is **set**, which FUN_00244F20(1/2) does (called from FUN_003418F4 / FUN_002A06DC for diorama and bust-up scenes), it copies only the projection (near/far/aspect/fov) from the game camera. The NW4C camera view then comes from the bccam camera animation on the camera node.

## Code
- Diorama scene object: FUN_002350F4(obj, "mikado/<name>") loads "<name>.bam", "<name>.bccam" (FUN_00341578: NW4C cameras at obj+0x56C.., count obj[0]) and "<name>.bcenv". Camera index at obj+4.
- Camera anim objects are at obj[camIdx*0x20+animIdx+0x17B] (FUN_003418F4, which also sets frame +0x38/+0x3C).
- FUN_00341C74(obj) runs in gated calcs (Mikado FUN_003B87A0, quest/event FUN_00384AC8).
  - It does a 10-step tween of the camera node translation (node+0x58/+0x68/+0x78) and of the green cursor (obj+0x1658 via SetPos).
  - It then calls SetLookAt on the game camera.
- Possibly related: FUN_0022B90C, a tween of +0x70 from +0x78 to +0x80 over counter +0x88/+0x8C in the Mikado calc.
- BustupCutAnim (Burroughs): work *(0x43DFC0), camera at work+4, camera anim *(*(work+0x5E8)+4). Calc FUN_002168D0; draw FUN_002CF8A8.

## Attempts, none with any visible effect
| Build | Idea | Note |
|---|---|---|
| 5q | blend the game camera's fov | never ran (in-between recording bug) |
| 5r | blend the BustupCutAnim camera | never ran (same bug) |
| 5u | blend big game-camera moves (> 150 units/tick) | never ran (same bug) |
| 5v | blend the camera node translation and view-updater vector | never ran (same bug) |
| 5w | midpoint of anim frames stepped in 30 Hz tasks | ran; no effect |
| 5x / 5y | midpoint of the current camera's 32 anim slots, plus Burroughs' camera anim | 5y fixed the recording bug and **still had no effect** |

## Probes (valid)
- D1 (jump the camera node translation on logic frames): no visible effect. The node is overwritten later.
- D2 (skip FUN_00341C74 entirely): only the green cursor froze; the backdrop still panned.
- A1/A2 (snap or freeze all anims stepped at dt == 1.0): nothing visible anywhere in the game, so almost no animation is stepped in 30 Hz tasks.

## Next step
Use runtime debugging (docs/RE_SETUP.md):
1. Break in FUN_00115B74 on the area-select screen and get the active NW4C camera.
2. Watch its view matrix or world position while highlighting a different place.
3. The writer tells you what actually runs at 30 Hz.
