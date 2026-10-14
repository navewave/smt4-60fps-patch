# SMT IV 60 FPS Patch

A 60 fps patch for **Shin Megami Tensei IV** (Nintendo 3DS).

The game normally renders at 30 fps. This patch renders at 60 fps while keeping the game logic at its original 30 Hz, so the game runs at its normal speed. The in-between frames are interpolated: cameras, 3D characters, menus, slides, backdrops and event portraits move smoothly instead of every other frame.

## Download

Get the zip for your game version from the **[Releases](../../releases)** page. Each zip contains a `README.txt` with step-by-step install instructions.

| Version | Title ID | Zip |
|---|---|---|
| USA 1.0 | `00040000000E5C00` | `SMT4_60fps_8_USA_v1.0.zip` |
| USA 1.1 update | `00040000000E5C00` | `SMT4_60fps_8_USA_v1.1.zip` |
| Europe | `0004000000141C00` | `SMT4_60fps_8_EUR.zip` |
| Japan | `0004000000088A00` | `SMT4_60fps_8_JPN.zip` |
| Korea | `00040000000F6100` | `SMT4_60fps_8_KOR.zip` |
| Taiwan | `00040000000D6D00` | `SMT4_60fps_8_TWN.zip` |

## Install (short version)

**Azahar (emulator), recommended:** copy the `Azahar_full/<title id>` folder into `Azahar/load/mods/`. Both `exheader.bin` and `exefs/code.ips` are required.

**Real 3DS (Luma3DS):** copy `Real_3DS/<title id>/code.ips` to `SD:/luma/titles/<title id>/` and enable *Enable game patching* in the Luma config (hold SELECT while powering on). On hardware only the core 60 fps features are available (see below).

## What is smoothed

| Feature | Azahar | Real 3DS |
|---|:---:|:---:|
| 60 fps with original game speed | ✅ | ✅ |
| Camera and 3D character movement | ✅ | ✅ |
| Sprite animations (menus, slides) | ✅ | ✅ |
| Generic 2D menu smoothing, maps, field markers | ✅ | — |
| Area select / event backdrop pans | ✅ | — |
| Event portraits (slides, fades, zooms, Burroughs) | ✅ | — |
| Compendium, status, talk panel, battle HP bars | ✅ | — |
| Battle effects (heat haze, glows), model zoom | ✅ | — |
| Right-stick camera control (New 3DS / Circle Pad Pro) | ✅ | — |

The extra features need code running from memory that a real 3DS does not allow, so they are emulator-only.

## Known limitations

- Pre-rendered video cutscenes stay at their encoded frame rate.
- Outside battle, flame / glow / scrolling-texture effects keep the original 30 Hz frames on purpose (this keeps fire billboards correct).
- Bottom-screen map markers move in whole pixels.
- A one-frame camera jump at the start of the terminal teleport scene.
- Save states made with an older version of the patch keep running that older version.

## Optional cheats

Each release includes the **ghosting fix** and **bloom removal** codes from the GBAtemp thread
[Shin Megami Tensei IV and IV:A Ghosting Fix](https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/), with credit to its author. They work alongside the patch.

## Building from source

You need your own dump of the game (the decompressed `code.bin` and `exheader.bin`); no game data is included in this repository.

Requirements: Python 3 (with `numpy`), LLVM/clang with `clang`, `ld.lld`, `llvm-objcopy`, `llvm-objdump`, `llvm-nm`.

```
python tools/build_release.py phase8                     # USA 1.0 (game/v10/)
SMT4_REGION=eu python tools/build_release.py phase8      # us11, eu, jp, kr, tw: see tools/regions.py
python tools/package.py 8                                # release zips
```

How it works and how to debug it: [`CLAUDE.md`](CLAUDE.md) (architecture and memory map), [`docs/`](docs) (research notes, emulator debugging setup).

## Legal

This repository contains only original patch code and tools. It does not contain any game code, assets or ROMs. *Shin Megami Tensei IV* is © ATLUS. This project is not affiliated with or endorsed by ATLUS or SEGA. Please use your own legally obtained copy of the game.

## License

The patch code and tools are released under the [MIT License](LICENSE).
