"""Read the 30 Hz detector report from a running diag build (SMT4_CFLAGS=-DQDIAG).
    python tools/gdb/qd_dump.py [--reset]
Each row: a 2D draw site that changed only on logic frames for >= 3 ticks in a row.
  task  = game task that was running (name from re/dump/taskfuncs.txt)
  lr    = return address into the function that called the quad function (FUN_002B1590)
  what  = pos / size / colour (alpha, fades)
  runs  = how many separate 30 Hz runs were seen
--reset clears the report (to test one area at a time)."""
import os, struct, subprocess, sys, tempfile, bisect
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GDB = r"C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin/arm-none-eabi-gdb.exe"
QD_PTR, QD_N, QD_R = 0x4F3FC0, 512, 160
OFF_REP = 16 + 64 * 12 + QD_N * 44
SIZE = OFF_REP + QD_R * 16

def gdb(cmds):
    tmp = tempfile.mkdtemp().replace(os.sep, '/')
    pre = ['set pagination off', 'set confirm off', 'set architecture armv6k', 'target remote localhost:24689']
    open(f'{tmp}/c.gdb', 'w').write('\n'.join(pre + [c.replace('$TMP', tmp) for c in cmds] + ['detach']) + '\n')
    r = subprocess.run([GDB, '-q', '-nx', '-batch', '-x', f'{tmp}/c.gdb'], capture_output=True, text=True, timeout=120)
    return tmp, r.stdout + r.stderr

tmp, out = gdb([f'dump binary memory $TMP/p.bin 0x{QD_PTR:X} 0x{QD_PTR + 4:X}'])
try: hp = struct.unpack('<I', open(f'{tmp}/p.bin', 'rb').read())[0]
except OSError: sys.exit('cannot read memory: is the GDB stub on and the game running?\n' + out[-300:])
if hp < 0x100000: sys.exit(f'no detector table (pointer {hp:#x}): diag build not running, or nothing drawn yet')
if '--reset' in sys.argv:
    gdb([f'set {{int}}0x{QD_PTR:X} = 0'])
    sys.exit('report cleared (a new table is allocated on the next 2D draw)')
tmp, out = gdb([f'dump binary memory $TMP/t.bin 0x{hp:X} 0x{hp + SIZE:X}'])
t = open(f'{tmp}/t.bin', 'rb').read()
magic, hits, lost = struct.unpack_from('<III', t, 0)
if magic != 0x31304451: sys.exit(f'bad magic {magic:#x}')

names = {}
for l in open(f'{ROOT}/re/dump/taskfuncs.txt'):
    p = l.split(None, 2)
    if p[0] != '????????': names.setdefault(int(p[0], 16), set()).add(p[2].strip())
funcs = []
for l in open(f'{ROOT}/re/dump/decomp.c.funcs.txt'):
    p = l.split()
    if len(p) >= 4: funcs.append((int(p[0], 16), p[3]))
funcs.sort(); starts = [a for a, _ in funcs]
def func_of(a):
    i = bisect.bisect_right(starts, a) - 1
    return f'{funcs[i][1]}+{a - funcs[i][0]:#x}' if i >= 0 else '?'

rows = []
for i in range(QD_R):
    task, lr, tex, mask, n = struct.unpack_from('<IIIHH', t, OFF_REP + i * 16)
    if not n: break
    what = '/'.join(w for b, w in ((1, 'pos'), (2, 'size'), (4, 'colour')) if mask & b)
    rows.append((n, task, lr, what, tex))
print(f'reports: {len(rows)} sites, {hits} runs, {lost} lost (table full)')
print(f'{"runs":>5}  {"task":34} {"what":16} caller (lr)')
for n, task, lr, what, tex in sorted(rows, reverse=True):
    tn = '/'.join(sorted(names.get(task, {f'{task:08x}'})))[:34]
    print(f'{n:5d}  {tn:34} {what:16} {lr:08x} {func_of(lr)}')
