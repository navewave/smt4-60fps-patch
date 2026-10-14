# nav map icon arrays (FUN_0039D2EC): N = *(0x44CDF0)+0x40
#   A: count N+0x61B0, entry N+i*0x4C: active s16 +0x5AC4, cell s16 +0x5AC6, pos s32 +0x5AC8/+0x5ACC
#   B: count N+0x5A88, entry N+i*0x4C: active s16 +0xEBC,  cell s16 +0xEBE,  pos s32 +0xEC0/+0xEC4
set $N = *(unsigned*)0x44CDF0 + 0x40
printf "N=%08x countA=%d countB=%d\n", $N, *(int*)($N+0x61b0), *(int*)($N+0x5a88)
set $i = 0
while $i < 6
  set $e = $N + $i*0x4c
  printf "A%d act=%d cell=%d pos=%d,%d | B%d act=%d cell=%d pos=%d,%d\n", $i, *(short*)($e+0x5ac4), *(short*)($e+0x5ac6), *(int*)($e+0x5ac8), *(int*)($e+0x5acc), $i, *(short*)($e+0xebc), *(short*)($e+0xebe), *(int*)($e+0xec0), *(int*)($e+0xec4)
  set $i = $i + 1
end
detach
