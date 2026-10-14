# per-frame log of portrait tasks (prio-2 list): EVE_BustupTex (fn 0x1CE3A4) and EVE_2dTex children (fn 0x1DFB60)
# values at phase-0 entry (0x3FE87C) are the true ones (midpoints are restored at run-end). 360 frames.
set $n = 0
break *0x3FE87C
commands 1
  silent
  set $n = $n + 1
  printf "%u L%u", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C
  set $k = *(unsigned*)(*(unsigned*)0x4E3CF4 + 16)
  while $k != 0
    set $f = *(unsigned*)($k+0x3c)
    set $w = *(unsigned*)($k+0x48)
    if $f == 0x1CE3A4
      printf " |B%x st%x,%x,%x tx%x,%x,%x,%x,%x a28=%.3f a88=%.3f h=%.1f,%.1f p=%.1f,%.1f,%.1f o58=%.1f", $w & 0xFFFF, *(unsigned*)($w+0xc), *(unsigned*)($w+0x24), *(unsigned*)($w+0x78), *(unsigned*)($w+0x10), *(unsigned*)($w+0x14), *(unsigned*)($w+0x80), *(unsigned*)($w+0x84), *(unsigned char*)($w+0x8d), *(float*)($w+0x28), *(float*)($w+0x88), *(float*)($w+0x1c), *(float*)($w+0x20), *(float*)($w+0x34), *(float*)($w+0x38), *(float*)($w+0x3c), *(float*)($w+0x58)
    end
    if $f == 0x1DFB60
      printf " |E%x st%x tx%x,%x,%x z%x x=%d o=%.2f a=%.3f", $w & 0xFFFF, *(unsigned*)($w+0xc), *(unsigned*)($w+0x3c), *(unsigned*)($w+0x40), *(unsigned*)($w+0x48), *(unsigned char*)($w+0x64), *(int*)($w+0x10), *(float*)($w+0x1c), *(float*)($w+0x24)
    end
    set $k = *(unsigned*)($k+0x4c)
  end
  printf "\n"
  if $n >= 360
    delete
    detach
    quit
  end
  continue
end
continue
