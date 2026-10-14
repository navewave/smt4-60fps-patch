# 7e state: QSKIP, cell table and nav table heads, one nav record dump
printf "QSKIP=%x CI_HEAP=%08x NV_HEAP=%08x QD_HEAP=%08x CAMPAGE=%d\n", *(unsigned*)0x4F3FC4, *(unsigned*)0x4F3FC8, *(unsigned*)0x4F3FCC, *(unsigned*)0x4F3FC0, *(unsigned char*)0x4F0FC4
printf "site words: %08x %08x %08x %08x\n", *(unsigned*)0x2BF460, *(unsigned*)0x2BF558, *(unsigned*)0x2B06A0, *(unsigned*)0x2B0908
set $nv = *(unsigned*)0x4F3FCC
if $nv > 0x100000
  printf "nv magic=%08x sw=%u\n", *(unsigned*)$nv, *(unsigned*)($nv+4)
  x/36wx $nv+8+24*24
end
detach
