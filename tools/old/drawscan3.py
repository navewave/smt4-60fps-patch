import re,collections,sys
exec(open('/home/claude/smt4/tools/drawscan2.py').read().split('field=')[0])
field=r'\*\((?:int|short|uint|ushort|float) \*\)\(([\w]+) \+ (0x[0-9a-f]+)\)'
pat=re.compile(field+r' = (?:'+field+r' \+ ([^;]+)|([^;]+) \+ '+field+r');')
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
skip={'0x124c','0x12ac','0x11ec','0x130c','0x132c','0x134c','0x126c','0x304'}
for f,(dep,root) in sorted(seen.items(),key=lambda x:(x[1][1],x[1][0])):
    hits=[]
    for l in funcs[f].split('\n'):
        m=pat.search(l)
        if not m: continue
        b1,o1=m.group(1),m.group(2)
        if (m.group(3)==b1 and m.group(4)==o1) or (m.group(7)==b1 and m.group(8)==o1):
            if o1 in skip: continue
            hits.append(l.strip()[:90])
    if hits: print(f"{root:26} d{dep} {f} :: "+' || '.join(hits[:3]))
