"""Package a built release for every version.
    python tools/package.py BUILD [--release X.Y] [region ...]
        (expects out/BUILD[_<region>]_* from build_release.py, BUILD with or without a
        'phase' prefix: 12, phase12, b13; --release names the public release: zips
        <GAME>_60fps_vX.Y_<VERSION>.zip)
        and out/BUILD[_<region>]_plugin.3gx / _exheader_hw.bin from build_plugin.py)
Writes releases/<GAME>_60fps_<BUILD>_<VERSION>.zip (GAME: SMT4 or SMT4A for Apocalypse/FINAL;
BUILD without 'phase'), one per version, each with
Azahar_full, Actual_3DS (SD card layout: luma/titles + luma/plugins), Cheats and a
README.  (build_release.py still makes NAME_60fps_only.ips; it is no longer shipped.)"""
import os, sys, shutil, zipfile, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import region, REGIONS, ROOT, available, family

BUILD = sys.argv[1]
ARGS = sys.argv[2:]
REL = None
if '--release' in ARGS:
    i = ARGS.index('--release'); REL = ARGS[i + 1]; del ARGS[i:i + 2]
BUILD = BUILD[5:] if BUILD.startswith('phase') else BUILD
LABEL = {'us10': 'USA_1.0', 'us11': 'USA_1.1', 'eu': 'EUR', 'jp': 'JPN', 'kr': 'KOR', 'tw': 'TWN',
         'apoc': 'USA', 'apoc_eu': 'EUR', 'apoc_jp': 'JPN_FINAL_1.0', 'apoc_jp11': 'JPN_FINAL_1.1'}
GAME = {'apoc': 'SMT4A'}                             # zip prefix per family(); SMT IV: SMT4
_IVA = lambda n: 'IV:A' in n or 'FINAL' in n                   # Apocalypse (JPN title: FINAL) blocks
_IV = lambda f: lambda n: not _IVA(n) and f(n)
CHEAT_TAG = {'us10': _IV(lambda n: 'USA' in n and '1.2.0' not in n), 'us11': _IV(lambda n: 'USA update 1.2.0' in n),
             'eu': _IV(lambda n: 'EUR' in n), 'jp': _IV(lambda n: n.endswith(' JP]')), 'kr': _IV(lambda n: n.endswith(' KR]')),
             'tw': _IV(lambda n: n.endswith(' TWN]')),
             'apoc': lambda n: _IVA(n) and 'USA' in n, 'apoc_eu': lambda n: _IVA(n) and 'EUR' in n,
             'apoc_jp': lambda n: 'FINAL' in n and (n.endswith(' JP]') or 'base' in n),
             'apoc_jp11': lambda n: 'FINAL' in n and 'v1.1.0' in n}
CHEATS = f'{ROOT}/Other Regions Builds/CHEATS.txt'
THREAD = 'https://gbatemp.net/threads/shin-megami-tensei-iv-and-iv-a-ghosting-fix.684175/'

def cheat_blocks():
    blocks, cur = [], None
    for line in open(CHEATS, encoding='utf-8').read().splitlines():
        line = line.strip()
        if line.startswith('['): cur = [line]; blocks.append(cur)
        elif line and cur is not None: cur.append(line)
    return blocks

README = """SMT IV 60 FPS patch - {build} - {name}
Game: {game} (title {tid})
Game logic still runs at 30 Hz (original speed); the in-between frames are interpolated.

Pick ONE of the two patch folders:

1) Azahar_full  (recommended for Azahar: 60 fps + all smoothing fixes + right-stick camera)
   Copy the folder "{tid}" into:  Azahar/load/mods/
   Result:
     Azahar/load/mods/{tid}/exheader.bin
     Azahar/load/mods/{tid}/exefs/code.ips
   exheader.bin is REQUIRED. Without it the game will not start
   (log shows "Failed to load ROM (Error 12)"). Always replace both files together.

2) Actual_3DS  (a real 3DS with Luma3DS 13.0 or newer; New 3DS recommended)
   Copy the "luma" folder inside Actual_3DS to the root of the SD card (merge folders).
   Result:
     SD:/luma/titles/{tid}/code.ips
     SD:/luma/titles/{tid}/exheader.bin
     SD:/luma/plugins/{tid}/smt4_60fps.3gx
   Keep only ONE .3gx file in that plugins folder.
   Setup, once:
     - Luma config (hold SELECT while powering on): turn on "Enable game patching".
     - Rosalina menu (L + Down + Select on the HOME menu): set "Plugin Loader" to Enabled.
     - New 3DS: Rosalina > "New 3DS settings": 804 MHz clock and L2 cache on.
   Start the game from the HOME menu: the top screen flashes blue briefly when the
   plugin loads. Same features as Azahar, plus a frame-drop fix: when a busy scene
   cannot hold 60 fps the game drops to 30 fps at full speed instead of slowing down.
   Performance: the 3DS cannot hold 60 fps everywhere. Turning the 3D slider off and
   using the cheats below (they remove full-screen effects) can help.
   To uninstall: delete the three files.

Optional cheats (Cheats folder): ghosting fix and bloom removal by the GBAtemp
thread "Shin Megami Tensei IV and IV:A Ghosting Fix":
  {thread}
  Azahar:    copy Cheats/Azahar/{tid}.txt into Azahar's user folder: Azahar/cheats/
             (Windows: %APPDATA%\\Azahar\\cheats), then tick the codes in Azahar's cheat
             window while the game is running
  Luma3DS:   copy Cheats/Luma3DS/cheats/{tid}.txt to SD:/cheats/
             (open the Rosalina menu with L+Down+Select > Cheats in game)
  "Ghosting Fix + Bloom Removal" holds only the bloom lines: enable it together with the ghosting fix code.
  On a real 3DS these cheats also lighten the GPU load, which helps keep 60 fps.

Smoothed at 60 fps: 3D characters and camera, menus and slides, maps, battle
effects, area select / event backdrop pans, event portraits (slides, fades,
zooms, Burroughs), compendium / status / talk panel / battle HP bars, right-stick camera.
Camera cuts in 3D cutscenes are clean (no in-between frame).

Known remaining: pre-rendered video cutscenes stay at their encoded frame rate;
outside battle, flame / glow / scrolling-texture effects stay on the game's
original 30 Hz frames (keeps fire billboards correct); bottom-screen map markers
move in whole pixels. Save states made with an older build keep running the older patch.

The whole game has not been play-tested with the patch. If you run into a problem,
please report it on the GitHub page.
{extra}"""
EXTRA = {
    'jp': "\nJPN/TWN note: the talk / choice panel uses the generic 2D smoothing (its code differs from the\nUSA version, so the dedicated smoothing for it is not applied).\n",
    'tw': "\nJPN/TWN note: the talk / choice panel uses the generic 2D smoothing (its code differs from the\nUSA version, so the dedicated smoothing for it is not applied).\n",
    'apoc_jp11': "\nThis folder is for the 1.1 update (installed update data).\nUse the JPN_FINAL_1.0 package if the update is not installed.\n",
    'us11': "\nThis folder is for the 1.1 update (installed update data, internal version 1.2.0).\nUse the USA_1.0 package if the update is not installed.\n",
}

CORE_PAGE_NOTE = ("\nActual_3DS: in this version the 60 fps code does not fit inside the game's code, so the\n"
                  "plugin installs it. Always install all three files: code.ips without the .3gx plugin\n"
                  "(or with the Plugin Loader off) crashes the game at boot.\n")

def package(key):
    R = region(key)
    tid, sfx = R['tid'], R['suffix']
    pre = next((p for p in (f'phase{BUILD}', BUILD) if os.path.exists(f'{ROOT}/out/{p}{sfx}_code.ips')), f'phase{BUILD}')
    out = lambda kind: f'{ROOT}/out/{pre}{sfx}_{kind}'
    for k in ('code.ips', 'exheader.bin', 'code_hw.ips'):
        if not os.path.exists(out(k)): raise SystemExit(f'missing {out(k)}: run SMT4_REGION={key} python tools/build_release.py {pre}')
    for k in ('plugin.3gx', 'exheader_hw.bin'):
        if not os.path.exists(out(k)): raise SystemExit(f'missing {out(k)}: run SMT4_REGION={key} python tools/build_plugin.py {pre}')
    name = f"{GAME.get(family(key), 'SMT4')}_60fps_{'v' + REL if REL else BUILD}_{LABEL[key]}"
    stage = f'{ROOT}/build/pkg/{name}'
    shutil.rmtree(stage, ignore_errors=True)
    files = {f'Azahar_full/{tid}/exefs/code.ips': out('code.ips'), f'Azahar_full/{tid}/exheader.bin': out('exheader.bin'),
             f'Actual_3DS/luma/titles/{tid}/code.ips': out('code_hw.ips'),
             f'Actual_3DS/luma/titles/{tid}/exheader.bin': out('exheader_hw.bin'),
             f'Actual_3DS/luma/plugins/{tid}/smt4_60fps.3gx': out('plugin.3gx')}
    for dst, src in files.items():
        os.makedirs(os.path.dirname(f'{stage}/{dst}'), exist_ok=True); shutil.copy(src, f'{stage}/{dst}')
    blocks = [b for b in cheat_blocks() if CHEAT_TAG[key](b[0])]
    assert len(blocks) == 2, f'expected ghosting + bloom cheats for {key}, got {[b[0] for b in blocks]}'
    text = '\n\n'.join('\n'.join(b) for b in blocks) + '\n'
    for dst in (f'Cheats/Azahar/{tid}.txt', f'Cheats/Luma3DS/cheats/{tid}.txt'):
        os.makedirs(os.path.dirname(f'{stage}/{dst}'), exist_ok=True)
        open(f'{stage}/{dst}', 'w', encoding='utf-8', newline='\n').write(text)
    game = R['name'] if family(key) == 'apoc' else f"Shin Megami Tensei IV, {R['name']}"
    readme = README.format(build=f'release {REL} (build {BUILD})' if REL else f'build {BUILD}', name=R['name'], game=game,
                           tid=tid, thread=THREAD, extra=EXTRA.get(key, '') + (CORE_PAGE_NOTE if R.get('core_page') else ''))
    open(f'{stage}/README.txt', 'w', encoding='utf-8', newline='\r\n').write(readme)
    zp = f'{ROOT}/releases/{name}.zip'
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as z:
        for r, _, fs in os.walk(stage):
            for f in fs: z.write(os.path.join(r, f), os.path.relpath(os.path.join(r, f), stage))
    print(f'{name}.zip: {len(blocks)} cheats ({", ".join(b[0] for b in blocks)})')

for key in (ARGS or [k for k in REGIONS if available(k)]):
    package(key)
