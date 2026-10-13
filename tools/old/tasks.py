import re,sys
src=open('dc.c').read()
funcs={}
for m in re.finditer(r'^// ==== ([0-9a-f]{8}) (\w+) (\S+)\n(.*?)(?=^// ==== )',src,re.S|re.M):
    funcs[m.group(1)]=m.group(4)
regs=[]
for m in re.finditer(r'FUN_002c(5978|4c00)\("([^"]*)",([^,]*),([^,]*),([^,]*),([^,]*),([^,)]*)',src):
    kind,name,prio,a,b,f1,f2=m.groups()
    regs.append((name,prio.strip(),f1.strip(),f2.strip()))
seen=set()
for name,prio,f1,f2 in regs:
    if (name,prio,f1) in seen: continue
    seen.add((name,prio,f1))
    print(f"{prio:>12} {name:30} {f1:14} {f2}")
