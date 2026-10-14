# nav icon source map positions: entry e = N + i*0x4C (+ array base): base+0xC/+0x10 floats, cell s16 base+0x8
set $N = *(unsigned*)0x44CDF0 + 0x40
set $i = 0
while $i < 8
  set $a = $N + $i*0x4c + 0x5a90
  set $b = $N + $i*0x4c + 0xe88
  printf "A%d act=%d cell=%d map=%.2f,%.2f scr=%d,%d | B%d act=%d cell=%d map=%.2f,%.2f scr=%d,%d\n", $i, *(short*)($a+0x34), *(short*)($a+0x8), *(float*)($a+0xc), *(float*)($a+0x10), *(int*)($a+0x38), *(int*)($a+0x3c), $i, *(short*)($b+0x34), *(short*)($b+0x8), *(float*)($b+0xc), *(float*)($b+0x10), *(int*)($b+0x38), *(int*)($b+0x3c)
  set $i = $i + 1
end
detach
