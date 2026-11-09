# SMT IV: Apocalypse in other regions (set up 2026-11-03)

| Key | Version | Title ID | Files (private, gitignored) |
|---|---|---|---|
| `apoc` | Apocalypse USA | 000400000019A200 | `Shin Megami Tensei IV - Apocalypse/` |
| `apoc_eu` | Apocalypse EUR (CTR-P-BG4P) | 00040000001A6B00 | `.../EUR/` |
| `apoc_jp` | SMT IV FINAL JPN 1.0 (CTR-P-BG4J) | 0004000000166B00 | `.../JPN/` |
| `apoc_jp11` | SMT IV FINAL JPN 1.1 (update CTR-U-BG4J, 0004000E00166B00) | 0004000000166B00 (Luma/Azahar use the base ID) | `.../JPN_v11/` |

The CIAs in `Shin Megami Tensei IV - Apocalypse/Other Versions/` are title-key encrypted (NCCH
encrypted as well). The repo has no keys and does not fetch any, so they must be decrypted on
the owner's side first.

## 1. Get decrypted files (once per version)
GodMode9 on the 3DS: copy the CIA to the SD card, select it in GodMode9 > "CIA image options..." >
"Decrypt file (SD output)". The decrypted CIA lands in `SD:/gm9/out/`. Then on the PC:
```
python tools/import_game.py apoc_eu   "<decrypted EUR .cia>"
python tools/import_game.py apoc_jp   "<decrypted JPN 1.0 .cia>"
python tools/import_game.py apoc_jp11 "<decrypted JPN 1.1 update .cia>"
```
Alternative: mount the CIA in GodMode9, "Extract .code" plus copy `exheader.bin`, then
`python tools/import_game.py <key> --code <.code> --exheader <exheader.bin>` (compressed or not).
The tool refuses encrypted input and prints the title ID: check it against the table.

## 2. Port and build
```
python tools/port.py apoc            # only after Apocalypse USA addresses changed
python tools/port.py apoc_eu         # -> ports/apoc_eu.json + ports/apoc_eu_native.json
SMT4_REGION=apoc_eu python tools/build_release.py <name>
SMT4_REGION=apoc_eu python tools/build_plugin.py <name>
python tools/verify_region.py apoc apoc_eu
python tools/package.py <build> apoc_eu      # releases/SMT4A_60fps_<build>_EUR.zip
```
Same for `apoc_jp` / `apoc_jp11`. Read the port.py notes: "function entry maps to a non-entry",
"native site ... expected", "took the shift of" and "owner range dropped" lines need a look.

## Status 2026-11-03: EUR and JPN 1.0 build (core page), untested in play
- Imported from the owner's GodMode9-decrypted .cci images (title IDs match). JPN 1.1 update CIA
  is still encrypted.
- Ports: all 127 Apocalypse USA addresses mapped in both, clear vote winners, verify_region OK.
  EUR code is nearly USA (+0 in text, -0x30 in the UI-view module, data -0x810..-0x1030).
  JPN shifts in small steps through the whole text (separate build). EFF_LISTS (apoc 0x619988)
  is a nearest-neighbour guess in both (EUR +0, JPN -0x4FE0): check before relying on it.
- Text cave too small (EUR 8 bytes: text end 0x524FF0, rodata 0x525000; JPN 1208 bytes: text end
  0x522B40) for the 3788-byte core. regions.py `core_page=True`: build.py links the core at
  CORE_PAGE (0x4F5000 + page delta: EUR 0x652000, JPN 0x64C000) and MEM_END grows a page.
  - Azahar: the full code.ips writes it; build_release raises bss to cover it (buffer ends at
    the page end exactly).
  - Actual 3DS: the core page is part of the plugin's extension diff (plg_main writes it before
    the game's entry point, then installs the gate). code_hw.ips holds only hook branches into it
    (26), so it crashes without the plugin (README note). Gate trampoline stays in the text cave
    (EUR's 8 bytes are exactly enough); gate word is found in the core page. Hardware exheader
    bss +0x7000 (was +0x5000).
  - No `_60fps_only.ips` for these (it needs the core in the cave).
- Test zips (local): releases/SMT4A_60fps_apoc20_EUR.zip, ..._JPN_FINAL_1.0.zip.
  Built from the working tree as of 16:37 (interp_ext.c / interp_arm.c edited 16:06 by another
  session are included).

## How it works
- `regions.py`: the three keys have `base='apoc'` (family). They inherit Apocalypse USA's
  `native_syms` / `native_sites` / `native_own` / `native_consts`, translated through
  `ports/<key>_native.json`.
- `port.py` (class `Chain`): maps every Apocalypse USA address the patch uses (values of
  `ports/apoc.json` + the native entries) into the version with the normal window search
  (whole-file search as fallback), then writes `ports/<key>.json` = SMT IV USA -> Apocalypse USA
  -> version. So all fixes keep being written with SMT IV USA addresses (or Apocalypse USA
  addresses in `regions.py`), and every Apocalypse version follows. Hand fixes go in
  `MANUAL['<key>']` with **Apocalypse USA** addresses as keys.
- Data nobody loads directly (EFF_LISTS, apoc 0x619988) takes the shift of the nearest mapped
  data address (noted in the port output).
- `build.py`: `-DREGION_<KEY>` plus `-DREGION_APOC`, so every `#ifdef REGION_APOC` block applies.
  The raw Apocalypse constants in those blocks (InfoWin 0x57F574, lock flags 0x5F15FC) are
  listed in `native_consts` and translated in the source copies. A new raw Apocalypse address in a
  REGION_APOC block must be added there too.
- Patch memory (STATE, pages) moves with each version's .bss end, as for the SMT IV regions.
- `package.py`: zips SMT4A_60fps_<build>_USA / _EUR / _JPN_FINAL_1.0 / _JPN_FINAL_1.1 (SMT IV: SMT4_60fps_<build>_USA_1.0 ...; build given with or without "phase"). Cheats
  (ghosting fix + bloom removal, same GBAtemp thread) come from the IV:A / FINAL blocks in
  CHEATS.txt (added 2026-11-03): USA and EUR share addresses; FINAL JPN 1.0 and 1.1 share the bloom
  block but have their own ghosting block. Targets checked against USA/EUR/JPN 1.0 code.bin
  (VFP ops / function prologues as expected, no overlap with the b13 / apoc20 IPS); JPN 1.1 not
  checked (still encrypted). The README feature text is still SMT IV's.
- `tools/gdb/cam_dump.py`, `qd_dump.py`: page shift from the region (any Apocalypse key).

## Checked (without the real files)
- Apocalypse USA, SMT IV USA 1.0 and JPN outputs (code.ips, hw ips, exheaders, 3gx) are
  byte-identical to before; ports for apoc/eu/jp/kr/tw/us11 regenerate unchanged.
- Synthetic version (Apocalypse USA with 0x100 bytes inserted at 0x1A0000): all 127 addresses
  mapped to the expected place, build, plugin, verify_region and package ran clean.
- Not checked: the real EUR/JPN binaries (encrypted). JPN FINAL may differ more than EUR
  (localisation code, text engine): expect manual entries there. All untested in play.

## FINAL JPN 1.1 imported and built (2026-11-08)
- The owner decrypted the 1.1 update CIA (0004000E00166B00, CTR-U-BG4J) with GodMode9; import_game.py put
  code.bin + exheader.bin in JPN_v11/. port.py apoc_jp11: 133 native / 131 ported addresses, 0 unmapped, the same
  vote splits as 1.0 (majority taken). Relative to 1.0, .data starts 0x4000 later and .bss ends 0x1000 earlier;
  every mapped address follows one of those two shifts (0x619988 took FadeRecs' -0x1000 shift, consistent).
- b15 builds like 1.0 (core in its own page 0x64B000, same sizes), verify_region OK, packaged as
  releases/SMT4A_60fps_b15_JPN_FINAL_1.1.zip. Untested in play.
