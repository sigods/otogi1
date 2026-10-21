# Otogi: Myth of Demons — technical preview v0.1

A static recompilation of the original Xbox release of *Otogi: Myth of Demons*
(FromSoftware / Sega, 2002) into a native Linux executable, built with the
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit. The game's own
x86 code is translated to C and compiled natively — there is no emulator, no
interpreter, and no hand-written substitute for the title's own logic.

**This is a research preview, not a playable port.** It boots the retail XBE,
runs the title's own main loop, streams the menu BGM, and opens a window. The
screen shows UI only; the 3D scene is not drawn yet.

**AI-assisted.** Code, debugging, and documentation were produced with
substantial AI assistance; the maintainer reviewed and verified every change
and claim in this repository.

## Status

| Component | State | Notes |
|---|---|---|
| Boot | Works | Loads the XBE, reaches `[AV] SetDisplayMode`, runs the main loop. No crash. |
| Kernel | Works | Threads, KEVENTs, the NV2A fence event, and asynchronous `NtReadFile` (`RECOMP_SYNC_IO=1` restores inline completion). |
| File I/O | Works | All XBE sections, assets, and the title's packed `.dev`/`.000` archives. |
| Audio | Works (BGM) | `RECOMP_AC97_READY=1`: streamed menu BGM with measured non-zero PCM. The DSP is a passthrough stub, so effects are not emulated. |
| Input | Partial | `RECOMP_USB=1`: the pad is enumerated and reports are served, but the title does not consume them yet. |
| Rendering | Partial | `RECOMP_PB_EXEC=1`: command stream decoded, triangles rasterised. The scene's source texture is never filled, so only UI is visible. |

Details and exact log lines: [`docs/BOOT_LOG.md`](docs/BOOT_LOG.md).

## Requirements

- Linux x86-64, CMake 3.20+, a C11 compiler, Ninja (optional).
- SDL2 and libepoxy for the video/audio/window path.
- Your own copy of the game, unpacked into `game/` (see below).
- The recompiled C in `src/game/recomp/gen/`, produced by `regen.sh`. It is
  generated, not tracked.

The runtime toolkit is vendored at `third_party/xboxrecomp/`; no separate
checkout is needed.

## Build

```sh
./regen.sh                 # once, after unpacking game/default.xbe
cmake -S . -B build-linux
cmake --build build-linux -j8
```

The executable is `build-linux/bin/otogi1`.

## Run

Run from the project root — the binary opens `game/default.xbe` relative to the
current directory:

```sh
./build-linux/bin/otogi1
```

Useful switches:

| Variable | Effect |
|---|---|
| `RECOMP_FB_WINDOW=1` | Open the SDL window and present frames (off by default) |
| `RECOMP_PB_EXEC=1` | Software NV2A push-buffer executor |
| `RECOMP_USB=1` | OHCI controller and emulated pad (`RECOMP_AUTOPRESS=1` sends START+A) |
| `RECOMP_AC97_READY=1` | MCPX APU and SDL2 audio output |
| `RECOMP_AUDIO_WAV=path` | Capture the mixed output to a WAV file |
| `RECOMP_SYNC_IO=1` | Complete `NtReadFile` inline (pre-audio-fix behaviour) |

## Game data

No game data, disc image, XBE, or generated source is distributed here.
`.gitignore` excludes `game/`, `*.xbe`, `*.iso`, and `src/game/recomp/gen/`.

Supply your own legally obtained copy, unpack it so that `game/default.xbe`
exists, and run `regen.sh`. Do not use this project with data you are not
entitled to use.

## Licensing

This project's own source is **GPL-3.0-only** — see [`LICENSE`](LICENSE).
`third_party/xboxrecomp/` is a vendored, MIT-licensed copy of the toolkit with
local changes; it remains MIT (see its `LICENSE` and `UPSTREAM.md`). Third-party
attribution, including the xemu-derived MCPX APU and NV2A sources, is in
[`NOTICE`](NOTICE).
