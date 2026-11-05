import re,struct,pickle
a=open('Shin Megami Tensei IV - Apocalypse/code.bin','rb').read()
pat=re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)')
rows=[(int(m[1],16),int(m[2],16),m[3]) for m in map(pat.match,open('build/apoc/text.asm',errors='replace').read().splitlines()) if m]
idx={r[0]:i for i,r in enumerate(rows)}
def st(ad):
    o=ad-0x100000
    if not 0<=o<len(a)-1: return None
    e=a.find(b'\0',o); t=a[o:e]
    return t.decode() if 3<=len(t)<48 and all(32<=c<127 for c in t) else None
def imm12(w):
    r=(w>>8&15)*2; v=w&255
    return ((v>>r)|(v<<(32-r)))&0xffffffff if r else v
out={}   # task fn -> names
for i,(ad,w,t) in enumerate(rows):
    if (w&0x0FFF0000)==0x028F0000:           # add rd, pc, #imm
        s=st(ad+8+imm12(w))
        if not s: continue
        # look around for a literal function pointer loaded near
        for j in range(max(0,i-10),min(len(rows),i+11)):
            ad2,w2,t2=rows[j]
            cand=None
            if (w2&0x0FFF0000)==0x059F0000: 
                la=ad2+8+(w2&0xfff)
                if 0x100000<=la<0x520000: cand=struct.unpack_from('<I',a,la-0x100000)[0]
            elif (w2&0x0FFF0000)==0x028F0000: cand=ad2+8+imm12(w2)
            if cand and 0x100000<=cand<0x525000 and (cand-0x100000+4<=len(a)) and (struct.unpack_from('<I',a,cand-0x100000)[0]>>16)&0xffff in(0xE92D,) :
                out.setdefault(cand,set()).add(s)
pickle.dump(out,open('build/apoc/tasknames.pkl','wb'))
print(len(out),'fn-name pairs')
for t in (0x2E7A8C,0x2E169C,0x2EFA8C,0x4A7260,0x4A723C,0x4A7588,0x4A7524,0x2BA754,0x14F3F4,0x13D0F0,0x202450,0x25CF44,0x227418,0x26C1A8,0x22B2D4,0x298674,0x267C70,0x229324):
    print(hex(t),sorted(out.get(t,[]))[:6])
for f,n in sorted(out.items()):
    if any(re.search('NAVI|Navi|navi',x) for x in n): print('NAVI',hex(f),sorted(n)[:5])
