# Otogi: Myth of Demons — technical preview v0.1

A static recompilation of the original Xbox release of *Otogi: Myth of Demons*
(FromSoftware / Sega, 2002) into native executables for Linux and Windows,
built with the [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit.
The game's own x86 code is translated to C and compiled natively — there is no
CPU emulator, no CPU interpreter, and no hand-written substitute for the
title's own logic.

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

- Linux x86-64, or Windows x86-64 (the MinGW cross-build is the verified
  Windows path; MSVC 2022 is supported but untested).
- CMake 3.20+, a C11 compiler, Ninja (optional).
- On Linux only: SDL2 and libepoxy for the video, audio and window path.
  Windows uses Direct3D 11 and XAudio2 from the Windows SDK instead.
- Python 3.10+ with `capstone` — `regen.sh` drives the vendored toolkit's
  Python tools.
- Your own legally obtained copy of the game, as an Xbox disc image (`.iso`).
  Step 3 unpacks it into `game/`; its `default.xbe` is what `regen.sh` reads.
- The recompiled C in `src/game/recomp/gen/`, produced by `regen.sh`. It is
  generated, not tracked.

The runtime toolkit is vendored at `third_party/xboxrecomp/`; no separate
checkout is needed.

## Step by step

The same steps this project is developed against, in order. Commands are for a
POSIX shell — Linux, or Git Bash on Windows.

1. **Install the prerequisites.**

   | Tool | Version | Notes |
   |---|---|---|
   | Python | 3.10 or newer | runs the vendored toolkit's Python tools |
   | capstone | 5.x | `python3 -m pip install --user capstone` |
   | CMake | 3.20 or newer | |
   | C compiler | C11 | GCC or Clang on Linux; MinGW-w64 for the verified Windows build |
   | Ninja | optional | used by the commands below |
   | SDL2, libepoxy | Linux only | video, audio and window path |

   ```
   $ python3 -c "import capstone; print(capstone.__version__)"
   5.0.1
   $ cmake --version | head -1
   cmake version 3.31.6
   ```

2. **Clone the repository.** The toolkit is vendored inside it — no second
   checkout, no patch to apply.

   ```
   git clone https://github.com/sigods/otogi1
   cd otogi1
   ```

3. **Unpack your disc image** into `game/`, using the toolkit's own XDVDFS
   reader. Otogi's USA image carries a video partition in front of the
   filesystem, so tools that assume the filesystem starts at offset 0 fail; this
   reader probes the known partition bases.

   ```
   $ cd third_party/xboxrecomp
   $ python3 -m tools.xiso unpack "/path/to/Otogi - Myth of Demons (USA).iso" -o ../../game
   $ cd ../..
   ```

   You should end up with `game/default.xbe` next to `chr/`, `map/`, `sfx/`,
   `movie/` and the rest — 1,121 files, about 2.2 GB.

4. **Regenerate the C.**

   ```
   $ ./regen.sh
   ==> xbe_parser
   ==> disasm
   ==> func_id
   ==> abi_analysis  (never skip: silent wrong signatures otherwise)
   ==> recomp  (--split 250: prevents OOM)
   ==> done. Now: cmake --build build-xr --config Release
   ```

   This writes `src/game/recomp/gen/`, which is gitignored. It takes a few
   minutes and is only needed once per disc.

5. **Build.**

   ```
   $ cmake -S . -B build-linux -G Ninja
   $ cmake --build build-linux -j8
   ```

   The executable is `build-linux/bin/otogi1`, about 19 MB.

6. **Run it from the project root.**

   ```
   $ ./build-linux/bin/otogi1
   ```

   The binary opens `game/default.xbe` relative to the current directory. Within
   a few seconds the log shows the title's own display-mode call and the kernel
   event it builds for the GPU fence:

   ```
   [AV] SetDisplayMode mode=0x080F0D12 format=0x00000012 pitch=2560 fb=0x00404000
   [KERNEL] event 0x00106B6C built by the title: host event created (notification)
   ```

   After that it sits in the title's main loop. `Ctrl+C` ends it.

7. **Turn on the implemented parts.** Each is opt-in:

   ```
   $ RECOMP_FB_WINDOW=1 RECOMP_AC97_READY=1 ./build-linux/bin/otogi1   # window + BGM
   $ RECOMP_PB_EXEC=1 ./build-linux/bin/otogi1                          # GPU command stream
   $ RECOMP_USB=1 RECOMP_AUTOPRESS=1 ./build-linux/bin/otogi1           # pad enumeration
   ```

   On a headless box, run the window under `Xvfb`. The full switch list is
   below.

### Windows

Steps 1–3 are the same. `regen.sh` is a POSIX shell script, so run step 4 once
from Git Bash or WSL. Then pick a toolchain:

- **MinGW-w64 cross-build — verified.** It links, and under Wine it reaches the
  title's display-mode call without faulting:

  ```
  $ cmake -S . -B build-win -G Ninja \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
  $ cmake --build build-win -j8
  ```

  The result is `build-win/bin/otogi1.exe`.

- **MSVC 2022 — supported, untested.** Needs the *Desktop development with C++*
  workload; the Windows SDK supplies Direct3D 11, DXGI, `d3dcompiler` and
  XAudio2, so no SDL2 is required:

  ```
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release --parallel
  ```

  The result is `build\bin\Release\otogi1.exe`.

Run from the project root, like on Linux:

```
./build-win/bin/otogi1.exe        # MinGW cross-build
.\build\bin\Release\otogi1.exe    # MSVC
```

## Switches

Every switch is an environment variable, read once at startup.

| Variable | Effect |
|---|---|
| `RECOMP_FB_WINDOW=1` | Open the SDL framebuffer window and present frames — Linux only, off by default |
| `RECOMP_PB_EXEC=1` | Software NV2A push-buffer executor |
| `RECOMP_USB=1` | OHCI controller and emulated pad (`RECOMP_AUTOPRESS=1` sends START+A) |
| `RECOMP_AC97_READY=1` | MCPX APU and host audio output (SDL2 on Linux, XAudio2 on Windows) |
| `RECOMP_AUDIO_WAV=path` | Capture the mixed output to a WAV file |
| `RECOMP_SYNC_IO=1` | Complete `NtReadFile` inline (pre-audio-fix behaviour) |

## Contributing

Corrections, bug reports and pull requests are welcome — read
[`CONTRIBUTING.md`](CONTRIBUTING.md) first. Two rules matter most: never commit
game data (the `repo hygiene` job fails if one appears), and back every claim
with the evidence behind it.

Credits, the AI assistance disclosure and upstream attribution are in
[`CONTRIBUTORS.md`](CONTRIBUTORS.md). Working with an AI agent? Point it at
[`AGENTS.md`](AGENTS.md) first; it lists the pipeline gates that fail silently.

## Licensing

This project's own source is **GPL-3.0-only** — see [`LICENSE`](LICENSE). No game
data, disc image, XBE or generated recompiled source is distributed here; you
supply your own copy (see Requirements).

`third_party/xboxrecomp/` is a vendored, MIT-licensed copy of the toolkit with
local changes; it remains MIT (see its `LICENSE` and `UPSTREAM.md`). Third-party
attribution, including the xemu-derived MCPX APU and NV2A sources, is in
[`NOTICE`](NOTICE).
