"""Supported game versions.  The patch sources use USA 1.0 addresses; every other
version gets a port map (ports/<key>.json, made by tools/port.py) plus a layout
derived from its exheader.

    key   name      title id          files
    us10  USA 1.0   00040000000E5C00  game/v10
    us11  USA 1.1   00040000000E5C00  game/v11        (update 1.2.0)
    eu    EUR       0004000000141C00  Other Regions Builds/SMTIV-E-ver1_extracted
    jp    JPN       0004000000088A00  Other Regions Builds/SMTIV-J-ver1_extracted
    kr    KOR       00040000000F6100  Other Regions Builds/SMTIV-K-ver1_extracted
    tw    TWN       00040000000D6D00  Other Regions Builds/SMTIV-T-ver1_extracted
    apoc  SMT IV: Apocalypse USA  000400000019A200  Shin Megami Tensei IV - Apocalypse
          (a different game on the same engine: functions moved by up to megabytes, so
          tools/port.py searches the whole file for it: GLOBAL below)
    apoc_eu    SMT IV: Apocalypse EUR  00040000001A6B00  .../EUR
    apoc_jp    SMT IV: FINAL JPN 1.0   0004000000166B00  .../JPN
    apoc_jp11  SMT IV: FINAL JPN 1.1   0004000000166B00  .../JPN_v11   (update 0004000E00166B00)
          (same game as apoc: base='apoc'.  tools/port.py maps Apocalypse USA addresses to them
          (ports/<key>_native.json) and chains ports/apoc.json through that map, so they share
          every apoc fix; the sources see REGION_APOC as well as REGION_<KEY>)
"""
import os, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_REG = 'Other Regions Builds'
_APOC = 'Shin Megami Tensei IV - Apocalypse'
REGIONS = {
    'us10': dict(name='USA 1.0', dir='game/v10', code='decompressed_code.bin'),
    'us11': dict(name='USA 1.1', dir='game/v11', code='decompressed_code.bin'),
    'eu':   dict(name='EUR',     dir=f'{_REG}/SMTIV-E-ver1_extracted', code='code.bin'),
    'jp':   dict(name='JPN',     dir=f'{_REG}/SMTIV-J-ver1_extracted', code='code.bin'),
    'kr':   dict(name='KOR',     dir=f'{_REG}/SMTIV-K-ver1_extracted', code='code.bin'),
    'tw':   dict(name='TWN',     dir=f'{_REG}/SMTIV-T-ver1_extracted', code='code.bin'),
    'apoc': dict(name='SMT IV: Apocalypse USA', dir=_APOC, code='code.bin', glob=True,
                 # Apocalypse-only code, used as is (not ported): h3d model system ("h3dModelCalcProc" 0x145F94)
                 # Apocalypse addresses written as plain constants inside REGION_APOC blocks
                 native_consts=[0x57F574,                     # InfoWin work pointer (WDEF)
                                0x5F15FC],                    # field lock flags (MIF3DL check)
                 native_syms={'H3dModelUpdate': 0x156994,      # per-model update (model r0, r1, anim step s0)
                              'FadeRecs': 0x6357D8,            # ScreenFadeUpdate's two fade records
                              'ViewSetXY': 0x4935B0, 'ViewSetXY_cont': 0x4935B4,      # UI view position setter
                              'AnimCamEval': 0x3F9C6C, 'AnimCamEval_cont': 0x3F9C70,   # .bch camera evaluate + step
                              'BtlCamPtr': 0x571508},                                  # battle camera work (camera log)
                 native_sites=[dict(addr=0x146020, kind='bl', to='hk_h3d_model', expect=0xEB00425B),
                               dict(addr=0x2DD538, kind='bl', to='hk_relic_begin', expect=0xE5946078),
                               dict(addr=0x2DD564, kind='bl', to='hk_relic_fade', expect=0xEEB81AC1),
                               dict(addr=0x2DD5A0, kind='bl', to='hk_relic_fade', expect=0xEEB81AC1),
                               dict(addr=0x2DD9D8, kind='bl', to='hk_relic_rise', expect=0xEEB80AC0),
                               dict(addr=0x2DDB5C, kind='bl', to='hk_relic_rise', expect=0xEEB80AC0)],
                 # cell smoothing owners (CI_OWN) the window search dropped, matched by task name and code
                 native_own=[(0x344CD0, 0x3450D4), (0x18152C, 0x1817B8),   # compendium, analyze
                             (0x493344, 0x49368C),                         # UI view draw helpers
                             (0x20BD54, 0x20CAEC),                         # ButtonHelp DDRAW
                             (0x2AA56C, 0x2AB7C4),                         # TMD DDRAW (talk / choice panel)
                             (0x22B2D4, 0x22E774),                         # Status DrawUp
                             (0x329A90, 0x329D64), (0x355428, 0x356308),   # SEARCH2D
                             (0x21775C, 0x218640), (0x288538, 0x288B20),   # Camp DDRAW 0x227418 helpers
                             (0x227294, 0x227418)]),                       # Camp DBaseDraw 0x25EC04 helper
    # Apocalypse in other regions: decompressed .code as code.bin + exheader.bin (docs/notes/APOC_versions.md)
    # core_page: text cave too small for the core (EUR 8 bytes, JPN 1208): the core gets its own page
    # after ext6 (tools/build.py CORE_PAGE); on hardware the plugin writes it, so code_hw.ips needs it
    'apoc_eu':   dict(name='SMT IV: Apocalypse EUR', dir=f'{_APOC}/EUR', code='code.bin', base='apoc', core_page=True),
    'apoc_jp':   dict(name='SMT IV: FINAL JPN 1.0', dir=f'{_APOC}/JPN', code='code.bin', base='apoc', core_page=True),
    'apoc_jp11': dict(name='SMT IV: FINAL JPN 1.1', dir=f'{_APOC}/JPN_v11', code='code.bin', base='apoc', core_page=True),
}
PAGE = 0x1000
up = lambda a, n=PAGE: (a + n - 1) // n * n

def available(key):
    """the version's game files are present (they are private and may be missing)"""
    r = REGIONS[key]
    return all(os.path.exists(f"{ROOT}/{r['dir']}/{f}") for f in (r['code'], 'exheader.bin'))

def family(key):
    """the game a version belongs to: 'apoc' for every Apocalypse version, else the key itself"""
    return REGIONS[key].get('base', key)

def native_map(key):
    """Apocalypse USA address -> address in version key (ports/<key>_native.json), {} before port.py ran"""
    import json
    p = f'{ROOT}/ports/{key}_native.json'
    return {int(k, 16): int(v, 16) for k, v in json.load(open(p)).items()} if os.path.exists(p) else {}

def region(key):
    r = dict(REGIONS[key], key=key)
    r['family'] = family(key)
    if r['family'] != key:                       # inherit the base version's own addresses, ported
        b = REGIONS[r['family']]
        nm = native_map(key)
        r['native_ported'] = bool(nm)
        m = lambda a: nm.get(a, a)
        for k in ('glob',): r.pop(k, None)
        r['native_consts'] = list(b.get('native_consts', []))
        r['native_syms'] = {k: m(v) for k, v in b.get('native_syms', {}).items()}
        r['native_sites'] = [dict(s, addr=m(s['addr'])) for s in b.get('native_sites', [])]
        r['native_own'] = [(m(lo), m(hi)) for lo, hi in b.get('native_own', [])
                           if nm.get(lo, 0) and nm.get(hi, 0) or not nm]
        r['native_xlat'] = nm
    if not available(key):
        raise SystemExit(f"{r['name']} ({key}): game files missing in {r['dir']}/ ({r['code']} + exheader.bin)")
    r['code'] = f"{ROOT}/{r['dir']}/{r['code']}"
    r['exh'] = f"{ROOT}/{r['dir']}/exheader.bin"
    e = open(r['exh'], 'rb').read()
    seg = lambda o: struct.unpack_from('<III', e, o)          # address, pages, size
    t, ro, d = seg(0x10), seg(0x20), seg(0x30)
    r['bss'] = struct.unpack_from('<I', e, 0x3C)[0]
    r['tid'] = '%016X' % struct.unpack_from('<Q', e, 0x200)[0]
    r['text_end'] = t[0] + t[2]
    r['ro_start'], r['ro_end'] = ro[0], ro[0] + ro[2]
    r['data_start'] = d[0]
    r['bss_end'] = d[0] + d[2] + r['bss']
    # patch layout (USA 1.0 reference: cave 0x3FE628-0x3FF000, STATE 0x4EFE38, pages from 0x4F0000)
    r['cave'] = up(r['text_end'] + 4, 8)
    r['cave_end'] = up(r['text_end'])
    r['ext2'] = up(r['ro_end'], 0x80)
    r['ext2_end'] = up(r['ro_end'])
    r['state'] = up(r['bss_end'] + 4, 8)
    r['state_delta'] = r['state'] - 0x4EFE38                 # patch STATE block
    r['page_delta'] = up(r['bss_end']) - 0x4F0000            # patch pages 0x4F0000-0x4F5000
    r['suffix'] = '' if key == 'us10' else '_' + key
    return r
