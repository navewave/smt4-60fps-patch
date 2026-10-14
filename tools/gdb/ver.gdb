x/2wx 0x4F3000
# list live tasks: bucket array *(L), count *(L+4); task +0x18 hash, +0x20 prio, +0x28 handle, +0x3C calc, +0x4C next
set $L = 0x4e3cf4
set $nb = *(int*)($L+4)
set $i = 0
while $i < $nb
  set $t = *(unsigned*)(*(unsigned*)$L + $i*8)
  while $t != 0
    printf "prio %3x hash %5x calc %08x draw40 %08x work28 %08x flags %08x\n", *(int*)($t+0x20), *(unsigned*)($t+0x18), *(unsigned*)($t+0x3c), *(unsigned*)($t+0x40), *(unsigned*)($t+0x28), *(unsigned*)($t+0x1c)
    set $t = *(unsigned*)($t+0x4c)
  end
  set $i = $i + 1
end
detach
