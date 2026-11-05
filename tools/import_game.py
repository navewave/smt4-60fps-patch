"""Put a game version's files where tools/regions.py expects them (decompressed code.bin + exheader.bin).
    python tools/import_game.py <key> <decrypted .cia / .cxi / .3ds>
    python tools/import_game.py <key> --code <.code or code.bin> --exheader <exheader.bin>
The image must be decrypted (NCCH "NoCrypto" flag; CIA content not title-key encrypted):
encrypted eShop/No-Intro CIAs are refused.  A compressed .code is decompressed (tools/blz.py).
Existing files are kept unless --force is given.  See docs/notes/APOC_versions.md."""
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import REGIONS, ROOT
from blz import decompress

MU = 0x200                                            # NCCH media unit
al = lambda x, n=64: (x + n - 1) // n * n

def cia_ncch(f):
    """offset of content 0 (the game NCCH) in a CIA"""
    hs, _, _, cs, ts, tmd = struct.unpack('<IHHIII', f(0, 0x14))
    off_tmd = al(hs) + al(cs) + al(ts)
    sig = struct.unpack('>I', f(off_tmd, 4))[0]
    th = off_tmd + {0x10003: 0x240, 0x10004: 0x140, 0x10005: 0x80}[sig]
    ctype = struct.unpack('>H', f(th + 0xC4 + 64 * 36 + 6, 2))[0]
    if ctype & 1: raise SystemExit('CIA content is title-key encrypted: decrypt it first (docs/notes/APOC_versions.md)')
    return al(off_tmd + tmd)

def from_image(path):
    fh = open(path, 'rb')
    def f(o, n): fh.seek(o); return fh.read(n)
    base = 0
    if f(0x100, 4) == b'NCSD': base = struct.unpack('<I', f(0x120, 4))[0] * MU          # .3ds: partition 0
    elif f(0x100, 4) != b'NCCH': base = cia_ncch(f)                                    # .cia
    h = f(base, 0x200)
    if h[0x100:0x104] != b'NCCH': raise SystemExit(f'{path}: no NCCH at {base:#x} (encrypted or not a game image)')
    if not h[0x18F] & 4: raise SystemExit(f'{path}: NCCH is encrypted (NoCrypto flag not set): decrypt it first')
    exh = f(base + 0x200, 0x800)
    exefs = base + struct.unpack('<I', h[0x1A0:0x1A4])[0] * MU
    for i in range(10):
        name, off, size = struct.unpack('<8sII', f(exefs + i * 16, 16))
        if name.rstrip(b'\0') == b'.code': return f(exefs + 0x200 + off, size), exh
    raise SystemExit(f'{path}: no .code in the ExeFS')

def main(a):
    force = '--force' in a; a = [x for x in a if x != '--force']
    if not a or a[0] not in REGIONS: raise SystemExit(__doc__ + '\nkeys: ' + ', '.join(REGIONS))
    key = a[0]
    if '--code' in a:
        code = open(a[a.index('--code') + 1], 'rb').read(); exh = open(a[a.index('--exheader') + 1], 'rb').read()[:0x800]
    else:
        code, exh = from_image(a[1])
    if len(exh) != 0x800: raise SystemExit(f'exheader is {len(exh):#x} bytes, expected 0x800')
    tsize = struct.unpack_from('<I', exh, 0x18)[0]
    if exh[0xD] & 1 or len(code) < tsize:            # compressed .code (SCI flag, or shorter than its text)
        code = decompress(code)
    if code[:4] == bytes(4) or len(code) < tsize: raise SystemExit('code does not look decompressed/decrypted')
    d = f"{ROOT}/{REGIONS[key]['dir']}"
    out_code = f"{d}/{REGIONS[key]['code']}"
    if not force and (os.path.exists(out_code) or os.path.exists(f'{d}/exheader.bin')):
        raise SystemExit(f'{d}: files exist (use --force to replace)')
    os.makedirs(d, exist_ok=True)
    open(out_code, 'wb').write(code); open(f'{d}/exheader.bin', 'wb').write(exh)
    tid = struct.unpack_from('<Q', exh, 0x200)[0]
    print(f"{REGIONS[key]['name']} ({key}): title {tid:016X}, code {len(code):#x} bytes, text {tsize:#x} -> {d}/")

if __name__ == '__main__':
    main(sys.argv[1:])
