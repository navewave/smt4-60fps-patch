# who writes the top scene camera x (+0x58) when it jumps (> 2 units)?  log 40 jumps
set $cam = *(unsigned*)0x464B10
set $prev = *(float*)($cam+0x58)
set $k = 0
set $hits = 0
watch -l *(unsigned*)($cam+0x58)
commands
  silent
  set $hits = $hits + 1
  set $v = *(float*)($cam+0x58)
  if $v - $prev > 2 || $prev - $v > 2
    set $k = $k + 1
    printf "#%d ctr=%u L%u pc=%08x lr=%08x x=%8.2f -> %8.2f  y=%8.2f z=%8.2f\n", $k, *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, $pc, $lr, $prev, $v, *(float*)($cam+0x68), *(float*)($cam+0x78)
  end
  set $prev = $v
  if $k >= 40 || $hits >= 20000
    printf "hits=%d\n", $hits
    delete
    detach
    quit
  end
  continue
end
continue
