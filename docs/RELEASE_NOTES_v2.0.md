SMT IV and SMT IV: Apocalypse render at 60 fps while the game logic keeps its original 30 Hz, so both games run at normal speed. In-between frames are interpolated.

## What's new since 1.0
- **SMT IV: Apocalypse** support, including the Japanese *Shin Megami Tensei IV: Final* (1.0 and 1.1 update).
- **Full patch on an actual 3DS.** The plugin now brings every smoothing fix and the right-stick camera to Luma3DS, not just the core.
- **No setup for the New 3DS clock.** The patch turns on the 804 MHz clock and L2 cache by itself.
- **Cheats included.** The ghosting fix and bloom removal codes (from the GBAtemp thread) install with the patch and stay off until you enable them.
- More smoothing: 60 fps main menus, relic scan, screen fades, battle zoom, clean camera cuts in cutscenes.

## Which zip?
`SMT4_and_SMT4A_60fps_ALL.zip` has every version as a folder. Or download just yours:

| Game | Version | Zip |
|---|---|---|
| SMT IV | USA 1.0 | `SMT4_60fps_USA_1.0.zip` |
| SMT IV | USA 1.1 (update installed) | `SMT4_60fps_USA_1.1.zip` |
| SMT IV | Europe / Japan / Korea / Taiwan | `SMT4_60fps_EUR.zip` / `_JPN` / `_KOR` / `_TWN` |
| Apocalypse | USA / Europe | `SMT4A_60fps_USA.zip` / `SMT4A_60fps_EUR.zip` |
| Final (JPN) | 1.0 / 1.1 (update installed) | `SMT4A_60fps_JPN_FINAL_1.0.zip` / `_1.1.zip` |

Use a 1.1 zip **only** if that update is installed. The 1.1 files crash at boot on a 1.0 game.

## Install
Each zip has a `README.txt` with the full steps.
- **Azahar:** copy everything inside `Azahar_full` into Azahar's user folder (`%APPDATA%\Azahar` on Windows).
- **Actual 3DS (Luma3DS 13.0+):** copy everything inside `Actual_3DS` to the root of the SD card. Turn on *Enable game patching* in the Luma config and the *Plugin Loader* in Rosalina.

## Known limitations
- **Apocalypse on an actual 3DS:** the 3D field and battles run at 60 fps, but the heavy menus (party, stats, demon sorting) stay at 30 fps. Their top-screen drawing is too much for the 3DS at 60 fps. Azahar runs them at 60.
- On an actual 3DS, busy scenes can drop to 30 fps. The patch keeps the game at full speed when that happens.
- On an actual 3DS, the stats screen bars can stop short and replay their animation.
- Pre-rendered video cutscenes keep their encoded frame rate.
- Tested: SMT IV USA 1.0 and Apocalypse USA, in Azahar and on a New 3DS. The other versions are built the same way but haven't been played yet.
