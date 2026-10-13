import re,struct,collections
src=open('dc.c').read()
funcs={}
for m in re.finditer(r'^// ==== ([0-9a-f]{8}) (\w+) (\S+)\n(.*?)(?=^// ==== |\Z)',src,re.S|re.M):
    funcs[m.group(1)]=m.group(4)
d=open('/home/claude/smt4/raw/decompressed_code.bin','rb').read()
roots={}
regs=re.findall(r'FUN_002c(5978|4c00)\("([^"]*)",([^,]*),([^,]*),([^,]*),([^,]*),([^,)]*)',src)
for kind,name,prio,a,b,f1,f2 in regs:
    p=prio.strip()
    try: pv=int(p,16) if p.startswith('0x') else int(p)
    except: pv=None
    if pv is None or pv<0x65: continue
    m=re.match(r'K_([0-9a-f]{8})',f1.strip())
    if m: roots[m.group(1)]=name
# extra known draw roots (Draw* core tasks)
for a,n in [('00103b58','BeforeDraw1L'),('00103d10','Draw1SetTargetProc'),('001035e0','Draw1ModelLDone'),('001026c4','Draw1CashS1'),('00106130','Draw1EplL'),('00103670','DrawULCD2DBegin'),('00102634','Draw1CashE1'),('00102724','Draw1CashS1b'),('00102678','Draw1CashE2'),('00103ba0','BeforeDraw1R'),('00102810','Draw1ModelR'),('00102760','Draw1Proc1'),('00106160','Draw1EplR'),('001027c0','Draw1Proc2'),('00103d5c','Draw2SetTarget'),('00103654','DrawDLCD2DBegin'),('00106190','Draw2Proc'),('00102cdc','DrawUFadeProc'),('00102cc8','DrawDFadeProc'),('0010b7e4','AsdModelDrawUp'),('0010b7d4','AsdModelDrawDw')]:
    roots[a]=n
calls=lambda b: set(re.findall(r'FUN_([0-9a-f]{8})',b))
inc=re.compile(r'(\*\([^;=]*\) = \*\([^;]*\) \+ -?1;|= i?u?Var\d+ \+ -?1;)')
seen={}
q=collections.deque((r,0,n) for r,n in roots.items())
while q:
    f,dep,root=q.popleft()
    if f in seen or f not in funcs: continue
    seen[f]=(dep,root)
    if dep<3:
        for c in calls(funcs[f]): q.append((c,dep+1,root))
out=[]
for f,(dep,root) in seen.items():
    b=funcs[f]
    if 'FUN_002c' in f[:6]: pass
    hits=[l.strip() for l in b.split('\n') if inc.search(l)]
    if hits: out.append((root,dep,f,len(b.split('\n')),hits[:4]))
for o in sorted(out): print(o[0],'| d',o[1],o[2],'lines',o[3],'|',' ;; '.join(o[4])[:220])
print(len(seen),'funcs reachable')
