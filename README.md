# SMT IV 60 FPS Patch

A 60 fps patch for **Shin Megami Tensei IV** and **Shin Megami Tensei IV: Apocalypse** (Nintendo 3DS) that works in the Azahar emulator and on an actual 3DS.

The game normally renders at 30 fps. This patch renders at 60 fps but keeps the game logic at its original 30 Hz, so the game still runs at normal speed. The in-between frames are interpolated, so cameras, 3D characters, menus, slides, backdrops and event portraits move smoothly instead of updating every other frame.

## Download

Download the zip for your game version from the **[Releases](../../releases)** page. Each zip has a `README.txt` with step-by-step install instructions.

### Shin Megami Tensei IV (`SMT4_60fps_…` zips)

| Game version | Title ID | Zip name ends in |
|---|---|---|
| USA 1.0 | `00040000000E5C00` | `USA_v1.0` |
| USA 1.1 (eShop update installed) | `00040000000E5C00` | `USA_v1.1` |
| Europe | `0004000000141C00` | `EUR` |
| Japan | `0004000000088A00` | `JPN` |
| Korea | `00040000000F6100` | `KOR` |
| Taiwan | `00040000000D6D00` | `TWN` |

> [!IMPORTANT]
> USA 1.0 and 1.1 have the same title ID but different code. Use the `USA_v1.1` zip **only** if the 1.1 update is installed. The 1.1 files crash at boot on a 1.0 game.

Testing so far: USA 1.0 in Azahar and on a New 3DS. The other versions are built the same way but have not been tested yet.

### Shin Megami Tensei IV: Apocalypse (`SMT4A_60fps_…` zips)

| Game version | Title ID | Zip name ends in |
|---|---|---|
| USA | `000400000019A200` | `USA` |
| Europe | `00040000001A6B00` | `EUR` |
| Japan (*Shin Megami Tensei IV: Final*) 1.0 | `0004000000166B00` | `JPN_FINAL_1.0` |

Testing so far: USA in Azahar and on a New 3DS. Europe and Japan are built the same way but have not been tested yet. The Japanese 1.1 update is not supported yet.

See [Apocalypse notes](#apocalypse-notes) for what differs from SMT IV.

## Install

Installation is the same for both games. Only the title ID changes.

### Azahar (recommended)

Copy the `Azahar_full/<title id>` folder into `Azahar/load/mods/`, so you end up with:

- `Azahar/load/mods/<title id>/exheader.bin`
- `Azahar/load/mods/<title id>/exefs/code.ips`

Both files are required and must come from the same zip. Without the `exheader.bin` the game won't start (Azahar's log shows `Failed to load ROM (Error 12)`).

### Actual 3DS (Luma3DS)

A New 3DS is recommended. An Old 3DS has not been tested and is likely too slow.

1. Install **Luma3DS 13.0 or newer** (it has the plugin loader built in).
2. Copy the `luma` folder from `Actual_3DS` to the root of the SD card. You should end up with:
   - `SD:/luma/titles/<title id>/code.ips`
   - `SD:/luma/titles/<title id>/exheader.bin`
   - `SD:/luma/plugins/<title id>/smt4_60fps.3gx` (keep only one `.3gx` file in that folder)
3. **Luma config** (hold SELECT while powering on): turn on *Enable game patching*.
4. **Rosalina menu** (L + Down + Select on the HOME menu): set *Plugin Loader* to Enabled.
5. **New 3DS:** in Rosalina's *New 3DS settings*, set the 804 MHz clock and turn the L2 cache on.
6. Start the game from the HOME menu. The top screen flashes blue for a moment when the plugin loads.

To uninstall, delete the three files.

#### Performance on an actual 3DS

The 3DS can't hold 60 fps everywhere, so expect drops in busy scenes (large areas, some battles, menus over the field). When that happens the patch falls back to 30 fps at full game speed instead of slowing the game down. To keep the frame rate up:

- Turn the **3D slider off**. Stereo 3D draws every frame twice.
- Use the **804 MHz clock and L2 cache** (step 5 above).
- Enable the **optional cheats** below. They remove full-screen effects, which lightens the GPU load.

## What is smoothed

Azahar and an actual 3DS get the same features:

- 60 fps at the original game speed
- Camera and 3D character movement, with clean camera cuts in 3D cutscenes
- Sprite animations, menus, slides, maps and field markers
- Area select and event backdrop pans
- Event portraits (slides, fades, zooms, Burroughs)
- Compendium, status screen, talk panel and battle HP bars
- Battle effects (heat haze, glows) and model zoom
- Torches and flames
- Right-stick camera control (New 3DS / Circle Pad Pro)

### Known limitations

- Pre-rendered video cutscenes stay at their encoded frame rate.
- Outside battle, flame, glow and scrolling-texture effects keep the original 30 Hz frames on purpose (this keeps fire billboards correct).
- Bottom-screen map markers move in whole pixels.
- Save states made with an older version of the patch keep running that older version. Load from an in-game save after updating.

### Apocalypse notes

*Apocalypse* runs on the same engine, but much of its code was rewritten, so the port is newer and some features are not available yet:

- Model zoom (status screen, loading) is not smoothed.
- The bottom-screen navigation map and some menu backgrounds stay at 30 Hz.
- Some UI panels that SMT IV smooths (such as the compendium and talk panel) may still update at 30 Hz.
- New systems (partner attacks, some event filter effects) have not been looked at yet.
- **Europe / Japan on an actual 3DS:** the patch code lives in the plugin, so the `code.ips` crashes the game if the `.3gx` plugin is missing or the Plugin Loader is off. Always install all three files together.

Reports from Apocalypse players are especially welcome.

## Optional cheats

Each zip includes the **ghosting fix** and **bloom removal** codes from the GBAtemp thread [Shin Megami Tensei IV and IV:A Ghosting Fix](https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/), with credit to its author. They work alongside the patch in Azahar's cheat window or in Luma3DS (Rosalina menu > Cheats), and on an actual 3DS they also help performance. Setup steps are in the zip's `README.txt`.

## Reporting problems

I haven't played through the whole game with the patch. If you run into a glitch, a crash or something that still looks like 30 fps, please [open an issue](../../issues) with:

- which game (SMT IV or Apocalypse) and version, and whether an update is installed
- Azahar or actual 3DS (and which model)
- where in the game it happened

## Building from source

You need your own dump of the game (the decompressed `code.bin` and `exheader.bin`). No game data is included in this repository.

Requirements: Python 3 (with `numpy`) and LLVM/clang with `clang`, `ld.lld`, `llvm-objcopy`, `llvm-objdump` and `llvm-nm` on your PATH.

```bash
python tools/build_release.py phase10                   # USA 1.0 (game/v10/): Azahar patch + exheader, actual-3DS code.ips
python tools/build_plugin.py phase10                    # actual-3DS plugin + exheader (run after build_release)
SMT4_REGION=eu python tools/build_release.py phase10    # other versions: us11, eu, jp, kr, tw (see tools/regions.py)
SMT4_REGION=apoc python tools/build_release.py phase10  # Apocalypse: apoc (USA), apoc_eu, apoc_jp
python tools/package.py 10 --release 1.0                # release zips in releases/
```

`SMT4_REGION` works the same way for `build_plugin.py`. For how the patch works and how to debug it, see [`CLAUDE.md`](CLAUDE.md) (architecture and memory map) and [`docs/`](docs) (research notes, emulator debugging setup).

## Legal

This repository contains only original patch code and tools. It does not contain any game code, assets or ROMs. *Shin Megami Tensei IV* and *Shin Megami Tensei IV: Apocalypse* are © ATLUS. This project is not affiliated with or endorsed by ATLUS or SEGA. Please use your own legally obtained copy of the game.

## License

The patch code and tools are released under the [MIT License](LICENSE).
