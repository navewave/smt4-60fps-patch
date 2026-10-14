# 7a debug: inside dc_swap, after the view-updater call (0x4F0910) and at the bail-out (0x4F08F4)
set $cam = *(unsigned*)0x464B10
set $o = *(unsigned*)($cam+0x258)
printf "cam=%08x o=%08x vt=%08x fn=%08x\n", $cam, $o, *(unsigned*)$o, *(unsigned*)(*(unsigned*)$o+0x10)
break *0x4F0910
break *0x4F08F4
break *0x4F095C
set $k = 0
commands 1
  silent
  printf "after call: upd=%08x hdr=%08x r8(cam)=%08x r10(i)=%u\n", $r0, *(unsigned*)$r0, $r8, $r10
  set $k = $k + 1
  continue
end
commands 2
  silent
  printf "bail: i=%u r8=%08x\n", $r10, $r8
  set $k = $k + 1
  if $k > 12
    x/16wx 0x4F1E80
    delete
    detach
    quit
  end
  continue
end
commands 3
  silent
  printf "recorded: r6=%08x\n", $r6
  set $k = $k + 1
  continue
end
continue
