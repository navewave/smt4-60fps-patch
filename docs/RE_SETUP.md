# Reverse-engineering setup for Claude Code

Two kinds of access let Claude Code find the remaining bugs:

1. **Static:** Ghidra, with the project in `re/ghidra_project`, connected to Claude Code through an MCP server. Claude can then query functions, cross-references and the decompilation directly. Today it greps the 24 MB `re/dump/decomp.c` export, which also works without any setup.
2. **Runtime:** Azahar's built-in GDB stub. Claude Code can pause the running game, read memory, and set breakpoints and **watchpoints**. A watchpoint answers "which code writes this value?", which is exactly what the parked diorama/Burroughs problem needs.

## 1. Ghidra (static)
1. Install a JDK 21 and Ghidra 11.x (ghidra-sre.org). The project was made with Ghidra 11.4.2.
2. Open `re/ghidra_project/smt4.gpr` in Ghidra. The program is the 1.0 `.code`, already loaded at 0x100000 and auto-analysed.
3. Optional: install a Ghidra MCP bridge plugin so Claude Code can query Ghidra live, then add it to Claude Code with `claude mcp add …`.
   - LaurieWired/GhidraMCP is one such plugin.
   - Follow that plugin's README and check that it supports your Ghidra version.
4. Without MCP, Claude Code can still use `re/dump/decomp.c` (every function, headed `// ==== <addr> ARM FUN_xxxxxxxx`) and `re/dump/text.asm`.
5. To regenerate the dump, run `re/ghidra_scripts/ExportAll.java` in Ghidra's headless analyzer (`analyzeHeadless … -postScript ExportAll.java decomp.c`).

## 2. Azahar GDB stub (runtime)
1. In Azahar, go to Emulation → Configure → Debug. Enable **GDB stub** (port 24689 by default; the setting is `Debugging_UseGdbstub`).
2. Start the game with the patch installed. Azahar waits for a debugger to attach.
3. Install `gdb-multiarch`. On Windows the easiest route is WSL Ubuntu: `sudo apt install gdb-multiarch`, since WSL can reach `localhost:24689` on the Windows side. Arm GNU Toolchain's `arm-none-eabi-gdb` also works.
4. Connect:
   ```
   gdb-multiarch -q -ex "set architecture armv6k" -ex "target remote localhost:24689"
   ```
5. Useful commands: `x/8wx 0x4EFE38` (patch STATE), `b *0x103B58` (phase 0), `watch *(int*)ADDR` (who writes ADDR), `bt`, `info registers`, `c`.
6. Claude Code can drive GDB non-interactively with a command file, for example:
   ```
   gdb-multiarch -batch -x probe.gdb
   ```
   where `probe.gdb` sets a watchpoint, continues, prints `$pc`/`$lr`/registers on hit, then detaches.
   - The game is paused while GDB is attached.
   - Hardware watchpoints in the stub may be limited. If `watch` does not trigger, use breakpoints on candidate writers instead.

## Suggested first runtime task (diorama backdrops)
1. Open the area-select screen ("Where will you go?").
2. Find the active NW4C camera. FUN_00115B74 reads it from the scene camera list. Let Claude break in FUN_00115B74 and log the camera pointer.
3. Watch its view matrix or world translation, highlight a different place, and note which function writes it, and from which task.
4. Interpolate that value with the logic-frame recording rule (see CLAUDE.md, IMPORTANT RULE).

## Legal
Keep `game/`, `re/` and any ROM-derived data private (local, or a **private** git repository). Publish only the patch sources and the built IPS/exheader files.
