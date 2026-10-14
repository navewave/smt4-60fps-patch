# general per-frame log at phase-0 entry (0x3FE87C): scene flags, top scene camera node + view rot,
# game camera pos/fov, BustupCutAnim work. 300 frames.
set $n = 0
break *0x3FE87C
commands 1
  silent
  set $n = $n + 1
  set $tc = *(unsigned*)0x464B10
  set $c = *(unsigned*)(*(unsigned*)(0x44EA58+4)+0x80)
  set $u = *(unsigned*)(*(unsigned*)($tc+0x258)+0xC)
  printf "%u L%u fl=%x tcam=%08x t=%7.2f %7.2f %7.2f r=%7.3f %7.3f %7.3f | gpos=%7.2f %7.2f %7.2f gtgt=%7.2f %7.2f %7.2f fov=%6.2f | bw=%08x\n", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C, *(unsigned*)0x464AF8, $tc, *(float*)($tc+0x58), *(float*)($tc+0x68), *(float*)($tc+0x78), *(float*)($u+8), *(float*)($u+12), *(float*)($u+16), *(float*)($c+4), *(float*)($c+8), *(float*)($c+12), *(float*)($c+0x10), *(float*)($c+0x14), *(float*)($c+0x18), *(float*)($c+0x28), *(unsigned*)0x43DFC0
  if $n >= 300
    delete
    detach
    quit
  end
  continue
end
continue
