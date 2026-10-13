import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
import re, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from port import map_text, map_data, wa, wn, B
MP = os.path.join(ROOT, 'v11/addrmap.json')
cur = {int(k, 16): int(v, 16) for k, v in json.load(open(MP)).items()}
SKIP = {0x100000, 0x3FE624, 0x3FE628, 0x3FE640, 0x3FF000, 0x42AB78, 0x42AB80, 0x42B000, 0x4EFE34, 0x4EFE38,
        0x4EFF00, 0x4EFFF4, 0x454F46}
FILES = ['patch/phase3b.S', 'patch/interp.h', 'patch/interp_arm.c', 'patch/interp_thumb.c', 'patch/interp_ext.c',
         'src/cam_stick.c', 'tools/build_release.py', 'tools/build_phase1.py']
consts = set()
for f in FILES:
    for m in re.finditer(r'0x([0-9A-Fa-f]{6,8})', open(os.path.join(ROOT, f)).read()):
        v = int(m.group(1), 16)
        if 0x100000 <= v < 0x4F0000 and v not in SKIP and v not in cur: consts.add(v)
new, bad = {}, []
for v in sorted(consts):
    if v < 0x3FF000:
        r = map_text(v)
        if len(r) != 1: r2 = map_text(v, 24, 40, 0x8000); r = r2 if len(r2) == 1 else r
        if len(r) == 1: new[v] = r[0]
        else: bad.append((hex(v), [hex(x) for x in r][:5]))
    else:
        votes = map_data(v)
        if not votes:
            if v >= 0x42B000: new[v] = v; print('data (no refs, assumed same)', hex(v))
            else: bad.append((hex(v), 'rodata no refs'))
        else:
            best = max(votes, key=votes.get)
            print('votes', hex(v), {hex(k): c for k, c in votes.items()})
            new[v] = best
for k, v in sorted(new.items()): print(f'{k:#08x} -> {v:#08x}  ({v - k:+#x})')
print('BAD', bad)
if '--write' in sys.argv:
    cur.update(new)
    json.dump({hex(k): hex(v) for k, v in sorted(cur.items())}, open(MP, 'w'), indent=1)
    print('written', len(cur))
