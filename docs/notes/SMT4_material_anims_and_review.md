# Material animations, torches, battle effects; code review notes (5p → 6b)

## Torch/flame cause (5p)
- The core hook hk_updframe (0x36880C) multiplies every NW4C UpdateFrame step by g_dt (0.5 in every-frame tasks).
- Material anims (flipbook-like UV steps plus alpha curves on torch and billboard flames) then sit on fractional frames, and the image and transparency drift apart.
- Fix: ext_updframe replaces `b hk_updframe` at runtime (when CAMPAGE==1). updf_class(anim) decides the step:
  - Anim object: anim+0x08 → AnimGroup → +0x08 ResAnimGroup → name BinString at +0x08. anim+0x54 = ResAnim, target-group name BinString at +0x0C.
  - "Vi" anims get whole frames: a full step on logic frames and 0 in between.
  - "Ma" anims get whole frames outside battle. In battle, mat_pattern decides (see below).
  - Everything else (skeletal, camera, …) gets step × 0.5.

## Material rules history
| Build | Rule | Result |
|---|---|---|
| 5p | every Ma/Vi anim gets whole frames | torches and billboards OK; battle heat ribbons 30 Hz |
| 5z | whole frames only for a type-7 (texture pattern) member | torches and billboards broke |
| 5z2 | plus any step curve segment | torches OK, ribbons smooth, billboards jumpy |
| 5z3/5z4 | plus dense linear/hermite keys (a key every ≤ 3, then ≤ 8 frames) | billboards still wrong |
| 6a | safe: all Ma anims on whole frames | ribbons 30 Hz, which the owner found unacceptable |
| **6b** | **5z4 rules in battle only; safe rule elsewhere** | everything correct |

- Battle detection, battle_check() at phase 0:
  - w = *(0x435D90), handle = *w ("BTLMAIN C").
  - Requires (h >> 20) ≤ 0x10, TaskAlive(h) (FUN_002BF41C) and *(u8*)(w+0x60) == 0.
  - The result is stored in IN_BATTLE (0x4F0FF8).
- mat_pattern/mem_stepped data layout:
  - Member: flags at +0, type at +8 (0 float, 1 int, 2 bool, 3 vec2, 4 vec3, 5 transform, 6 rgba, 7 texture). Component curves at +0xC+4i (self-relative). Flags bit i = constant; bit (ncomp+i) = absent.
  - Curve (FUN_00235794): flags at +0xC (bit0 compound, bit1 skip, bit2 single segment), count at +0x10, segment offsets from +0x14.
  - Segment: start frame at +4, flags at +8 (bit0 constant; bits 2–4 interpolation via table 0x471BC0: 0 step, 1 linear, 2 hermite, 3 special), key block at +0xC (u16 count first).
  - Results are cached per ResAnim (128 entries, heap pointer at 0x4F0FF0).

## The phase-0 bug (found in the 5t review)
- `ext_watch_swap` (phase 0) is only reached on logic frames: the core `spr_swap` returns early otherwise.
- Ext features must record their "previous" value on the logic frame and blend between the last logic-frame value and the current one.
- The attempts that recorded on in-between frames (5q fov, 5r, 5u, 5v, 5x) never ran.

## Cleanup in 5y/6b
- Removed: the EPL per-particle record hook, epl_swap/restore and the EPL allocation patches.
- Kept: epl_reset_corners. EPL draws transform the quad corners in place, so they are reset every frame.
- Deleted: the #if 0 blocks and diag code. QRec trimmed to 24 bytes.

## Stats/load-screen zoom jitter (5t)
- The zoom scales the model (SetScale FUN_0025C0F0) and moves the camera each tick. The camera was blended but the scale was not, which caused the jitter.
- Fix: a runtime hook on SetScale (0x25C0F0, word 0xE590002C) records the pre-scale on logic frames. scl_swap draws the midpoint at phase 0; scl_restore puts the true scale back at run-end.
- Rejected 5s: detecting cameras moved outside logic made the field camera jittery (the right-stick camera mod moves the camera every frame).

## Right-stick camera mod (src/cam_stick.c)
- Page at 0x4F1000 (magic 'CCAM'). Hooks:
  - 0x2645CC: yaw, controller 1 (FUN_00264368)
  - 0x2D22FC: yaw, controller 2 (FUN_002D1DA0)
  - 0x266284: tilt (placement)
  - 0x25C484: tilt in FUN_0025C470 → SetLookAt
- Opens ir:rst through the game's own srv wrapper. The exheader adds ir:rst and +0x2000 bss.

## 1.1 port
- (old v11/mapnew.py, removed; now tools/port.py for every version) mapped new constants: text by masked instruction windows, rodata/data by literal-pool votes.
- Manual entries are in tools/build.py. Data/bss layout is unchanged in 1.1.
- The 1.1 build was verified with matching hook words but has not been play-tested.
