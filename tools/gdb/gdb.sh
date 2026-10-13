#!/bin/sh
# Run a GDB command file against Azahar's GDB stub (Emulation > Configure > Debug > GDB stub, port 24689).
#   sh tools/gdb/gdb.sh tools/gdb/<probe>.gdb [logfile]
GDB="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin/arm-none-eabi-gdb.exe"
LOG=${2:-${1%.gdb}.log}
"$GDB" -q -nx -batch -x "$(dirname "$0")/common.gdb" -x "$1" 2>&1 | tee "$LOG"
