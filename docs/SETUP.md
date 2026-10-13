# Setting up this project on your PC (Windows)

## Option A: WSL (recommended; matches the environment the patch was built in)
1. In PowerShell (admin), run `wsl --install`, then reboot. You get Ubuntu.
2. In Ubuntu:
   ```
   sudo apt update
   sudo apt install -y python3 clang lld llvm ffmpeg gdb-multiarch unzip git
   ```
3. Unzip the project somewhere under your Linux home, e.g. `~/smt4-60fps`. Avoid `/mnt/c/...`; builds there are slow.
4. Install Claude Code in Ubuntu (see the official install instructions at docs.claude.com), then:
   ```
   cd ~/smt4-60fps
   claude
   ```
5. Ask Claude Code to "build phase6b and confirm it matches releases/". That proves the toolchain works.
6. Your Windows files are visible from WSL under `/mnt/c/...`. For example, Azahar's mod folder is usually
   `/mnt/c/Users/<you>/AppData/Roaming/Azahar/load/mods/00040000000E5C00/`.
   Claude Code can copy test builds straight there.

## Option B: native Windows
- Install Python 3 and LLVM (llvm.org Windows installer; tick "add to PATH"). The build needs `clang`, `ld.lld`, `llvm-objcopy`, `llvm-objdump` and `llvm-nm`.
- Install Claude Code for Windows, open a terminal in the project folder, and run `claude`.
- GDB: use `arm-none-eabi-gdb` from the Arm GNU Toolchain, or use WSL just for GDB.

## Version control
- `git init && git add -A && git commit -m "6b"` gives Claude Code history and safe rollbacks.
- `.gitignore` keeps `game/` and `re/` (game-derived data) out of the repository. Keep any remote **private**.

## Runtime and RE access
See `docs/RE_SETUP.md`: Ghidra (+ optional MCP plugin) and Azahar's GDB stub.
