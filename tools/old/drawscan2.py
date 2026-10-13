import re,collections,sys
src=open('dc.c').read()
funcs={}
for m in re.finditer(r'^// ==== ([0-9a-f]{8}) (\w+) (\S+)\n(.*?)(?=^// ==== |\Z)',src,re.S|re.M):
    funcs[m.group(1)]=m.group(4)
roots={}
for kind,name,prio,a,b,f1,f2 in re.findall(r'FUN_002c(5978|4c00)\("([^"]*)",([^,]*),([^,]*),([^,]*),([^,]*),([^,)]*)',src):
    p=prio.strip()
    try: pv=int(p,16) if p.startswith('0x') else int(p)
    except: continue
    if pv<0x65: continue
    m=re.match(r'K_([0-9a-f]{8})',f1.strip())
    if m: roots[m.group(1)]=name
for a,n in [('00103b58','BeforeDraw1L'),('00103d10','Draw1SetTargetProc'),('001035e0','Draw1ModelLDone'),('001026c4','Draw1CashS1'),('00106130','Draw1EplL'),('00103670','DrawULCD2DBegin'),('00102634','Draw1CashE1'),('00102724','Draw1CashS1b'),('00102678','Draw1CashE2'),('00103ba0','BeforeDraw1R'),('00102810','Draw1ModelR'),('00102760','Draw1Proc1'),('00106160','Draw1EplR'),('001027c0','Draw1Proc2'),('00103d5c','Draw2SetTarget'),('00103654','DrawDLCD2DBegin'),('00106190','Draw2Proc'),('00102cdc','DrawUFadeProc'),('00102cc8','DrawDFadeProc'),('0010b7e4','AsdModelDrawUp'),('0010b7d4','AsdModelDrawDw')]:
    roots[a]=n
field=r'\*\((?:int|short|uint|ushort|char|byte|undefined\d?|float) \*\)\(([\w\[\]]+) \+ (0x[0-9a-f]+)\)'
pat1=re.compile(field+r' = '+field+r' \+ (-?1|-?0x[0-9a-f]+);')
pat2=re.compile(r'(\w+) = '+field+r' \+ (-?1);')
calls=lambda b: set(re.findall(r'FUN_([0-9a-f]{8})',b))
seen={}
q=collections.deque((r,0,n) for r,n in roots.items())
D=int(sys.argv[1]) if len(sys.argv)>1 else 2
while q:
    f,dep,root=q.popleft()
    if f in seen or f not in funcs: continue
    seen[f]=(dep,root)
    if dep<D:
        for c in calls(funcs[f]): q.append((c,dep+1,root))
for f,(dep,root) in sorted(seen.items(),key=lambda x:(x[1][1],x[1][0])):
    b=funcs[f]; hits=[]
    for l in b.split('\n'):
        m=pat1.search(l)
        if m and m.group(1)==m.group(3) and m.group(2)==m.group(4): hits.append(f"{m.group(2)}{'+' if not m.group(5).startswith('-') else ''}{m.group(5)}")
        m=pat2.search(l)
        if m:
            v,base,off,inc=m.groups()
            if re.search(re.escape(f'+ {off}) = {v};'),b): hits.append(f"{off}{'+' if inc=='1' else ''}{inc}")
    if hits: print(f"{root:28} d{dep} {f}  {sorted(set(hits))}")
print("---- alias pass")
alias=re.compile(r'(p\w+Var\d+) = \((?:int|short|uint|ushort|char|byte|undefined\d?) \*\)\(([\w]+) \+ (0x[0-9a-f]+)\);')
for f,(dep,root) in sorted(seen.items(),key=lambda x:(x[1][1],x[1][0])):
    b=funcs[f]; hits=[]
    al={m.group(1):m.group(3) for m in alias.finditer(b)}
    for v,off in al.items():
        if re.search(re.escape('*'+v)+r' = \w+ \+ 1;',b) and not re.search(re.escape('*'+v)+r' = \w+ \+ -1;',b):
            hits.append(off+'+1')
        if re.search(re.escape('*'+v)+r' = \w+ \+ -1;',b) and not re.search(re.escape('*'+v)+r' = \w+ \+ 1;',b):
            hits.append(off+'-1')
    if hits: print(f"{root:28} d{dep} {f}  {sorted(set(hits))}")
