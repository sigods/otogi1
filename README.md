# Otogi: Myth of Demons — static recompilation

A static recompilation of the original Xbox version of *Otogi: Myth of Demons*
(FromSoftware / Sega, 2002) into a native Windows x86-64 executable, using the
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit. The game's own
x86 code is lifted to C and compiled natively — no emulation, no interpreter,
no hand-written game logic.

**Generated source and game data are not distributed.** You supply your own
disc image; the recompiler runs on your machine and writes the C into a
gitignored folder.

## Quick start

```
# 1. Unpack your own disc image into game/
py -3 -m tools.xiso unpack "Otogi - Myth of Demons.iso" -o game/

# 2. Regenerate the C (Git Bash)
./regen.sh

# 3. Build
cmake -S . -B build-xr -G "Visual Studio 17 2022" -A x64
cmake --build build-xr --config Release --parallel 2

# 4. Run
bin\otogi1.exe 2>stderr.txt
```

It will crash on first run. That is expected — start at `AGENTS.md` §6.

## Layout

- `AGENTS.md` — operating manual for coding agents (read first).
- `docs/` — title facts, pipeline, architecture, boot log.
- `config/` — title-specific seeds and overrides.
- `src/game/` — boot constants and manual overrides (generated C is gitignored).
