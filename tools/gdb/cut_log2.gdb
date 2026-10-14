# per-frame log at run-end entry: top scene camera node translation, game camera fov, 7g clamp counter + last clamped anim (frame/start/end/step)
set $n = 0
break *0x3FE8A4
commands 1
  silent
  set $n = $n + 1
  set $c = *(unsigned*)(*(unsigned*)(0x44EA58+4)+0x80)
  set $tc = *(unsigned*)0x464B10
  set $an = *(unsigned*)0x4F3FD4
  printf "%u L%u fl=%x t=%8.2f %8.2f %8.2f fov=%6.2f | clamps=%u anim=%08x f=%7.2f s=%6.2f e=%6.2f st=%5.2f\n", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, *(unsigned*)0x464AF8, *(float*)($tc+0x58), *(float*)($tc+0x68), *(float*)($tc+0x78), *(float*)($c+0x28), *(unsigned*)0x4F3FD0, $an, *(float*)($an+0x38), *(float*)($an+0x44), *(float*)($an+0x48), *(float*)($an+0x40)
  if $n >= 600
    delete
    detach
    quit
  end
  continue
end
continue
