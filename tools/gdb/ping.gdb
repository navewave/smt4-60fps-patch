# connection test: print where the game is, patch state, scene flags, then resume
printf "pc=%08x lr=%08x\n", $pc, $lr
st
printf "\nscene flags=%08x topcam=%08x botcam=%08x gamecam=%08x\n", *(unsigned*)0x464AF8, *(unsigned*)0x464B10, *(unsigned*)0x464B1C, *(unsigned*)(*(unsigned*)0x44EA58+4)+0x80
detach
