"""Which build is running in Azahar?  Dumps the patch regions over the GDB stub and
compares them with every out/*_code.ips.   python tools/gdb/whichbuild.py"""
import glob, os, struct, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GDB = r"C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin/arm-none-eabi-gdb.exe"
REGIONS = [(0x3FE628, 0x9D8), (0x4F0000, 0xF00), (0x4F1580, 0x900), (0x4F2000, 0xE00), (0x4F3000, 0xC00)]

def ips(path):
    d = open(path, 'rb').read(); i = 5; recs = []
    while d[i:i + 3] != b'EOF':
        o = int.from_bytes(d[i:i + 3], 'big') + 0x100000; n = struct.unpack('>H', d[i + 3:i + 5])[0]
        recs.append((o, d[i + 5:i + 5 + n])); i += 5 + n
    return recs

tmp = tempfile.mkdtemp().replace(os.sep, '/')     # gdb command files want forward slashes
cmds = ['set pagination off', 'set confirm off', 'set architecture armv6k', 'target remote localhost:24689']
for k, (a, n) in enumerate(REGIONS):
    cmds.append(f'dump binary memory {tmp}/r{k}.bin 0x{a:X} 0x{a + n:X}')
cmds.append('detach')
open(f'{tmp}/c.gdb', 'w').write('\n'.join(cmds) + '\n')
r = subprocess.run([GDB, '-q', '-nx', '-batch', '-x', f'{tmp}/c.gdb'], capture_output=True, text=True, timeout=60)
mem = {}
for k, (a, n) in enumerate(REGIONS):
    p = f'{tmp}/r{k}.bin'
    if os.path.exists(p): mem[a] = open(p, 'rb').read()
if not mem: sys.exit('could not read memory (is the GDB stub on and the game running?)\n' + r.stdout[-400:] + r.stderr[-400:])
def read(addr, n):
    for a, b in mem.items():
        if a <= addr and addr + n <= a + len(b): return b[addr - a:addr + n - a]
best = []
for path in sorted(glob.glob(f'{ROOT}/out/*_code.ips')):
    tot = ok = 0
    for o, raw in ips(path):
        if len(raw) <= 4: continue
        m = read(o, len(raw))
        if m is None: continue
        tot += len(raw); ok += sum(x == y for x, y in zip(m, raw))
    if tot: best.append((ok / tot, os.path.basename(path)))
for score, name in sorted(best, reverse=True)[:5]: print(f'{score*100:6.2f}%  {name}')
