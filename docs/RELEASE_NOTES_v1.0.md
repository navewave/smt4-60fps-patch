## SMT IV 60 FPS Patch 1.0

First public release. Shin Megami Tensei IV renders at 60 fps while the game logic keeps its original 30 Hz, so the game runs at normal speed. In-between frames are interpolated.

### Downloads: pick the zip for your game

| Your game | Zip |
|---|---|
| USA, no update | `SMT4_60fps_v1.0_USA_v1.0.zip` |
| USA with the 1.1 update installed | `SMT4_60fps_v1.0_USA_v1.1.zip` |
| Europe | `SMT4_60fps_v1.0_EUR.zip` |
| Japan | `SMT4_60fps_v1.0_JPN.zip` |
| Korea | `SMT4_60fps_v1.0_KOR.zip` |
| Taiwan | `SMT4_60fps_v1.0_TWN.zip` |

Each zip has a `README.txt` with install steps:
- **Azahar_full**: for the Azahar emulator (60 fps + all smoothing + right-stick camera). Copy both `exheader.bin` and `exefs/code.ips`.
- **Real_3DS**: for a real 3DS with Luma3DS (core 60 fps features).
- **Cheats**: optional ghosting fix and bloom removal (from the [GBAtemp thread](https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/)), for Azahar and Luma3DS.

### Smoothed
Camera and 3D characters, sprite animations, menus and slides, maps, area select and event backdrop pans, event portraits (slides, fades, zooms, Burroughs), compendium / status / talk panel / battle HP bars, battle effects, model zoom, right-stick camera. On a real 3DS only the core part (camera, characters, sprite animations) is available.

### Known limitations
- Pre-rendered video cutscenes keep their encoded frame rate.
- Outside battle, flame / glow effects keep the original 30 Hz frames on purpose.
- Bottom-screen map markers move in whole pixels.
- One-frame camera jump at the start of the terminal teleport scene.
- Save states made with another version of the patch keep running that version.
