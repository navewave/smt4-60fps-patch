# per-frame log of EVE_2dTex children (event portraits): work *(0x43DED0), handles +4.., count +0x2C
define taskwork
  set $tw = 0
  set $tt = *(unsigned*)(*(unsigned*)0x4E3CF4 + ($arg0 >> 20)*8)
  while $tt != 0
    if *(unsigned*)($tt+0x24) == ($arg0 & 0xFFFFF)
      set $tw = *(unsigned*)($tt+0x48)
      set $tt = 0
    else
      set $tt = *(unsigned*)($tt+0x4c)
    end
  end
end
set $n = 0
break *0x3FE87C
commands 1
  silent
  set $n = $n + 1
  set $e = *(unsigned*)0x43DED0
  if $e != 0
    set $cnt = *(int*)($e+0x2c)
    printf "%u L%u off=%.1f,%.1f n=%d", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, *(float*)($e+0x60), *(float*)($e+0x64), $cnt
    set $i = 0
    while $i < $cnt && $i < 4
      taskwork *(unsigned*)($e+4+$i*4)
      if $tw != 0
        printf " |%d w=%08x i1=%d i2=%d i3=%d x4=%d y5=%d f6=%.2f f7=%.2f f8=%.3f f9=%.3f f10=%.3f f21=%.2f f22=%.2f c19=%u", $i, $tw, *(int*)($tw+4), *(int*)($tw+8), *(int*)($tw+12), *(int*)($tw+16), *(int*)($tw+20), *(float*)($tw+24), *(float*)($tw+28), *(float*)($tw+32), *(float*)($tw+36), *(float*)($tw+40), *(float*)($tw+0x84), *(float*)($tw+0x88), *(unsigned char*)($tw+0x64)
      end
      set $i = $i + 1
    end
    printf "\n"
  end
  if $n >= 400
    delete
    detach
    quit
  end
  continue
end
continue
