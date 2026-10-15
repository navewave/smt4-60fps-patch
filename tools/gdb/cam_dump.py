"""Read the camera log from a running log build (SMT4_CFLAGS=-DCAMLOG, see cam_log in patch/interp_ext.c).
    python tools/gdb/cam_dump.py [--all] [--csv FILE] [--reset]
One row per logic tick, oldest first:
  dpos / dtgt = how far camera position / target moved this tick (vs. S->camPrev, what the core blends from)
  ppos / ptgt = the same for the tick before (a pan continues it, a cut does not)
  core  = what the core's cam_swap does: B = blends (all |d| <= 150 per axis), cut = no blend, new = other camera
  fov   = field of view; F = fov_swap blended it this tick
Without --all only ticks where the camera moved, and their neighbours, are printed.
--reset clears the log (a new one is allocated on the next logic tick)."""
import os, struct, subprocess, sys, tempfile, math
GDB = r"C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin/arm-none-eabi-gdb.exe"
CL_PTR, CL_N, REC = 0x4F3FCC, 512, 72
SIZE = 16 + CL_N * REC

def gdb(cmds):
    tmp = tempfile.mkdtemp().replace(os.sep, '/')
    pre = ['set pagination off', 'set confirm off', 'set architecture armv6k', 'target remote localhost:24689']
    open(f'{tmp}/c.gdb', 'w').write('\n'.join(pre + [c.replace('$TMP', tmp) for c in cmds] + ['detach']) + '\n')
    r = subprocess.run([GDB, '-q', '-nx', '-batch', '-x', f'{tmp}/c.gdb'], capture_output=True, text=True, timeout=120)
    return tmp, r.stdout + r.stderr

if '--anims' in sys.argv:          # anims stepped at half frames (an_log in patch/interp_ext.c)
    tmp, out = gdb(['dump binary memory $TMP/p.bin 0x4F3FD0 0x4F3FD4'])
    ap = struct.unpack('<I', open(f'{tmp}/p.bin', 'rb').read())[0]
    if ap < 0x100000: sys.exit(f'no anim table ({ap:#x})')
    if '--reset' in sys.argv:
        gdb(['set {int}0x4F3FD0 = 0']); sys.exit('anim table cleared')
    tmp, out = gdb([f'dump binary memory $TMP/a.bin 0x{ap:X} 0x{ap + 16 + 32 * 28:X}'])
    a = open(f'{tmp}/a.bin', 'rb').read()
    print(f'{"vtable":>8} {"count":>8} {"last tick":>9}  group / target')
    for i in range(32):
        vt, cnt, tk = struct.unpack_from('<III', a, 16 + i * 28)
        if not cnt: break
        s1, s2 = (a[16 + i * 28 + 12 + k * 8:16 + i * 28 + 20 + k * 8].split(b'\0')[0].decode('latin1') for k in (0, 1))
        print(f'{vt:08x} {cnt:8d} {tk:9d}  {s1!r} / {s2!r}')
    sys.exit()
tmp, out = gdb([f'dump binary memory $TMP/p.bin 0x{CL_PTR:X} 0x{CL_PTR + 4:X}'])
try: hp = struct.unpack('<I', open(f'{tmp}/p.bin', 'rb').read())[0]
except OSError: sys.exit('cannot read memory: is the GDB stub on and the game running?\n' + out[-300:])
if hp < 0x100000: sys.exit(f'no camera log (pointer {hp:#x}): log build not running, or no logic tick yet')
if '--reset' in sys.argv:
    gdb([f'set {{int}}0x{CL_PTR:X} = 0'])
    sys.exit('log cleared')
tmp, out = gdb([f'dump binary memory $TMP/t.bin 0x{hp:X} 0x{hp + SIZE:X}'])
t = open(f'{tmp}/t.bin', 'rb').read()
magic, n = struct.unpack_from('<II', t, 0)
if magic != 0x31474C43: sys.exit(f'bad magic {magic:#x}')
recs = []
for i in range(max(0, n - CL_N), n):
    v = struct.unpack_from('<5I13f', t, 16 + (i % CL_N) * REC)
    recs.append(dict(tick=v[0], ctr=v[1], cam=v[2], flags=v[3], scene=v[4], fov=v[5], real=v[6:12], prev=v[12:18]))

L = lambda a: math.sqrt(sum(x * x for x in a))
rows, prevd = [], None
for k, r in enumerate(recs):
    dp = [r['real'][i] - r['prev'][i] for i in range(3)]
    dt = [r['real'][i + 3] - r['prev'][i + 3] for i in range(3)]
    same = r['flags'] & 3 == 3
    big = any(abs(x) > 150 for x in dp + dt)
    moved = any(x != 0 for x in dp + dt)
    core = 'new' if not same else 'cut' if big else 'B' if moved else '-'
    cont = prevd is not None and k > 0 and recs[k - 1]['tick'] + 1 == r['tick'] and recs[k - 1]['cam'] == r['cam']
    pp, pt = (prevd if cont else ([0, 0, 0], [0, 0, 0]))
    rows.append((r, dp, dt, pp, pt, core, moved))
    prevd = (dp, dt)
show = set(range(len(rows))) if '--all' in sys.argv else \
    {j for i, x in enumerate(rows) if x[6] or x[5] in ('new', 'cut') for j in (i - 1, i, i + 1) if 0 <= j < len(rows)}
print(f'{len(recs)} ticks logged ({n} total)')
print(f'{"tick":>7} {"cam":>8} {"scene":>8} {"core":>4} {"dpos":>8} {"dtgt":>8} {"ppos":>8} {"ptgt":>8} {"cos":>6} {"fov":>7}  pos')
last = None
for i in sorted(show):
    r, dp, dt, pp, pt, core, moved = rows[i]
    if last is not None and i != last + 1: print('    ...')
    last = i
    d6, p6 = dp + dt, pp + pt
    cos = sum(a * b for a, b in zip(d6, p6)) / (L(d6) * L(p6)) if L(d6) and L(p6) else float('nan')
    f = 'F' if r['flags'] & 4 else ' '
    print(f'{r["tick"]:7d} {r["cam"]:08x} {r["scene"]:08x} {core:>4} {L(dp):8.2f} {L(dt):8.2f} {L(pp):8.2f} {L(pt):8.2f} '
          f'{cos:6.2f} {r["fov"]:6.2f}{f}  ' + ' '.join(f'{x:8.1f}' for x in r['real'][:3]))
if '--csv' in sys.argv:
    path = sys.argv[sys.argv.index('--csv') + 1]
    with open(path, 'w') as fp:
        fp.write('tick,counter,cam,flags,scene,fov,' + ','.join(f'real{i}' for i in range(6)) + ',' + ','.join(f'prev{i}' for i in range(6)) + '\n')
        for r in recs:
            fp.write(f'{r["tick"]},{r["ctr"]},{r["cam"]:#x},{r["flags"]},{r["scene"]:#x},{r["fov"]},' +
                     ','.join(str(x) for x in r['real']) + ',' + ','.join(str(x) for x in r['prev']) + '\n')
    print('csv:', path)
