set $cam = *(unsigned*)0x464B10
set $o = *(unsigned*)($cam+0x258)
set $u = *(unsigned*)($o+0xC)
printf "cam=%08x o=%08x upd=%08x hdr=%08x\n", $cam, $o, $u, *(unsigned*)$u
x/6wx $u
printf "cam t = %f %f %f\n", *(float*)($cam+0x58), *(float*)($cam+0x68), *(float*)($cam+0x78)
x/16wx 0x4F1E80
detach
