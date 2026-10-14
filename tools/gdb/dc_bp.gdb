# does phase 0 / ext_watch_swap / dc_swap run? count hits for each over ~3 s of game time
break *0x3FE87C
break *0x4F018C
break *0x4F0884
set $a = 0
set $b = 0
set $c = 0
commands 1
  silent
  set $a = $a + 1
  continue
end
commands 2
  silent
  set $b = $b + 1
  continue
end
commands 3
  silent
  set $c = $c + 1
  if $c == 1
    printf "dc_swap r0(t)=%u ctr=%u\n", $r0, *(unsigned*)0x4EFE38
  end
  if $c >= 30
    printf "beforedraw=%d ext_watch_swap=%d dc_swap=%d\n", $a, $b, $c
    delete
    detach
    quit
  end
  continue
end
continue
