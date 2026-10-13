# Who writes the top-screen NW4C camera translation X (cam+0x58), and on which frames?
# Use on the area-select screen while moving the cursor (diorama camera active: flags bit0 set).
set $cam = *(unsigned*)0x464B10
printf "flags=%08x topcam=%08x\n", *(unsigned*)0x464AF8, $cam
set $n = 0
watch -l *(unsigned*)($cam+0x58)
commands
  silent
  set $n = $n + 1
  printf "#%d pc=%08x lr=%08x ", $n, $pc, $lr
  st
  printf "x=%f\n", *(float*)($cam+0x58)
  if $n >= 60
    delete
    detach
    quit
  end
  continue
end
continue
