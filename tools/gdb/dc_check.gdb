# 7a check: is dc_swap running? dump DcRec[0..1], CAMPAGE, scene flags, then watch camera x writers
printf "CAMPAGE=%u flags=%08x topcam=%08x ext_first_word=%08x\n", *(unsigned char*)0x4F0FC4, *(unsigned*)0x464AF8, *(unsigned*)0x464B10, *(unsigned*)0x4F0000
x/32wx 0x4F1E80
set $cam = *(unsigned*)0x464B10
set $n = 0
watch -l *(unsigned*)($cam+0x58)
commands
  silent
  set $n = $n + 1
  printf "#%d pc=%08x lr=%08x ", $n, $pc, $lr
  st
  printf "x=%f\n", *(float*)($cam+0x58)
  if $n >= 40
    delete
    x/32wx 0x4F1E80
    detach
    quit
  end
  continue
end
continue
