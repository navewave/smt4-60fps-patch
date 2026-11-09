"""Apocalypse: who moves the player's NW4C node?  (player stutters while the camera is smooth)
    python tools/gdb/apoc_player.py          (Azahar GDB stub on, in the field, keep walking)
1. reads the game camera's look-at point (the player) and dumps the heap twice, ~1 s apart
2. lists nodes whose translation (+0x58/+0x68/+0x78) is near the look-at point and moved
   between the dumps, and whether the patch's SetPos hooks recorded them (STATE rec[])
3. watches the best candidate's x and logs the writer pc/lr (watchpoint pc/lr are reliable)
Talks to the stub directly (no arm-none-eabi-gdb): a failed read is an error reply, not a
GDB batch abort (which would kill instead of detach).  Always detaches, so the game resumes.
Addresses are Apocalypse USA ones (STATE 0x64C120, GetGameCamera = *(*(0x59DBC0+4)+0x80))."""
import os, socket, struct, sys, time
import numpy as np
STATE, CAMBASE = 0x64C120, 0x59DBC0
BLK = 0x800
RANGES = [(0x568000, 0x652000), (0x08000000, 0x10000000), (0x14000000, 0x1C000000), (0x30000000, 0x38000000)]

class Stub:
    def __init__(self, port=24689):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=10); self.buf = b''
    def _recv(self, timeout=10):
        self.s.settimeout(timeout)
        while True:
            i = self.buf.find(b'$'); j = self.buf.find(b'#', i + 1) if i >= 0 else -1
            if i >= 0 and j >= 0 and len(self.buf) >= j + 3:
                pkt = self.buf[i + 1:j]; self.buf = self.buf[j + 3:]
                self.s.sendall(b'+'); return pkt.decode('latin1')
            self.buf += self.s.recv(65536)
    def cmd(self, data, timeout=10):
        d = data.encode('latin1')
        self.s.sendall(b'$' + d + b'#%02x' % (sum(d) & 0xFF))
        return self._recv(timeout)
    def read(self, a, n):
        r = self.cmd(f'm{a:x},{n:x}')
        return None if not r or r.startswith('E') else bytes.fromhex(r)
    def u32(self, a):
        b = self.read(a, 4); return struct.unpack('<I', b)[0] if b else 0
    def regs(self):
        g = bytes.fromhex(self.cmd('g'))
        return struct.unpack_from('<16I', g)
    def detach(self):
        try: self.cmd('D', timeout=3)
        except Exception: pass
        self.s.close()

def heap(st):
    """{base: bytes} of every readable 0x800 block in RANGES (1 MB steps over holes)"""
    out = {}
    for lo, hi in RANGES:
        a = lo
        while a < hi:
            b = st.read(a, BLK)
            if b is None or len(b) != BLK:
                a = (a | 0xFFFFF) + 1; continue
            out[a] = b; a += BLK
    return out

def watch(xaddr, n=30):
    """log the pc/lr of the next n writes to the word at xaddr"""
    out = []
    st = Stub()
    try:
        st.cmd('?')
        st.cmd(f'Z2,{xaddr:x},4')
        for k in range(n):
            r = st.cmd('c', timeout=20)
            if not r.startswith('T'): print('stop:', r); break
            R = st.regs()
            ctr, lg = st.u32(STATE), (st.read(STATE + 4, 1) or bytes(1))[0]
            xv = struct.unpack('<f', st.read(xaddr, 4))[0]
            l = f'#{k} ctr={ctr} L{lg} pc={R[15]:08x} lr={R[14]:08x} x={xv:8.2f}'
            print(l); out.append(l)
        st.cmd(f'z2,{xaddr:x},4')
    except socket.timeout:
        print('no write within 20 s (not walking?)')
        try:
            st.s.sendall(b''); st._recv(5)                      # halt before removing the watchpoint
            st.cmd(f'z2,{xaddr:x},4', timeout=3)
        except Exception: pass
    finally:
        st.detach()
    return out

if len(sys.argv) > 1:                                           # --watch ADDR: skip the search
    xa = int(sys.argv[-1], 16)
    hits = watch(xa)
    log = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'apoc_player.log')
    open(log, 'w').write(f'watched {xa:#x}\n' + '\n'.join(hits) + '\n'); sys.exit(0)

st = Stub()
try:
    st.cmd('?')
    state = st.read(STATE, 0x1C0)
    cam = st.u32(st.u32(CAMBASE + 4) + 0x80)
    tgt = np.array(struct.unpack('<3f', st.read(cam + 0x10, 12))) if cam >= 0x100000 else None
    t0 = time.time(); h1 = heap(st); print(f'read {len(h1) * BLK // 1024} KB in {time.time() - t0:.1f} s; camera {cam:#x} look-at {tgt}')
finally:
    st.detach()
if not h1: sys.exit('no memory read')
time.sleep(1.0)
st = Stub()
try:
    st.cmd('?'); h2 = heap(st)
    tgt2 = np.array(struct.unpack('<3f', st.read(cam + 0x10, 12))) if cam >= 0x100000 else None
finally:
    st.detach()
recs = {struct.unpack_from('<I', state, 0x90 + 20 * k)[0] for k in range(15)} - {0}
print('STATE rec[] nodes:', ' '.join(f'{r:#x}' for r in sorted(recs)) or 'none')
if tgt is None: tgt = np.array(struct.unpack_from('<3f', state, 0x34 + 12))   # camCur target

def segments(d):
    """merge consecutive blocks into (base, bytes)"""
    out, base, buf = [], None, []
    for a in sorted(d):
        if base is not None and a == base + BLK * len(buf): buf.append(d[a]); continue
        if buf: out.append((base, b''.join(buf)))
        base, buf = a, [d[a]]
    if buf: out.append((base, b''.join(buf)))
    return out

D = tgt2 - tgt
tol = max(0.5, 0.1 * float(np.hypot(D[0], D[2])))
print(f'look-at step between snapshots {D}')
if float(np.hypot(D[0], D[2])) < 1.0: sys.exit('the look-at point did not move: walk while the script runs')
tgt = tgt2
best = []
for base, b2 in segments(h2):
    b1 = b''.join(h1.get(base + k, bytes(BLK)) for k in range(0, len(b2), BLK))
    f1, f2 = np.frombuffer(b1, '<f4'), np.frombuffer(b2, '<f4')
    with np.errstate(all='ignore'):
        for kind, (ox, oy, oz) in (('node', (22, 26, 30)), ('xyz', (0, 1, 2))):
            m = len(f2) - 40
            if m <= 0: continue
            x, y, z = f2[ox:ox + m], f2[oy:oy + m], f2[oz:oz + m]
            px, pz = f1[ox:ox + m], f1[oz:oz + m]
            # moved by the same step as the look-at point between the snapshots (any offset from it)
            ok = (np.abs((x - px) - D[0]) < tol) & (np.abs((z - pz) - D[2]) < tol)
            if kind == 'node':
                for o in (19, 20, 21, 23, 24, 25, 27, 28, 29):   # matrix +0x4C..: rotation * scale
                    v = f2[o:o + m]; ok &= np.isfinite(v) & (np.abs(v) < 20)
            for i in np.nonzero(ok)[0][:200]:
                i = int(i); xa = base + 4 * (i + ox)
                if cam <= xa < cam + 0x40: continue                  # the camera itself
                d = float(np.hypot(f2[i + ox] - tgt[0], f2[i + oz] - tgt[2]))
                best.append((d, kind, xa, (float(f2[i + ox]), float(f2[i + oy]), float(f2[i + oz])), base + 4 * i))
best.sort()
print(f'{len(best)} candidates (closest first)')
for d, kind, xa, v, obj in best[:30]:
    tag = ('RECORDED' if obj in recs else 'not recorded') if kind == 'node' else ''
    print(f'  {kind:4s} x@{xa:#x} ({v[0]:8.2f} {v[1]:8.2f} {v[2]:8.2f}) dist {d:6.2f} {tag}')
lines = [f'look-at {tgt}', f'recs {[hex(r) for r in sorted(recs)]}']
node_c = [c for c in best if c[1] == 'node']
pick = (node_c or best)[:1]
if pick:
    xaddr = pick[0][2]
    print(f'\nwatching {pick[0][1]} x at {xaddr:#x} (30 writes, keep walking) ...')
    st = Stub()
    try:
        st.cmd('?')
        st.cmd(f'Z2,{xaddr:x},4')
        for k in range(30):
            r = st.cmd('c', timeout=20)
            if not r.startswith('T'): print('stop:', r); break
            R = st.regs()
            ctr, lg = st.u32(STATE), (st.read(STATE + 4, 1) or b'\0')[0]
            xv = struct.unpack('<f', st.read(xaddr, 4))[0]
            l = f'#{k} ctr={ctr} L{lg} pc={R[15]:08x} lr={R[14]:08x} x={xv:8.2f}'
            print(l); lines.append(l)
        st.cmd(f'z2,{xaddr:x},4')
    except socket.timeout:
        print('no write within 20 s (not walking?)')
        try:
            st.s.sendall(b'\x03'); st._recv(5)                      # halt before removing the watchpoint
            st.cmd(f'z2,{xaddr:x},4', timeout=3)
        except Exception: pass
    finally:
        st.detach()
    lines.insert(1, f'watched {pick[0][1]} x at {xaddr:#x}')
    lines += [f'cand {k} x@{xa:#x} {v} d={d:.2f}' for d, k, xa, v, o in best[:30]]
else:
    print('nothing moving near the look-at point: walk while the script runs')
log = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'apoc_player.log')
open(log, 'w').write('\n'.join(lines) + '\n')
print('log:', log)
