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
"""
import os, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_REG = 'Other Regions Builds'
REGIONS = {
    'us10': dict(name='USA 1.0', dir='game/v10', code='decompressed_code.bin'),
    'us11': dict(name='USA 1.1', dir='game/v11', code='decompressed_code.bin'),
    'eu':   dict(name='EUR',     dir=f'{_REG}/SMTIV-E-ver1_extracted', code='code.bin'),
    'jp':   dict(name='JPN',     dir=f'{_REG}/SMTIV-J-ver1_extracted', code='code.bin'),
    'kr':   dict(name='KOR',     dir=f'{_REG}/SMTIV-K-ver1_extracted', code='code.bin'),
    'tw':   dict(name='TWN',     dir=f'{_REG}/SMTIV-T-ver1_extracted', code='code.bin'),
    'apoc': dict(name='SMT IV: Apocalypse USA', dir='Shin Megami Tensei IV - Apocalypse', code='code.bin', glob=True),
}
PAGE = 0x1000
up = lambda a, n=PAGE: (a + n - 1) // n * n

def region(key):
    r = dict(REGIONS[key], key=key)
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
