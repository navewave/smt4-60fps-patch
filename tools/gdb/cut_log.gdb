# per-frame log at run-end entry (0x3FE8A4: blended values still in place, restored right after)
# game camera pos/target/fov, core camera swap ptr, FovRec (0x4F0FD8), DcRec[0..1] (0x4F1E80), scene flags
set $n = 0
break *0x3FE8A4
commands 1
  silent
  set $n = $n + 1
  set $c = *(unsigned*)(*(unsigned*)(0x44EA58+4)+0x80)
  set $tc = *(unsigned*)0x464B10
  printf "%u L%u fl=%x gc=%08x pos=%8.2f %8.2f %8.2f tgt=%8.2f %8.2f %8.2f fov=%6.2f | camSw=%08x | fov sw=%u last=%6.2f mid=%6.2f | dc0 sw=%u dc1 sw=%u tcam=%08x t=%8.2f %8.2f %8.2f\n", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, *(unsigned*)0x464AF8, $c, *(float*)($c+4), *(float*)($c+8), *(float*)($c+12), *(float*)($c+16), *(float*)($c+20), *(float*)($c+24), *(float*)($c+0x28), *(unsigned*)0x4EFEB4, *(unsigned*)0x4F0FE0, *(float*)0x4F0FE4, *(float*)0x4F0FEC, *(unsigned*)0x4F1E8C, *(unsigned*)0x4F1ECC, $tc, *(float*)($tc+0x58), *(float*)($tc+0x68), *(float*)($tc+0x78)
  if $n >= 600
    delete
    detach
    quit
  end
  continue
end
continue
