import struct,sys
# mkelf.py out.elf  name:addr:file:flags ...  flags: x=exec, w=write
out=sys.argv[1]; secs=[]
for a in sys.argv[2:]:
    n,addr,f,fl=a.split(':'); secs.append((n,int(addr,16),open(f,'rb').read(),fl))
shstr=b'\0'; names=[]
for s in secs: names.append(len(shstr)); shstr+=s[0].encode()+b'\0'
shn=len(shstr); shstr+=b'.shstrtab\0'
off=0x34; body=b''; offs=[]
for s in secs:
    offs.append(off+len(body)); body+=s[2]
    while len(body)%4: body+=b'\0'
shstroff=off+len(body); body+=shstr
while len(body)%4: body+=b'\0'
shoff=off+len(body)
hdr=b'\x7fELF\x01\x01\x01'+b'\0'*9+struct.pack('<HHIIIIIHHHHHH',2,40,1,secs[0][1],0,shoff,0x05000000,0x34,0,0,40,len(secs)+2,len(secs)+1)
sh=b'\0'*40
for i,s in enumerate(secs):
    flags=2|(4 if 'x' in s[3] else 0)|(1 if 'w' in s[3] else 0)
    sh+=struct.pack('<IIIIIIIIII',names[i],1,flags,s[1],offs[i],len(s[2]),0,0,4,0)
sh+=struct.pack('<IIIIIIIIII',shn,3,0,0,shstroff,len(shstr),0,0,1,0)
open(out,'wb').write(hdr+body+sh)
