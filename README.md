# Otogi: Myth of Demons — technical preview v0.1

This is a static recompilation of the original Xbox release of *Otogi: Myth of
Demons* (FromSoftware / Sega, 2002) into a native executable built with the
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit. The game's own
x86 code is translated to C and compiled natively — there is no emulator and no
interpreter, and no hand-written game logic is substituted for the title's own.
This v0.1 preview boots the retail XBE on Linux, creates the title's D3D device
and frame-completion event, loads assets through the recompiled kernel and file
I/O paths, streams the menu BGM, and runs the title's own main loop headless. It
does not yet produce a picture: the software push-buffer executor decodes the
guest's command stream behind `RECOMP_PB_EXEC`, but the title's vertex program
writes `w = inf`, so every triangle is rejected before rasterisation.

## Status

| Component | State | Notes |
|---|---|---|
| Boot | Works | Loads the XBE, reaches `[AV] SetDisplayMode`, and runs the title's main loop headless. A 25 s run exits on the test timeout without crashing. |
| Kernel | Works | Kernel, thread, and synchronization paths are recompiled/native. Title-built KEVENTs and the NV2A fence-completion event are handled. Asynchronous `NtReadFile` completes with `STATUS_PENDING` (default; `RECOMP_SYNC_IO=1` restores inline completion). |
| File I/O | Works | Loads all 12 XBE sections and reads title assets from the `game/` directory, including the title's packed `.dev`/`.000` archives. |
| Input | Experimental | USB OHCI and emulated pads are behind `RECOMP_USB`; the guest never reaches the pad path (no OHCI access, no call into the host pad bridge), so the pad cache stays `0xFFFFFFFF`. No usable controls are confirmed. |
| Audio | Experimental | `RECOMP_AC97_READY=1` brings up the MCPX APU and SDL2 output. Streamed menu BGM now produces measured non-zero PCM (peak ≈ 32000, RMS ≈ 4900 over 13 s, captured with `RECOMP_AUDIO_WAV`); the DSP is a passthrough stub, so effects and menu flow are not verified. |
| Rendering | Not implemented | The software push-buffer executor exists behind `RECOMP_PB_EXEC` and decodes clears and draws, but produces a black frame: the title's vertex program outputs `w = inf` and every triangle is rejected. The NV2A register aperture has no register semantics. |

Current behavior, including the exact boot log lines, is recorded in
[`docs/BOOT_LOG.md`](docs/BOOT_LOG.md).

## Building on Linux

Requirements:

- Linux x86-64, CMake 3.20+, and a C11 compiler (the existing `build-linux`
  directory was configured with Ninja).
- The `xboxrecomp` toolkit checked out alongside this project at
  `../xboxrecomp` (or pass `-DXBOXRECOMP_DIR=/path/to/xboxrecomp`).
- Generated recompiled sources in `src/game/recomp/gen/`, produced by
  `regen.sh` after you supply game data (see below). They are not tracked.

From the project root:

```
cmake -S . -B build-linux
cmake --build build-linux -j8
```

The executable is written to `build-linux/bin/otogi1`.

Run it from the project root:

```
./build-linux/bin/otogi1
```

The working directory matters. `src/game/main.c` opens `game/default.xbe`
relative to the current directory, not relative to the executable. Running the
binary from anywhere else fails with:

```
Cannot open XBE: game/default.xbe
```

## You must provide the game data

This repository ships no game data and no generated recompiled code.
`.gitignore` excludes `game/`, `*.xbe`, `*.iso`, and
`src/game/recomp/gen/`, so none of those files are part of the tracked tree.

You must supply your own legally obtained copy of the game (XBE or disc image),
unpack it into `game/` so that `game/default.xbe` exists, and run `regen.sh` to
generate the recompiled C before building. The repository does not include the
recompiler or the disc; both come from the `xboxrecomp` toolkit and your own
disc. Do not use this project with data you are not entitled to use.

## Known limitations

- No rendering: the GPU command stream is not decoded and the screen stays
  blank. This is the main blocker for a visual build.
- The intro movie / XMV path is incomplete. Its demux expects asynchronous read
  completion, which the kernel now provides by default, but the movie path is
  still unverified.
- Input is not usable for play; pad support is experimental and has only been
  exercised during boot.
- Audio is experimental: streamed menu BGM is verified, but the DSP is a
  passthrough stub, so effects and the menu flow are unverified.
- Only the Linux build is documented here. A Windows host path exists in
  `src/game/main.c`, but it is not verified in this checkout.
- This is a single-title research preview, not a general Xbox runtime. No
  performance, compatibility, or completeness claims are made.

## Licensing

This project's own source is licensed under the **GNU General Public License,
version 3 only** (GPL-3.0-only). The full text is in [`LICENSE`](LICENSE);
[`NOTICE`](NOTICE) records the third-party attribution.

The runtime it builds against, `xboxrecomp` (default checkout
`../xboxrecomp`), is MIT-licensed; its `LICENSE` is at
`../xboxrecomp/LICENSE`. The toolkit changes this project needs are carried as
`xboxrecomp-linux.patch`, which applies to upstream tag `v0.15.0`; that patch is
derived from MIT-licensed code and remains under those terms. Third-party
attribution, including the xemu-derived MCPX APU and NV2A sources, is in
`../xboxrecomp/NOTICE`, and the applicable LGPL-2.1 text is
`../xboxrecomp/LICENSES/LGPL-2.1.txt`. The xemu-derived components remain under
their upstream licences and are not relicensed by this project.
