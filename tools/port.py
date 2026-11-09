"""Port the patch's USA 1.0 game addresses to another version.
    python tools/port.py <region>        -> ports/<region>.json  (see tools/regions.py)
Every hex constant in the sources that lies in the game's code/data range is mapped:
  code   : masked instruction window match around the address (B/BL targets and
           pointer-like literals ignored), widening the window when ambiguous;
           function pointers fall back to literal-pool votes
  data   : literal-pool votes (USA literal word d at i -> same pool slot in the target)
Patch memory (STATE block, extra pages) is not mapped here; build.py shifts it.

Apocalypse in other regions (base='apoc' in tools/regions.py): Chain maps every Apocalypse
USA address the patch uses (the values of ports/apoc.json and the native_* entries) into the
version, writes that map to ports/<key>_native.json, and ports/<key>.json = ports/apoc.json
followed by it.  Run tools/port.py apoc first after changing addresses."""
import os, re, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from regions import region, family, REGIONS, ROOT

B = 0x100000
SRC = ['patch/phase3b.S', 'patch/interp.h', 'patch/interp_arm.c', 'patch/interp_thumb.c', 'patch/interp_ext.c',
       'src/cam_stick.c', 'tools/build_release.py', 'tools/build_phase1.py', 'tools/build.py']
NOT_ADDR = {0x100000, 0x400000,                                     # pointer / offset bounds
            0x3FE3F8, 0x42AF30,                                     # USA 1.1 layout (old build.py)
            0x3FE624, 0x3FE628, 0x3FE640, 0x3FF000, 0x42AB78, 0x42AB80, 0x42B000,  # USA layout (build.py)
            0x4EFE34, 0x4EFF00, 0x4EFFF4, 0x454F46,                 # comments, IPS 'EOF'
            0x145F94, 0x146020, 0x155CA8, 0x156478, 0x156994, 0x2DD40C, 0x3F9C6C}   # Apocalypse addresses in comments
OPTIONAL_SITES = {0x1ECE64, 0x1ECF54, 0x1FC4AC}                       # COMBG line/dot draw calls, nav map draw call
# hand-checked corrections per version: {USA 1.0 address: address in that version}
# (Apocalypse versions other than apoc: {Apocalypse USA address: address in that version})
MANUAL = {
    'apoc': {
        0x10946C: 0x13DCA8,   # XFADEDL counter "add r0,r0,#1": Apocalypse keeps it in a global (ldr r0,[r1]; add; str)
        # CAMPDL camp-menu update/draw split was rewritten: hooks left out for now (0x100000 = absent)
        0x108878: 0x100000, 0x108A64: 0x100000, 0x108A60: 0x100000, 0x108A78: 0x100000,
        0x10AED8: 0x1400FC,   # AsdModelCalcProc "bl ModelCalc" (function at 0x1400F8)
        0x28C8AC: 0x32C5C8,   # SetPosA (entry 0x32C5C4, identical body)
        0x29A2B4: 0x36BB00,   # SetPosB (entry 0x36BAFC, identical body)
        0x28C8A8: 0x32C5C4, 0x29A2B0: 0x36BAFC,   # their entries (named in comments only)
        0x2BF9F0: 0x163AA4,   # sprite animator entry (now takes a float in s0; wrappers keep d0-d7)
        0x2B9220: 0x167458, 0x29538C: 0x316580, 0x296528: 0x340A94,   # animator variants (identical entries)
        0x12477C: 0x162ABC,   # model crossfade: "vadd s0,s0,s2" (step in s2, not s19): hk_blend_s2 (REGION_APOC)
        0x2BF460: 0x1835B4,   # CellPut (exact entry match; window search had picked a neighbour)
        0x25C0F0: 0x100000,   # SetScale: folded into the engine's node helpers (0x44Bxxx): scale smoothing off
        0x1FC4AC: 0x100000,   # nav map draw call: Apocalypse has no NAVI task in that form: marker refresh off
        0x39F238: 0x100000,   # its call target (only reached through 0x1FC4AC)
        0x2A8D68: 0x100000,   # layout pane renderer: named in a comment only
        # called game functions: the window search had landed mid-function (boot crash in frame_c)
        0x2C4B84: 0x163060,   # GameAlloc (inlined mutex release; main-loop caller 0x137380 = USA 0x1029CC)
        0x2BCBB0: 0x1524CC,   # CamUpdateStereo (same prologue; stereo struct fields +4)
        0x2C20A8: 0x14F0DC,   # CamSetLookAt (reordered; pos +4, target +0x10, up +0x1C, dirty +1)
        0x2C5218: 0x152F54,   # GetGameCamera *(*(0x59DBC0+4)+0x80) (had mapped into another function: no camera smoothing)
        0x1A310C: 0, 0x1A6B1C: 0,   # Status DrawUp owner range: start mapped into data: dropped
        0x464B10: 0x5B3C14,   # scene cameras = scene manager 0x5B3BF4 + 0x20 (USA 0x464AF0 + 0x20; store at
                              # 0x143480 = USA 0x1079D8). Had mapped to 0x63FFD0: dc_swap wrote/called through
                              # an unrelated object in battle (crash)
        # right-stick camera (cam_stick.c, REGION_APOC: snap +0xB4, cool-down +0xB6, &cam in r10)
        0x2645CC: 0x33FDE4,   # field yaw controller "cmp r5,#0" before the L/R step (function 0x33F9F4)
        0x266284: 0x36A548,   # camera placement "vstr s0,[r4,#100]" (window search was one word late)
        0x2D22FC: 0x339D8C,   # mode-0 camera "ldr r0,[r9,#4]" after the yaw update (r9 = 0x5806A0)
        0x25C484: 0x324B7C,   # free-roam hand-off "vstmia r1,{s0-s2}" (function 0x324B68)
        # 30 Hz detector stack walk: return addresses inside the 2D draw helpers (QD_WALK, diag builds)
        0x2C1D2C: 0x164158, 0x2B7A94: 0x16769C,   # rect-draw wrappers (0x1640CC, 0x167618), same frames
        0x2A93B8: 0x19BE64,   # layout pane renderer 0x19B770
        0x2B007C: 0x185824, 0x2AFC94: 0x1852D0,   # cell renderer 0x184EFC: rect and pane calls
        0x2BF548: 0x1836A0, 0x2BF6A8: 0x164CB0,   # CellPut 0x1835B4, 0x164B5C -> cell renderer
        0x2B0778: 0x184A98, 0x2B09A8: 0x185E74,   # 0x1849BC, 0x185DD4 -> cell renderer
    },
}
PATCH_LO = 0x4EFE38                                                 # patch STATE and pages: shifted by build.py

def consts():
    out = set()
    for f in SRC:
        for m in re.finditer(r'0[xX]([0-9A-Fa-f]{6,8})(?![0-9A-Fa-f])', open(os.path.join(ROOT, f), encoding='utf-8').read()):
            v = int(m.group(1), 16)
            if 0x100000 <= v < PATCH_LO and v not in NOT_ADDR: out.add(v)
    return sorted(out)

class Port:
    PTR_HI = 0x500000                    # literals below this count as pointers (wildcards)
    def __init__(self, key):
        self.R = region(key)
        self.U = region('us10')
        a = open(self.U['code'], 'rb').read(); n = open(self.R['code'], 'rb').read()
        self.wa = np.frombuffer(a[:len(a) // 4 * 4], '<u4')
        self.wn = np.frombuffer(n[:len(n) // 4 * 4], '<u4')
        self.ma, self.mn = self.mask(self.wa), self.mask(self.wn)
        self.na = (self.U['text_end'] - B) // 4                    # USA text words
        self.nn = (self.R['text_end'] - B) // 4
        self.glob = bool(self.R.get('glob'))
        if self.glob: self.build_index()

    # ---- whole-file search (another game on the same engine: functions reordered) ----
    K = 6
    @classmethod
    def hashes(cls, m):
        m = m.copy(); m[m == -1] = 0x7777777
        h = np.zeros(len(m) - cls.K + 1, dtype=np.uint64)
        for j in range(cls.K): h = h * np.uint64(1000003) + m[j:len(m) - cls.K + 1 + j].astype(np.uint64)
        return h
    def build_index(self):
        self.hu = self.hashes(self.ma[:self.na + 64])
        hn = self.hashes(self.mn[:self.nn])
        u, first, cnt = np.unique(hn, return_index=True, return_counts=True)
        keep = cnt == 1
        self.gkeys, self.gpos = u[keep], first[keep]           # windows that occur exactly once in the target
    def map_global(self, a, span=16, need=3):
        i = (a - B) // 4
        lo, hi = max(0, i - span), min(len(self.hu), i + span + 1)
        h = self.hu[lo:hi]
        j = np.searchsorted(self.gkeys, h)
        j[j >= len(self.gkeys)] = 0
        hit = self.gkeys[j] == h
        votes = {}
        for d, ok, k in zip(range(lo - i, hi - i), hit, j):
            if ok:
                t = B + (int(self.gpos[k]) - d) * 4
                votes[t] = votes.get(t, 0) + 1
        if not votes: return []
        best = max(votes, key=votes.get)
        if votes[best] < need or sorted(votes.values())[-2:-1] == [votes[best]]: return []
        return [best]

    def mask(self, w):
        m = w.astype(np.int64)
        br = ((w >> 25) & 7) == 5
        m = np.where(br, (w & 0xFF000000).astype(np.int64), m)    # B/BL: kind only
        ptr = (w >= 0x100000) & (w < self.PTR_HI)
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
        if self.glob:
            r = self.map_global(a)
            if r: return r[0], 'global'
            r = self.map_global(a, 48, 2)
            if r: return r[0], 'global wide'
            v = self.call_votes(a)
            if v:
                best = max(v, key=v.get)
                return best, f'callers {v[best]}/{sum(v.values())}'
            return None, 'not found (global)'
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
            m = self.map_global(site) if self.glob else self.map_text(site, 12, 20, 0x8000)
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
            if self.glob:
                m = self.map_global(a, 32, 2)
                if len(m) == 1:
                    v = int(self.wn[(m[0] - B) // 4])
                    if 0x100000 <= v < 0x800000: out[v] = out.get(v, 0) + 1
                continue
            m = self.map_text(a, 6, 6, 0x8000)
            if len(m) != 1: m = self.map_text(a, 12, 12, 0x8000)
            if len(m) != 1:
                dl = self.local_delta(a)
                m = [a + dl] if dl is not None else []
            if len(m) == 1:
                v = int(self.wn[(m[0] - B) // 4])
                if 0x100000 <= v < self.PTR_HI: out[v] = out.get(v, 0) + 1
        return out

    def run(self):
        mp, bad, notes = {}, [], []
        self.how = {}
        # hook sites (addr=...) from the build scripts: their "+4" continuations follow them
        self._sites = set()
        for f in ('tools/build_release.py', 'tools/build_phase1.py'):
            for m in re.finditer(r'addr=0x([0-9A-Fa-f]+)', open(os.path.join(ROOT, f)).read()): self._sites.add(int(m.group(1), 16))
        self._sites.add(0x103B58)                                   # BeforeDraw (b hk_beforedraw; cont +4)
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
            else: mp[v] = r; self.how[v] = how
        for v, t in MANUAL.get(self.R['key'], {}).items():
            mp[v] = t; self.how[v] = 'manual'
            bad[:] = [(x, h) for x, h in bad if x != v]
        # a hook's continuation is always its site + 4 (or absent with it)
        for v in self._sites | {0x2BF460, 0x2BF558, 0x2B06A0, 0x2B0908}:   # + runtime-hooked cell entries
            if v in mp and v + 4 in mp:
                mp[v + 4] = mp[v] + 4 if mp[v] != B else B
        # runtime hook sites the extension checks before patching (hook_call): when the game
        # has no such call, point them at _start so the check fails and the feature stays off
        for v in OPTIONAL_SITES:
            if v not in mp and any(b[0] == v for b in bad):
                mp[v] = B
                notes.append(f'{v:#x}: optional hook site not found -> feature off')
                bad[:] = [(x, h) for x, h in bad if x != v]
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
        # a function entry (push, or any bl target) must map to a matching entry, or calls into it misbehave
        wd = lambda w, a: int(w[(a - B) // 4]) if B <= a < B + 4 * len(w) else 0
        ta = self.wa[:self.na].astype(np.int64)
        bl = (ta & 0x0F000000) == 0x0B000000
        off = (ta & 0xFFFFFF) - ((ta & 0x800000) << 1)
        bl_tgt = set((B + 4 * np.nonzero(bl)[0] + 8 + 4 * off[bl]).tolist())
        for v, t in sorted(mp.items()):
            wu, wn = wd(self.wa, v), wd(self.wn, t)
            push = wu >> 16 == 0xE92D
            if t > B and (push or v in bl_tgt) and not (push and wn >> 16 == 0xE92D) and self.mask(np.array([wu], '<u4'))[0] != self.mask(np.array([wn], '<u4'))[0]:
                notes.append(f'{v:#x} -> {t:#x}: function entry maps to a non-entry: check it')
        return mp, bad, notes

class Chain(Port):
    """Apocalypse USA -> another version of Apocalypse (same game, small shifts)"""
    PTR_HI = 0x700000                    # Apocalypse data and .bss run to 0x64C11C
    def __init__(self, key):
        self.R, self.U = region(key), region(family(key))
        a = open(self.U['code'], 'rb').read(); n = open(self.R['code'], 'rb').read()
        self.wa = np.frombuffer(a[:len(a) // 4 * 4], '<u4')
        self.wn = np.frombuffer(n[:len(n) // 4 * 4], '<u4')
        self.ma, self.mn = self.mask(self.wa), self.mask(self.wn)
        self.na = (self.U['text_end'] - B) // 4
        self.nn = (self.R['text_end'] - B) // 4
        self.glob = False

    def text(self, a):
        r, how = Port.text(self, a)
        if r is None:                                        # moved further than the window search reaches
            if not hasattr(self, 'gkeys'): self.build_index()
            g = self.map_global(a) or self.map_global(a, 48, 2)
            if g: return g[0], 'global'
        return r, how

    def data(self, v):
        vt = self.votes(v)
        if not vt:                                           # reached as base + offset
            for k in range(4, 0x100, 4):
                vb = self.votes(v - k)
                if vb: vt = {kk + k: c for kk, c in vb.items()}; break
        if not vt: return None, 'no literal refs', None
        r = max(vt, key=vt.get)
        split = f'{v:#x}: ' + ', '.join(f'{k:#x}x{c}' for k, c in vt.items()) if len(vt) > 1 else None
        return r, f'votes {vt[r]}/{sum(vt.values())}', split

    def run(self):
        key, fam = self.R['key'], family(self.R['key'])
        base = {int(k, 16): int(v, 16) for k, v in json.load(open(f'{ROOT}/ports/{fam}.json')).items()}
        b = REGIONS[fam]
        native = set(b.get('native_consts', [])) | set(b.get('native_syms', {}).values()) \
                 | {s['addr'] for s in b.get('native_sites', [])} | {x for o in b.get('native_own', []) for x in o}
        want = sorted({v for v in base.values() if v > B} | native)
        mp, bad, notes = {}, [], []
        self.how = {}
        for v in want:
            if v < self.U['text_end']: r, how = self.text(v)
            else:
                r, how, split = self.data(v)
                if split: notes.append(split)
            if r is None: bad.append((v, how))
            else: mp[v] = r; self.how[v] = how
        # data nobody loads directly (reached through a far base): the shift of the nearest mapped data
        dat = sorted(v for v in mp if v >= self.U['text_end'])
        for v, how in list(bad):
            near = min(dat, key=lambda d: abs(d - v), default=None)
            if v >= self.U['text_end'] and near is not None and abs(near - v) < 0x40000:
                mp[v] = v + mp[near] - near; self.how[v] = f'shift of {near:#x}'
                notes.append(f'{v:#x}: no literal refs, took the shift of {near:#x} ({mp[near] - near:+#x}): check it')
                bad.remove((v, how))
        for v, t in MANUAL.get(key, {}).items():
            mp[v] = t; self.how[v] = 'manual'
            bad[:] = [(x, h) for x, h in bad if x != v]
        # hook continuations (site + 4): two code words 4 apart move together
        for v in want:
            if v in mp and v + 4 in mp and v + 4 < self.U['text_end'] and mp[v + 4] != mp[v] + 4:
                notes.append(f'{v + 4:#x}: {mp[v + 4]:#x} -> {mp[v] + 4:#x} (follows {v:#x})')
                mp[v + 4] = mp[v] + 4
        # owner ranges (CI_OWN through the base map, and native_own): drop when the size changed a lot
        src = open(os.path.join(ROOT, 'patch/interp_ext.c'), encoding='utf-8').read()
        blk = src[src.index('CI_OWN[][2] = {'):]
        pairs = [(base.get(int(lo, 16), 0), base.get(int(hi, 16), 0))
                 for lo, hi in re.findall(r'\{ 0x([0-9A-Fa-f]{8})u, 0x([0-9A-Fa-f]{8})u \}', blk[:blk.index('};')])]
        for lo, hi in pairs + list(b.get('native_own', [])):
            if not lo or not hi: continue
            if not (lo in mp and hi in mp and 0.7 <= (mp[hi] - mp[lo]) / (hi - lo) <= 1.4):
                notes.append(f'owner range {lo:#x}-{hi:#x} dropped (UI code differs in this version)')
                mp[lo] = mp[hi] = 0
                bad[:] = [(v, h) for v, h in bad if v not in (lo, hi)]
        # function entries must land on matching entries
        for v, t in sorted(mp.items()):
            if not (B < t < self.R['text_end'] and v < self.U['text_end']): continue
            wu, wn = int(self.wa[(v - B) // 4]), int(self.wn[(t - B) // 4])
            if wu >> 16 == 0xE92D and self.mask(np.array([wu], '<u4'))[0] != self.mask(np.array([wn], '<u4'))[0]:
                notes.append(f'{v:#x} -> {t:#x}: function entry maps to a non-entry: check it')
        # native hook sites: the original instruction must be there (build.py asserts it as well)
        for s in b.get('native_sites', []):
            t = mp.get(s['addr'])
            if t:
                w = int(self.wn[(t - B) // 4])
                if not (w == s['expect'] or (s['expect'] >> 25) & 7 == 5 and w >> 24 == s['expect'] >> 24):
                    notes.append(f"native site {s['addr']:#x} -> {t:#x}: word {w:08x}, expected {s['expect']:08x}")
        self.native = mp
        out = {}
        for k, v in base.items():                # SMT IV USA -> Apocalypse USA -> this version
            if v in (0, B): out[k] = v
            elif v in mp: out[k] = mp[v]
        return out, bad, notes

if __name__ == '__main__':
    key = sys.argv[1]
    chain = family(key) != key
    P = Chain(key) if chain else Port(key)
    mp, bad, notes = P.run()
    os.makedirs(f'{ROOT}/ports', exist_ok=True)
    json.dump({f'{k:#x}': f'{v:#x}' for k, v in sorted(mp.items())}, open(f'{ROOT}/ports/{key}.json', 'w'), indent=1)
    if chain:
        json.dump({f'{k:#x}': f'{v:#x}' for k, v in sorted(P.native.items())}, open(f'{ROOT}/ports/{key}_native.json', 'w'), indent=1)
        print(f'{key}: {len(P.native)} Apocalypse USA addresses mapped (ports/{key}_native.json)')
    print(f'{key}: {len(mp)} mapped, {len(bad)} unmapped')
    for n in notes: print('  vote split', n)
    for v, how in bad: print(f'  UNMAPPED {v:#x}: {how}')
    if '-v' in sys.argv:
        for k in sorted(P.how): print(f'  {k:#x} -> {mp[k]:#x}  {P.how[k]}')
