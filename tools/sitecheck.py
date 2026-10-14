"""Check every hook site of a ported version: expected original instruction at the
mapped address, else list nearby addresses that hold it (for MANUAL in tools/port.py).
    python tools/sitecheck.py <region> [radius_words]"""
import os, re, sys, json, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import region, ROOT

key = sys.argv[1]; rad = int(sys.argv[2]) if len(sys.argv) > 2 else 96
R = region(key); U = region('us10')
d = open(R['code'], 'rb').read(); u = open(U['code'], 'rb').read()
w = lambda buf, a: struct.unpack_from('<I', buf, a - 0x100000)[0]
mp = {int(k, 16): int(v, 16) for k, v in json.load(open(f'{ROOT}/ports/{key}.json')).items()}
def same(e, g):
    if (e >> 25) & 7 == 5: return (e >> 24) == (g >> 24)
    return e == g
sites = []
for f in ('tools/build_phase1.py', 'tools/build_release.py'):
    for m in re.finditer(r"addr=0x([0-9A-Fa-f]+).*?expect=0x([0-9A-Fa-f]+)", open(f'{ROOT}/{f}').read()):
        sites.append((int(m.group(1), 16), int(m.group(2), 16)))
for m in re.finditer(r"\((0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+), '(cam_\w+)'\)", open(f'{ROOT}/tools/build_release.py').read()):
    sites.append((int(m.group(1), 16), int(m.group(2), 16)))
var = re.search(r"SITE_VARIANT = (\{.*\})", open(f'{ROOT}/tools/build_release.py').read())
VARIANT = eval(var.group(1)).get(key, {}) if var else {}
sites = [(a, VARIANT[a][1] if a in VARIANT else e) for a, e in sites]
bad = 0
for a, exp in sites:
    t = mp.get(a)
    if t is None: print(f'{a:#x}: not mapped'); bad += 1; continue
    if t == 0x100000: print(f'{a:#x}: absent'); continue
    got = w(d, t)
    if same(exp, got): continue
    bad += 1
    near = [t + 4 * k for k in range(-rad, rad + 1) if same(exp, w(d, t + 4 * k))]
    print(f'{a:#x} -> {t:#x}: has {got:08x}, expected {exp:08x}; nearby with it: {[hex(x) for x in near[:8]]}')
    print('   USA :', ' '.join(f'{w(u, a + 4 * k):08x}' for k in range(-4, 5)))
    for n in near[:3]:
        print(f'   {n:#x}:', ' '.join(f'{w(d, n + 4 * k):08x}' for k in range(-4, 5)))
print(f'{len(sites) - bad}/{len(sites)} sites OK')
