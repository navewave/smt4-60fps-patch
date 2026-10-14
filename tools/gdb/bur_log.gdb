# wait for BustupCutAnim work (*0x43DFC0) to appear, then log its values at every phase-0 entry (0x3FE87C)
set $n = 0
watch *(unsigned*)0x43DFC0
commands 1
  silent
  set $w = *(unsigned*)0x43DFC0
  if $w != 0
    printf "work=%08x created ctr=%u\n", $w, *(unsigned*)0x4EFE38
    delete 1
    break *0x3FE87C
    commands 2
      silent
      set $n = $n + 1
      set $c = *(unsigned*)(*(unsigned*)(0x44EA58+4)+0x80)
      printf "%u L%u s%u t=%5.2f pos=%7.2f %7.2f %7.2f tgt=%7.2f %7.2f %7.2f f530=%6.2f 640=%6.3f 64c=%6.3f gc=%08x gpos=%7.2f %7.2f %7.2f gfov=%6.2f\n", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, *(unsigned*)($w+0x5ec), *(float*)($w+0x5dc), *(float*)($w+0x50c), *(float*)($w+0x510), *(float*)($w+0x514), *(float*)($w+0x518), *(float*)($w+0x51c), *(float*)($w+0x520), *(float*)($w+0x530), *(float*)($w+0x640), *(float*)($w+0x64c), $c, *(float*)($c+4), *(float*)($c+8), *(float*)($c+12), *(float*)($c+0x28)
      if $n >= 200
        delete
        detach
        quit
      end
      continue
    end
  end
  continue
end
continue
