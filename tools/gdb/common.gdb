# shared setup: connect to Azahar, no prompts
set pagination off
set confirm off
set height 0
set architecture armv6k
set remotetimeout 20
target remote localhost:24689
# patch STATE (interp.h): +0 counter, +4 logic flag
define st
  printf "ctr=%u logic=%u ", *(unsigned*)0x4EFE38, *(unsigned char*)0x4EFE3C
end
