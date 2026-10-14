# 7d review: what was fixed, the pattern behind it, and where else it can occur

## What 7a-7d fixed (all found with Azahar's GDB stub)
| Build | Scene | Root cause | Fix |
|---|---|---|---|
| 7a | Area select / event backdrop pans | `FUN_00341C74` (30 Hz calc) tweens the scene camera node (+0x58/+0x68/+0x78) and the view updater rotation (upd+8, `FUN_0023D51C`). Scene flag bit i of `*(0x464AF8)` disables the game-camera sync. | `dc_swap/dc_restore`: midpoint both on logic frames for `*(0x464B10 + i*0xC)` while flag bit i is set. Records 0x4F1E80. |
| 7b | Burroughs zoom, talk portraits | `EVE_2dTex` children (calc `FUN_001DFB60`) step work +0x10 (int slide), +0x1C (offset), +0x24 (alpha / zoom scale). Draw `FUN_002857F4` every frame. | `pt_swap/pt_restore` |
| 7c | BustupCutAnim portraits | `EVE_BustupTex` (calc `FUN_001CE3A4`): centre +0x34..0x3C, +0x58, half size +0x1C/+0x20, alpha +0x28/+0x88. Draw `FUN_001DBB54`. | PT table generalised; walks the prio-2 task list, 12 records at 0x4F3C00 |
| 7d | Expression change flash | Crossfade start swaps the texture and jumps the old face's alpha +0x88 0 -> 1 in one tick | Per-portrait signature of state words (texture handle, mode, old texture, flag); no blend on a tick where it changes |

Not fixable: `Move` task plays `rom:/moflex/*.moflex` (pre-rendered video). Frame rate is baked in.

## The pattern
A 30 Hz logic task writes plain values into an object, and a draw that runs every frame reads them directly.
Nothing the patch hooks (SetPos, SetScale, sprite animators, quad positions, game camera) is in the path.

Inverse pattern (double speed): a time step inside a draw task. Seen in 6b (XFADEDL, ENCEFFDL, CAMPDL counters).
The static scan found no new case: the only draw-side "tween" hits (`MAP U`, `TMNL UD`) are the 2D layout
renderer `FUN_00281ADC`, which recomputes pane transforms each draw (not time based).

## Static sweep: direct node-translation writers (23 functions)
Handled: `0x140B2C` (game-camera sync), `0x28C8A8`/`0x29A2B0` (SetPos, hooked), `0x341C74`/`0x234E70`/`0x23D4A0` (diorama camera, covered by dc_swap whatever the writer).

Open candidates:
| Function | Driver | What | Risk |
|---|---|---|---|
| `0x34C9F4` | btlCamera Calc -> `0x3DA318` | object `+0x304C` placed at camera anchor + camera-anim offset each tick (probably the battle background following the camera); also writes `*DAT_003da608` +0x90/+0x94 | battle background may lag the 60 Hz camera |
| `0x25CCB8` | field map update (`FUN_002D52C8`) | `Light_Player2/3` light positions follow the player | subtle lighting step only |
| `0x307978` | field map update (`FUN_002767B0`) | camera/target maths, writes node `*(*(DAT_002c5e1c)+0x44)` | field is play-tested smooth; identity of that node unconfirmed |
| `0x2E7204` | vtable method (table 0x464880) | matrix-based placement of a node | unknown owner |
| `0x119D74`, `0x2C07CC` | `0x10797C` | placement from script (likely one-shot) | low |

## Generic gaps in the existing smoothing
- Generic 2D quad hook (`quad_interp`): identity = texture + size + uv, position only. 2D elements that **zoom** (size changes) or **fade via vertex colour** outside the sprite animators stay 30 Hz.
- Sprite animators (`FUN_002BF9F0` and variants): position, alpha, scale track are handled. Sprites moved by other code are not.
- 3D nodes: only SetPos/SetScale/rotation hooks. Direct writers (table above) are not.

## Systems never checked (by name), worth a look in play
NpcRoom (shops/bars), QuestCenter, Unite (fusion), BIBLE (compendium: DEVIL/ANALYZE/LEGIONSKILL), TMNL (terminals),
LMAP (large map), STAFFROLL (credits), SmokeFade / BlinkFade / WhisperEffect (screen effects), btlEncount / BTLFADE,
3dEventOpening, GauntletApp, AOC (DLC), Camp CEC screens, SEARCH2D/3D, DEPART.
