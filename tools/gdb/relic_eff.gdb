# relic scan effect: which effect nodes exist while Flynn scans a relic?
# Every frame at phase 0 (BeforeDraw1L 0x103B58): per effect list (0x4C5830, 5 x 0x10,
# next at node+0x20) the node count, the EPL count (update fn +8 == 0x1929BC) and the
# first two distinct update fns; for the first EPL node of list 0..4 its position
# (+0x34+0x120), scale (+0x1F4) and a few colour/alpha candidate words.  900 frames.
set $n = 0
break *0x103B58
commands 1
  silent
  set $n = $n + 1
  st
  set $g = 0
  while $g < 5
    set $nd = *(unsigned*)(0x4C5830 + $g * 0x10)
    set $c = 0
    set $e = 0
    set $f1 = 0
    set $f2 = 0
    set $first = 0
    while $nd >= 0x100000 && $c < 400
      set $fn = *(unsigned*)($nd + 8)
      if $fn == 0x1929BC
        set $e = $e + 1
        if $first == 0
          set $first = $nd
        end
      end
      if $f1 == 0
        set $f1 = $fn
      else
        if $fn != $f1 && $f2 == 0
          set $f2 = $fn
        end
      end
      set $c = $c + 1
      set $nd = *(unsigned*)($nd + 0x20)
    end
    printf "| g%d n=%d epl=%d fn=%x,%x ", $g, $c, $e, $f1, $f2
    if $first != 0
      set $d = $first + 0x34
      printf "p=%.1f,%.1f,%.1f s=%.2f w=%08x,%08x,%08x,%08x ", *(float*)($d+0x120), *(float*)($d+0x124), *(float*)($d+0x128), *(float*)($d+0x1F4), *(unsigned*)($d+0x210), *(unsigned*)($d+0x214), *(unsigned*)($d+0x218), *(unsigned*)($d+0x21C)
    end
    set $g = $g + 1
  end
  printf "\n"
  if $n >= 900
    delete
    detach
    quit
  end
  continue
end
continue
