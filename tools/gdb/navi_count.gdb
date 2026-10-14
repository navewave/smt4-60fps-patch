# how often do the nav marker recompute (FUN_0039E788) and marker draw (FUN_0039D2EC) run, per frame?
set $c0 = *(unsigned*)0x4EFE38
set $a = 0
set $b = 0
set $l = 0
break *0x39E788
break *0x39D2EC
commands 1
  silent
  set $a = $a + 1
  set $l = $l + *(unsigned char*)0x4EFE3C
  continue
end
commands 2
  silent
  set $b = $b + 1
  if $b >= 200
    printf "frames=%u draws=%d recomputes=%d (on logic frames: %d)\n", *(unsigned*)0x4EFE38 - $c0, $b, $a, $l
    set $N = *(unsigned*)0x44CDF0 + 0x40
    printf "N=%08x flags 5a8c..5a8f = %02x %02x %02x %02x  +0x10=%08x +0xc=%02x\n", $N, *(unsigned char*)($N+0x5a8c), *(unsigned char*)($N+0x5a8d), *(unsigned char*)($N+0x5a8e), *(unsigned char*)($N+0x5a8f), *(unsigned*)($N+0x10), *(unsigned char*)($N+0xc)
    delete
    detach
    quit
  end
  continue
end
continue
