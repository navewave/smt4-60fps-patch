import re,struct,sys
d=open('/home/claude/smt4/raw/decompressed_code.bin','rb').read()
src=open(sys.argv[1]).read()
def rep(m):
    a=int(m.group(1),16)
    if 0x100000<=a<0x42b000 and a%4==0:
        v=struct.unpack_from('<I',d,a-0x100000)[0]
        return 'K_%08x'%v
    return m.group(0)
open(sys.argv[2],'w').write(re.sub(r'DAT_([0-9a-f]{8})',rep,src))
