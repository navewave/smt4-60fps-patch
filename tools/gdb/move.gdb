# "Move" task (moflex video player): work *(0x44D04C), state +0x10, player result +0x14, file name +0x1B
set $w = *(unsigned*)0x44D04C
printf "work=%08x state=%u player=%u\n", $w, *(unsigned*)($w+0x10), *(unsigned*)($w+0x14)
x/s $w+0x1b
detach
