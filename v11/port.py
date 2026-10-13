import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
import struct, sys
B = 0x100000
A = open(os.path.join(ROOT, 'game/v10/decompressed_code.bin'), 'rb').read()
N = open(os.path.join(ROOT, 'game/v11/decompressed_code.bin'), 'rb').read()
wa = struct.unpack('<%dI' % (len(A) // 4), A)
wn = struct.unpack('<%dI' % (len(N) // 4), N)
TEXT_END_A, TEXT_END_N = 0x3FE624, 0x3FE3F8

def masked(w):
    if (w >> 25) & 7 == 5: return w & 0xFF000000          # B/BL: keep cond+op only
    if 0x100000 <= w < 0x500000: return None              # pointer-like literal
    return w

def map_text(a, before=12, after=20, rng=0x6000):
    i = (a - B) // 4
    pat = [masked(wa[j]) for j in range(i - before, i + after)]
    best = []
    lo = max(before, i - rng // 4); hi = min(len(wn) - after, i + rng // 4)
    for k in range(lo, hi):
        ok = True
        for t, p in enumerate(pat):
            if p is None: continue
            if masked(wn[k - before + t]) != p: ok = False; break
        if ok: best.append(k)
    return [B + k * 4 for k in best]

def map_data(d):
    """vote using literal pool words equal to d in 1.0 text, mapped to 1.1"""
    votes = {}
    for i in range(0, (TEXT_END_A - B) // 4):
        if wa[i] != d: continue
        m = map_text(B + i * 4, 6, 6, 0x3000)
        if len(m) == 1:
            v = wn[(m[0] - B) // 4]
            votes[v] = votes.get(v, 0) + 1
    return votes

if __name__ == '__main__':
    for s in sys.argv[1:]:
        a = int(s, 16)
        if a < 0x42B000:
            r = map_text(a)
            print(f'{a:#x} -> {[hex(x) for x in r]}', 'same word' if len(r) == 1 and wn[(r[0]-B)//4] == wa[(a-B)//4] else f'1.0 {wa[(a-B)//4]:08x} 1.1 {[hex(wn[(x-B)//4]) for x in r]}')
        else:
            print(f'{a:#x} data -> {map_data(a)}')
