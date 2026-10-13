import re,collections,sys
src=open('dc.c').read()
funcs={}
for m in re.finditer(r'^// ==== ([0-9a-f]{8}) (\w+) (\S+)\n(.*?)(?=^// ==== |\Z)',src,re.S|re.M):
    funcs[m.group(1)]=m.group(4)
roots=sys.argv[2].split(',')
D=int(sys.argv[1])
field=r'\*\((?:int|short|uint|ushort|float|char|byte) \*\)\(([\w]+) \+ (0x[0-9a-f]+)\)'
pat=re.compile(field+r' = (?:'+field+r' \+ ([^;]+)|([^;]+) \+ '+field+r');')
pat2=re.compile(r'(\w+) = '+field+r' \+ ([^;]+);')
alias=re.compile(r'(p\w+Var\d+) = \((?:int|short|uint|ushort|float) \*\)\(([\w]+) \+ (0x[0-9a-f]+)\);')
skip={'0x124c','0x12ac','0x11ec','0x130c','0x132c','0x134c','0x126c','0x304'}
calls=lambda b: set(re.findall(r'FUN_([0-9a-f]{8})',b))
seen={}
q=collections.deque((r,0) for r in roots)
while q:
    f,dep=q.popleft()
    if f in seen or f not in funcs: continue
    seen[f]=dep
    if dep<D:
        for c in calls(funcs[f]): q.append((c,dep+1))
for f,dep in sorted(seen.items(),key=lambda x:x[1]):
    b=funcs[f]; hits=[]
    for l in b.split('\n'):
        m=pat.search(l)
        if m:
            b1,o1=m.group(1),m.group(2)
            if ((m.group(3)==b1 and m.group(4)==o1) or (m.group(7)==b1 and m.group(8)==o1)) and o1 not in skip:
                hits.append(l.strip()[:80]); continue
        m=pat2.search(l)
        if m:
            v,base,off,rhs=m.groups()
            if off not in skip and re.search(re.escape(f'+ {off}) = {v};'),b): hits.append(l.strip()[:80])
    al={m.group(1):m.group(3) for m in alias.finditer(b)}
    for v,off in al.items():
        if off in skip: continue
        if re.search(re.escape('*'+v)+r' = [^;]*\+[^;]*;',b) and re.search(re.escape('*'+v),b):
            mm=re.search(re.escape('*'+v)+r' = ([^;]*\+[^;]*);',b)
            if mm and v in mm.group(1) or (mm and re.search(r'\w+Var\d+ \+ (1|-1)$',mm.group(1))): hits.append(f'*{v}(@{off}) = {mm.group(1)[:50]}')
    if hits: print(f"d{dep} {f} ({len(b.splitlines())} lines) :: "+' || '.join(hits[:4]))
print(len(seen),'functions scanned')
