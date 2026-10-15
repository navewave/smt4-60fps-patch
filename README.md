# SMT IV 60 FPS Patch

A 60 fps patch for **Shin Megami Tensei IV** (Nintendo 3DS), for the Azahar emulator and for an actual 3DS.

The game normally renders at 30 fps. This patch renders at 60 fps while keeping the game logic at its original 30 Hz, so the game runs at its normal speed. The in-between frames are interpolated: cameras, 3D characters, menus, slides, backdrops and event portraits move smoothly instead of every other frame.

## Download

Get the zip for your game version from the **[Releases](../../releases)** page. Each zip contains a `README.txt` with step-by-step install instructions.

| Version | Title ID | Zip |
|---|---|---|
| USA 1.0 | `00040000000E5C00` | `SMT4_60fps_v1.0_USA_v1.0.zip` |
| USA 1.1 update | `00040000000E5C00` | `SMT4_60fps_v1.0_USA_v1.1.zip` |
| Europe | `0004000000141C00` | `SMT4_60fps_v1.0_EUR.zip` |
| Japan | `0004000000088A00` | `SMT4_60fps_v1.0_JPN.zip` |
| Korea | `00040000000F6100` | `SMT4_60fps_v1.0_KOR.zip` |
| Taiwan | `00040000000D6D00` | `SMT4_60fps_v1.0_TWN.zip` |

## Install (short version)

**Azahar (emulator), recommended:** copy the `Azahar_full/<title id>` folder into `Azahar/load/mods/`. Both `exheader.bin` and `exefs/code.ips` are required.

**Actual 3DS (Luma3DS):** copy the `luma` folder from `Actual_3DS` to the root of your SD card. See [Actual 3DS setup](#actual-3ds-setup) below.

## What is smoothed

Azahar and an actual 3DS run the same patch with the same features.

| Feature | Azahar | Actual 3DS |
|---|:---:|:---:|
| 60 fps with original game speed | ✅ | ✅ |
| Camera and 3D character movement | ✅ | ✅ |
| Clean camera cuts in 3D cutscenes | ✅ | ✅ |
| Sprite animations (menus, slides) | ✅ | ✅ |
| Generic 2D menu smoothing, maps, field markers | ✅ | ✅ |
| Area select / event backdrop pans | ✅ | ✅ |
| Event portraits (slides, fades, zooms, Burroughs) | ✅ | ✅ |
| Compendium, status, talk panel, battle HP bars | ✅ | ✅ |
| Battle effects (heat haze, glows), model zoom | ✅ | ✅ |
| Torches and flames | ✅ | ✅ |
| Right-stick camera control (New 3DS / Circle Pad Pro) | ✅ | ✅ |
| Full game speed when frames drop | — | ✅ |

## Actual 3DS setup

Tested on a New 3DS with Luma3DS and the USA 1.0 game. The other versions are built the same way but have not been tested on hardware yet. An Old 3DS has not been tested and is likely too slow.

1. **Luma3DS 13.0 or newer** (it has the plugin loader built in).
2. Copy the `luma` folder from `Actual_3DS` to the root of the SD card. You should end up with:
   - `SD:/luma/titles/<title id>/code.ips`
   - `SD:/luma/titles/<title id>/exheader.bin`
   - `SD:/luma/plugins/<title id>/smt4_60fps.3gx` (keep only one `.3gx` file in that folder)
3. **Luma config** (hold SELECT while powering on): turn on *Enable game patching*.
4. **Rosalina menu** (L + Down + Select on the HOME menu): set *Plugin Loader* to Enabled.
5. **New 3DS:** in Rosalina's *New 3DS settings*, set the 804 MHz clock and turn the L2 cache on.
6. Start the game from the HOME menu. The top screen flashes blue for a moment when the plugin loads.

To uninstall, delete the three files.

### Performance on an actual 3DS

The 3DS is not fast enough to hold 60 fps everywhere, so expect frame drops in busy scenes (large areas, some battles, menus over the field). When that happens the patch drops to 30 fps at full game speed instead of slowing the game down, and smoothing has less to work with there. To get more consistent frame rates:

- Turn the **3D slider off**. Stereo 3D draws every frame twice.
- Use the **804 MHz clock and L2 cache** (step 5 above).
- Enable the **optional cheats** below. They remove full-screen effects, which lightens the GPU load.

## Known limitations

- Pre-rendered video cutscenes stay at their encoded frame rate.
- Outside battle, flame / glow / scrolling-texture effects keep the original 30 Hz frames on purpose (this keeps fire billboards correct).
- Bottom-screen map markers move in whole pixels.
- Save states made with an older version of the patch keep running that older version.

## Optional cheats

Each release includes the **ghosting fix** and **bloom removal** codes from the GBAtemp thread
[Shin Megami Tensei IV and IV:A Ghosting Fix](https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/), with credit to its author. They work alongside the patch, in Azahar's cheat window or in Luma3DS (Rosalina menu > Cheats). On an actual 3DS they also help performance.

## Not fully play-tested

I have not played through the whole game with the patch. If you run into a glitch, a crash or something that still looks like 30 fps, please [open an issue](../../issues) (game version, Azahar or 3DS, and where it happened) and I'll do my best to fix it.

## Building from source

You need your own dump of the game (the decompressed `code.bin` and `exheader.bin`); no game data is included in this repository.

Requirements: Python 3 (with `numpy`), LLVM/clang with `clang`, `ld.lld`, `llvm-objcopy`, `llvm-objdump`, `llvm-nm`.

```
python tools/build_release.py phase10                     # USA 1.0 (game/v10/): Azahar patch + exheader, Actual 3DS code.ips
python tools/build_plugin.py phase10                      # Actual 3DS plugin + exheader (needs the build above)
SMT4_REGION=eu python tools/build_release.py phase10      # us11, eu, jp, kr, tw: see tools/regions.py (same for build_plugin.py)
python tools/package.py 10 --release 1.0                  # release zips (releases/)
```

How it works and how to debug it: [`CLAUDE.md`](CLAUDE.md) (architecture and memory map), [`docs/`](docs) (research notes, emulator debugging setup).

## Legal

This repository contains only original patch code and tools. It does not contain any game code, assets or ROMs. *Shin Megami Tensei IV* is © ATLUS. This project is not affiliated with or endorsed by ATLUS or SEGA. Please use your own legally obtained copy of the game.

## License

The patch code and tools are released under the [MIT License](LICENSE).
