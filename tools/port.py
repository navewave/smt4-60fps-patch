"""Port the patch's USA 1.0 game addresses to another version.
    python tools/port.py <region>        -> ports/<region>.json  (see tools/regions.py)
Every hex constant in the sources that lies in the game's code/data range is mapped:
  code   : masked instruction window match around the address (B/BL targets and
           pointer-like literals ignored), widening the window when ambiguous;
           function pointers fall back to literal-pool votes
  data   : literal-pool votes (USA literal word d at i -> same pool slot in the target)
Patch memory (STATE block, extra pages) is not mapped here; build.py shifts it."""
import os, re, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import region, ROOT

B = 0x100000
SRC = ['patch/phase3b.S', 'patch/interp.h', 'patch/interp_arm.c', 'patch/interp_thumb.c', 'patch/interp_ext.c',
       'src/cam_stick.c', 'tools/build_release.py', 'tools/build_phase1.py', 'tools/build.py']
NOT_ADDR = {0x100000, 0x400000,                                     # pointer / offset bounds
            0x3FE3F8, 0x42AF30,                                     # USA 1.1 layout (old build.py)
            0x3FE624, 0x3FE628, 0x3FE640, 0x3FF000, 0x42AB78, 0x42AB80, 0x42B000,  # USA layout (build.py)
            0x4EFE34, 0x4EFF00, 0x4EFFF4, 0x454F46}                 # comments, IPS 'EOF'
PATCH_LO = 0x4EFE38                                                 # patch STATE and pages: shifted by build.py

def consts():
    out = set()
    for f in SRC:
        for m in re.finditer(r'0[xX]([0-9A-Fa-f]{6,8})(?![0-9A-Fa-f])', open(os.path.join(ROOT, f), encoding='utf-8').read()):
            v = int(m.group(1), 16)
            if 0x100000 <= v < PATCH_LO and v not in NOT_ADDR: out.add(v)
    return sorted(out)

class Port:
    def __init__(self, key):
        self.R = region(key)
        self.U = region('us10')
        a = open(self.U['code'], 'rb').read(); n = open(self.R['code'], 'rb').read()
        self.wa = np.frombuffer(a[:len(a) // 4 * 4], '<u4')
        self.wn = np.frombuffer(n[:len(n) // 4 * 4], '<u4')
        self.ma, self.mn = self.mask(self.wa), self.mask(self.wn)
        self.na = (self.U['text_end'] - B) // 4                    # USA text words
        self.nn = (self.R['text_end'] - B) // 4

    @staticmethod
    def mask(w):
        m = w.astype(np.int64)
        br = ((w >> 25) & 7) == 5
        m = np.where(br, (w & 0xFF000000).astype(np.int64), m)    # B/BL: kind only
        ptr = (w >= 0x100000) & (w < 0x500000)
        return np.where(ptr, -1, m)                               # pointer-like: wildcard

    def map_text(self, a, before=12, after=20, rng=0x8000):
        i = (a - B) // 4
        if i - before < 0 or i + after > self.na: return []
        pat = self.ma[i - before:i + after]
        lo = max(before, i - rng // 4); hi = min(self.nn - after, i + rng // 4)
        if hi <= lo: return []
        cand = np.arange(lo, hi)
        for t in np.argsort(pat == -1, kind='stable'):              # informative words first
            p = pat[t]
            if p == -1: break
            col = self.mn[cand - before + t]
            cand = cand[(col == p) | (col == -1)]
            if len(cand) == 0: break
        return [B + int(k) * 4 for k in cand]

    def text(self, a):
        for args in ((12, 20, 0x8000), (24, 40, 0x8000), (12, 20, 0x40000), (40, 64, 0x40000)):
            r = self.map_text(a, *args)
            if len(r) == 1: return r[0], 'window'
        v = self.votes(a)
        if v: return max(v, key=v.get), f'votes {len(v)}'
        v = self.call_votes(a)
        if v:
            best = max(v, key=v.get)
            return best, f'callers {v[best]}/{sum(v.values())}'
        # neighbours: a hook continuation (site + 4) or a function whose own bytes changed
        got = {}
        for k in (1, -1, 2, -2, 3, -3, 4, -4, 6, -6, 8, -8):
            for args in ((12, 20, 0x8000), (24, 40, 0x8000)):
                m = self.map_text(a + 4 * k, *args)
                if len(m) == 1:
                    got[m[0] - 4 * k] = got.get(m[0] - 4 * k, 0) + 1
                    break
            if got and max(got.values()) >= 2: break
        if got:
            best = max(got, key=got.get)
            if got[best] >= 2 or len(got) == 1: return best, f'neighbours {got[best]}'
        # ambiguous window (e.g. a generic prologue): the candidate that agrees with the shift of the body after it
        cands = self.map_text(a, 12, 20, 0x8000)
        for k in range(1, 96):
            m = self.map_text(a + 4 * k, 12, 20, 0x8000)
            if len(m) == 1:
                exp = m[0] - 4 * k
                if exp in cands: return exp, f'body shift (+{4 * k:#x})'
                break
        return None, f'window {len(r)} candidates'

    def call_votes(self, a):
        """USA B/BL instructions targeting a -> where the matching target instruction branches"""
        w = self.wa[:self.na].astype(np.int64)
        isb = ((w >> 25) & 7) == 5
        off = ((w & 0xFFFFFF) ^ 0x800000) - 0x800000
        tgt = B + np.arange(self.na) * 4 + 8 + off * 4
        out = {}
        for i in np.nonzero(isb & (tgt == a))[0][:60]:
            site = B + int(i) * 4
            m = self.map_text(site, 12, 20, 0x8000)
            if len(m) != 1:
                dl = self.local_delta(site)
                m = [site + dl] if dl is not None else []
            if len(m) != 1: continue
            wn = int(self.wn[(m[0] - B) // 4])
            if ((wn >> 25) & 7) != 5: continue
            o = ((wn & 0xFFFFFF) ^ 0x800000) - 0x800000
            t = m[0] + 8 + o * 4
            out[t] = out.get(t, 0) + 1
        return out

    def local_delta(self, a):
        """target - USA shift of the code just before a (literal pools follow their code)"""
        for k in range(1, 96):
            m = self.map_text(a - 4 * k, 12, 20, 0x8000)
            if len(m) == 1: return m[0] - (a - 4 * k)
        return None

    def votes(self, d):
        """USA literal words equal to d -> the word at the matched target position"""
        out = {}
        for i in np.nonzero(self.wa[:self.na] == d)[0][:200]:
            a = B + int(i) * 4
            m = self.map_text(a, 6, 6, 0x8000)
            if len(m) != 1: m = self.map_text(a, 12, 12, 0x8000)
            if len(m) != 1:
                dl = self.local_delta(a)
                m = [a + dl] if dl is not None else []
            if len(m) == 1:
                v = int(self.wn[(m[0] - B) // 4])
                if 0x100000 <= v < 0x500000: out[v] = out.get(v, 0) + 1
        return out

    def run(self):
        mp, bad, notes = {}, [], []
        # hook sites (addr=...) from the build scripts: their "+4" continuations follow them
        self._sites = set()
        for f in ('tools/build_release.py', 'tools/build_phase1.py'):
            for m in re.finditer(r'addr=0x([0-9A-Fa-f]+)', open(os.path.join(ROOT, f)).read()): self._sites.add(int(m.group(1), 16))
        for v in consts():
            if v < self.U['ro_start'] and v - 4 in mp and v - 4 in self._sites:
                r, how = mp[v - 4] + 4, 'site + 4'                # hook continuation
            elif v < self.U['ro_start']:
                r, how = self.text(v)
            else:
                vt = self.votes(v)
                if not vt:                                       # reached as base + offset
                    for k in range(4, 0x100, 4):
                        vb = self.votes(v - k)
                        if vb: vt = {kk + k: c for kk, c in vb.items()}; break
                if not vt: r, how = None, 'no literal refs'
                else:
                    r = max(vt, key=vt.get); how = f'votes {vt[r]}/{sum(vt.values())}'
                    if len(vt) > 1: notes.append(f'{v:#x}: ' + ', '.join(f'{k:#x}x{c}' for k, c in vt.items()))
            if r is None: bad.append((v, how))
            else: mp[v] = r
        # cell-call owner ranges (CI_OWN in interp_ext.c): drop a range for this version when it
        # cannot be mapped or its size changed a lot (that UI code was rewritten); 0..0 matches nothing
        src = open(os.path.join(ROOT, 'patch/interp_ext.c'), encoding='utf-8').read()
        blk = src[src.index('CI_OWN[][2] = {'):]
        blk = blk[:blk.index('};')]
        for lo, hi in re.findall(r'\{ 0x([0-9A-Fa-f]{8})u, 0x([0-9A-Fa-f]{8})u \}', blk):
            lo, hi = int(lo, 16), int(hi, 16)
            ok = lo in mp and hi in mp and 0.7 <= (mp[hi] - mp[lo]) / (hi - lo) <= 1.4
            if not ok:
                notes.append(f'owner range {lo:#x}-{hi:#x} dropped (UI code differs in this version)')
                mp[lo] = mp[hi] = 0
                bad[:] = [(v, h) for v, h in bad if v not in (lo, hi)]
        return mp, bad, notes

if __name__ == '__main__':
    key = sys.argv[1]
    P = Port(key)
    mp, bad, notes = P.run()
    os.makedirs(f'{ROOT}/ports', exist_ok=True)
    json.dump({f'{k:#x}': f'{v:#x}' for k, v in sorted(mp.items())}, open(f'{ROOT}/ports/{key}.json', 'w'), indent=1)
    print(f'{key}: {len(mp)} mapped, {len(bad)} unmapped')
    for n in notes: print('  vote split', n)
    for v, how in bad: print(f'  UNMAPPED {v:#x}: {how}')
