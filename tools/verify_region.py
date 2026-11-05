"""Static checks for a ported version: the instructions the extension expects at its
runtime hook points (ext_install / ci_install / hook_call) must be there, or the
feature silently stays off.   python tools/verify_region.py [region ...]"""
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault('SMT4_REGION', 'us10')
from regions import region, REGIONS, available
import importlib

WORDS = [  # (USA 1.0 address, expected word, what)
    (0x25C0F0, 0xE590002C, 'SetScale entry (scale smoothing)'),
    (0x2B1590, 0xEE305A01, 'quad function entry (generic 2D smoothing)'),
    (0x2BF460, 0xE92D4070, 'CellPut entry (cell smoothing)'),
    (0x2BF558, 0xE92D4FFF, 'cell draw B entry'),
    (0x2B06A0, 0xE92D40F0, 'cell draw C entry'),
    (0x2B0908, 0xE92D41F0, 'cell draw W entry'),
    (0x36880C, 0xED900A10, 'anim UpdateFrame entry'),
]
CALLS = [  # (USA 1.0 call site, USA 1.0 callee, what)
    (0x1ECE64, 0x2BF460, 'COMBG line draw call'),
    (0x1ECF54, 0x2BF460, 'COMBG dot draw call'),
    (0x1FC4AC, 0x39F238, 'nav map draw call'),
]
def check(key):
    os.environ['SMT4_REGION'] = key
    import build; importlib.reload(build)
    d = open(build.ORIG, 'rb').read()
    w = lambda a: struct.unpack_from('<I', d, a - 0x100000)[0]
    ok = True
    for a, exp, what in WORDS:
        n = build.P(a)
        if n == 0x100000: print(f'  {key}: {what}: absent (feature off)'); continue
        got = w(n)
        if got != exp: ok = False; print(f'  {key}: {what}: {a:#x}->{n:#x} has {got:08x}, expected {exp:08x}')
    for site, callee, what in CALLS:
        s, c = build.P(site), build.P(callee)
        if s == 0x100000: print(f'  {key}: {what}: absent (feature off)'); continue
        got = w(s)
        tgt = s + 8 + ((((got & 0xFFFFFF) ^ 0x800000) - 0x800000) << 2)
        if (got >> 24) != 0xEB or tgt != c: ok = False; print(f'  {key}: {what}: {s:#x} is {got:08x} (bl to {tgt:#x}), expected bl {c:#x}')
    print(f'{key}: {"OK" if ok else "PROBLEMS"}')
    return ok
if __name__ == '__main__':
    keys = sys.argv[1:] or [k for k in REGIONS if available(k)]
    sys.exit(0 if all([check(k) for k in keys]) else 1)
