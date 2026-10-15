## SMT IV 60 FPS Patch 1.0

First public release. Shin Megami Tensei IV renders at 60 fps while the game logic keeps its original 30 Hz, so the game runs at normal speed. In-between frames are interpolated. Works in the Azahar emulator and on an actual 3DS (Luma3DS).

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
- **Azahar_full**: for the Azahar emulator. Copy both `exheader.bin` and `exefs/code.ips`.
- **Actual_3DS**: for a real 3DS with Luma3DS 13.0 or newer. Copy the `luma` folder to the SD card, turn on *Enable game patching* in the Luma config and *Plugin Loader* in the Rosalina menu.
- **Cheats**: optional ghosting fix and bloom removal (from the [GBAtemp thread](https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/)), for Azahar and Luma3DS. On a 3DS they also help performance.

### Smoothed (Azahar and actual 3DS)
Camera and 3D characters, clean camera cuts in cutscenes, sprite animations, menus and slides, maps, area select and event backdrop pans, event portraits (slides, fades, zooms, Burroughs), compendium / status / talk panel / battle HP bars, battle effects, model zoom, torches and flames, right-stick camera.

### Actual 3DS notes
- Tested on a New 3DS with the USA 1.0 game; other versions are untested on hardware.
- The 3DS cannot hold 60 fps everywhere. When frames drop, the game falls back to 30 fps at full speed instead of slowing down. Turning the 3D slider off, the New 3DS 804 MHz clock + L2 cache and the cheats help.

### Known limitations
- Pre-rendered video cutscenes keep their encoded frame rate.
- Outside battle, flame / glow effects keep the original 30 Hz frames on purpose.
- Bottom-screen map markers move in whole pixels.
- Save states made with another version of the patch keep running that version.

The whole game has not been play-tested. Please open an issue if you run into anything.
