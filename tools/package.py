"""Package a built release for every version.
    python tools/package.py BUILD [--release X.Y] [region ...]
        (expects out/phaseBUILD[_<region>]_* from build_release.py; --release names the
        public release: zips SMT4_60fps_vX.Y_<VERSION>.zip)
Writes releases/SMT4_60fps_<BUILD>_<VERSION>.zip, one per version, each with
Azahar_full, Real_3DS, Cheats and a README.  (build_release.py still makes
NAME_60fps_only.ips; it is no longer shipped.)"""
import os, sys, shutil, zipfile, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import region, REGIONS, ROOT

BUILD = sys.argv[1]
ARGS = sys.argv[2:]
REL = None
if '--release' in ARGS:
    i = ARGS.index('--release'); REL = ARGS[i + 1]; del ARGS[i:i + 2]
LABEL = {'us10': 'USA_v1.0', 'us11': 'USA_v1.1', 'eu': 'EUR', 'jp': 'JPN', 'kr': 'KOR', 'tw': 'TWN'}
CHEAT_TAG = {'us10': lambda n: 'USA' in n and '1.2.0' not in n, 'us11': lambda n: 'USA update 1.2.0' in n,
             'eu': lambda n: 'EUR' in n, 'jp': lambda n: n.endswith(' JP]'), 'kr': lambda n: n.endswith(' KR]'),
             'tw': lambda n: n.endswith(' TWN]')}
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
Game: Shin Megami Tensei IV, {name} (title {tid})
Game logic still runs at 30 Hz (original speed); the in-between frames are interpolated.

Pick ONE of the two patch folders:

1) Azahar_full  (recommended for Azahar: 60 fps + all smoothing fixes + right-stick camera)
   Copy the folder "{tid}" into:  Azahar/load/mods/
   Result:
     Azahar/load/mods/{tid}/exheader.bin
     Azahar/load/mods/{tid}/exefs/code.ips
   exheader.bin is REQUIRED. Without it the game will not start
   (log shows "Failed to load ROM (Error 12)"). Always replace both files together.

2) Real_3DS  (Luma3DS)
   Copy to SD:  /luma/titles/{tid}/code.ips
   Enable "Enable game patching" in the Luma config (hold SELECT at boot).
   Basic 60 fps (logic gate, camera/model/sprite interpolation) only. The other
   fixes need memory that a real 3DS does not allow code to run from, so they
   are emulator-only.

Optional cheats (Cheats folder): ghosting fix and bloom removal by the GBAtemp
thread "Shin Megami Tensei IV and IV:A Ghosting Fix":
  {thread}
  Azahar:    copy Cheats/Azahar/{tid}.txt into Azahar's user folder: Azahar/cheats/
             (Windows: %APPDATA%\\Azahar\\cheats), then tick the codes in Azahar's cheat
             window while the game is running
  Luma3DS:   copy Cheats/Luma3DS/cheats/{tid}.txt to SD:/cheats/
             (open the Rosalina menu with L+Down+Select > Cheats in game)
  "Ghosting Fix + Bloom Removal" holds only the bloom lines: enable it together with "Ghosting Fix".

Smoothed at 60 fps: 3D characters and camera, menus and slides, maps, battle
effects, area select / event backdrop pans, event portraits (slides, fades,
zooms, Burroughs), compendium / status / talk panel / battle HP bars, right-stick camera.

Known remaining: pre-rendered video cutscenes stay at their encoded frame rate;
outside battle, flame / glow / scrolling-texture effects stay on the game's
original 30 Hz frames (keeps fire billboards correct); bottom-screen map markers
move in whole pixels; a one-frame camera jump at the start of the terminal
teleport scene. Save states made with an older build keep running the older patch.
{extra}"""
EXTRA = {
    'jp': "\nJPN/TWN note: the talk / choice panel uses the generic 2D smoothing (its code differs from the\nUSA version, so the dedicated smoothing for it is not applied).\n",
    'tw': "\nJPN/TWN note: the talk / choice panel uses the generic 2D smoothing (its code differs from the\nUSA version, so the dedicated smoothing for it is not applied).\n",
    'us11': "\nThis folder is for the 1.1 update (installed update data, internal version 1.2.0).\nUse the USA_v1.0 package if the update is not installed.\n",
}

def package(key):
    R = region(key)
    tid, sfx = R['tid'], R['suffix']
    out = lambda kind: f'{ROOT}/out/phase{BUILD}{sfx}_{kind}'
    for k in ('code.ips', 'exheader.bin', 'code_hw.ips'):
        if not os.path.exists(out(k)): raise SystemExit(f'missing {out(k)}: run SMT4_REGION={key} python tools/build_release.py phase{BUILD}')
    name = f'SMT4_60fps_v{REL}_{LABEL[key]}' if REL else f'SMT4_60fps_{BUILD}_{LABEL[key]}'
    stage = f'{ROOT}/build/pkg/{name}'
    shutil.rmtree(stage, ignore_errors=True)
    files = {f'Azahar_full/{tid}/exefs/code.ips': out('code.ips'), f'Azahar_full/{tid}/exheader.bin': out('exheader.bin'),
             f'Real_3DS/{tid}/code.ips': out('code_hw.ips')}
    for dst, src in files.items():
        os.makedirs(os.path.dirname(f'{stage}/{dst}'), exist_ok=True); shutil.copy(src, f'{stage}/{dst}')
    blocks = [b for b in cheat_blocks() if CHEAT_TAG[key](b[0])]
    assert blocks, f'no cheats for {key}'
    text = '\n\n'.join('\n'.join(b) for b in blocks) + '\n'
    for dst in (f'Cheats/Azahar/{tid}.txt', f'Cheats/Luma3DS/cheats/{tid}.txt'):
        os.makedirs(os.path.dirname(f'{stage}/{dst}'), exist_ok=True)
        open(f'{stage}/{dst}', 'w', encoding='utf-8', newline='\n').write(text)
    open(f'{stage}/README.txt', 'w', encoding='utf-8', newline='\r\n').write(
        README.format(build=f'release {REL} (build {BUILD})' if REL else f'build {BUILD}', name=R['name'], tid=tid, thread=THREAD, extra=EXTRA.get(key, '')))
    zp = f'{ROOT}/releases/{name}.zip'
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as z:
        for r, _, fs in os.walk(stage):
            for f in fs: z.write(os.path.join(r, f), os.path.relpath(os.path.join(r, f), stage))
    print(f'{name}.zip: {len(blocks)} cheats ({", ".join(b[0] for b in blocks)})')

for key in (ARGS or REGIONS):
    package(key)
