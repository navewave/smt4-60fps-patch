set $w = *(unsigned*)0x43DFC0
printf "work=%08x state=%u t=%f/%f fn=%08x\n", $w, *(unsigned*)($w+0x5ec), *(float*)($w+0x5dc), *(float*)($w+0x5d8), *(unsigned*)($w+0x5e0)
printf "pos %f %f %f  tgt %f %f %f\n", *(float*)($w+0x50c), *(float*)($w+0x510), *(float*)($w+0x514), *(float*)($w+0x518), *(float*)($w+0x51c), *(float*)($w+0x520)
printf "530: %f %f %f  540 %f  640 %f 644 %f 648 %f 64c %f 650 %f\n", *(float*)($w+0x530), *(float*)($w+0x534), *(float*)($w+0x538), *(float*)($w+0x540), *(float*)($w+0x640), *(float*)($w+0x644), *(float*)($w+0x648), *(float*)($w+0x64c), *(float*)($w+0x650)
printf "cam(work+4): pos %f %f %f fov %f  | gamecam=%08x\n", *(float*)($w+8), *(float*)($w+0xc), *(float*)($w+0x10), *(float*)($w+4+0x28), *(unsigned*)(*(unsigned*)(0x44EA58+4)+0x80)
printf "scene flags=%08x\n", *(unsigned*)0x464AF8
detach
