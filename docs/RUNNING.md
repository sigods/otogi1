# Running

Long form of `README.md` § Step by step. Build, run, and capture the evidence a
bug report needs.

## Build

### Linux — the verified path

$ cmake -S . -B build-linux -G Ninja
$ cmake --build build-linux -j8


Output: `build-linux/bin/otogi1`, about 19 MB. Ninja is what the commands use;
drop `-G Ninja` to let CMake pick its default generator.

Requires `src/game/recomp/gen/`. If it does not exist, run `./regen.sh` first —
see [`PIPELINE.md`](PIPELINE.md).

### Windows — MinGW-w64 cross-build, verified

Links, and under Wine reaches the title's display-mode call without faulting.

$ cmake -S . -B build-win -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
$ cmake --build build-win -j8


Output: `build-win/bin/otogi1.exe`.

### Windows — MSVC 2022, supported but untested

Needs the *Desktop development with C++* workload. The Windows SDK supplies
Direct3D 11, DXGI, `d3dcompiler` and XAudio2, so no SDL2 is required. This path
has never been run; see [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md).

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel


Output: `build\bin\Release\otogi1.exe`.

## Run

**From the project root.** The binary opens `game/default.xbe` relative to the
current directory, so running it from inside `build-linux/` fails to find the
data.

$ ./build-linux/bin/otogi1 # Linux
$ ./build-win/bin/otogi1.exe # MinGW cross-build


It runs the title's main loop indefinitely; `Ctrl+C` ends it.

**Reference run:** `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 RECOMP_AC97_READY=1 ./build-linux/bin/otogi1` with a physical gamepad reaches New Game, save creation and loading, the main menu, the start-game stage and the pause menu. On the stage the bamboo grove, the ground haze, the light shafts, the HUD and the player's 3D model are drawn.

**Headless run, no input at all:** `RECOMP_PB_EXEC=1 RECOMP_USB=1 ./build-linux/bin/otogi1` sits on the title screen, which is where the looping attract movie plays — any pad input skips it, so this is the only way to capture that phase (`BOOT_LOG.md`, 2026-10-28).

**Headless run, no pad:** `RECOMP_PB_EXEC=1 RECOMP_USB=1 RECOMP_AUTOPRESS=1 ./build-linux/bin/otogi1` taps START then A on the emulated pad, starts a new game and reaches the start-game stage in about 90-120 s. It renders through the OpenGL back end by default. Add `RECOMP_FB_DUMP=/tmp/f` and `RECOMP_FB_DUMP_FLIPS=/tmp/go` to write a BMP per presented frame while `/tmp/go` exists — the only way to look at a headless run.

**Headless run to a specific screen:** a pad *script* is deterministic where the autopress schedule is not — one 2026-10-27 run sat on the title screen for 175 s because both START taps fell before the title was reading its pad. `RECOMP_PAD_SCRIPT` takes a timeline in milliseconds (`<ms>:<button>[:<hold ms>]`, comma or newline separated, `@file` to read it from a file), e.g. START taps every 1.5 s for 24 s and then an A tap every 1.2 s; that reaches the stage and the pause menu every time (`BOOT_LOG.md`, 2026-10-27).

The OpenGL path needs a display. On a machine with none, run it under `Xvfb`; without one, `RECOMP_PB_GL=0` falls back to the CPU rasteriser, which is roughly ten times slower on the same scene.

## Switches

Each is an environment variable, read once at startup. These are the ones these
instructions use; `AGENTS.md` § 7 has the complete set.

| Variable | Effect |
|---|---|
| `RECOMP_FB_WINDOW=1` | Open the SDL framebuffer window and present frames — Linux only |
| `RECOMP_FB_DUMP_STRIDE=<n>` | Keep one frame in `n` from the dump sequence. Menus run at over a hundred frames a second and each BMP is most of a megabyte, so a dump left on for a whole run fills the disk before the scene of interest arrives; the frames kept are still consecutive |
| `RECOMP_PB_EXEC=1` | Software NV2A push-buffer executor |
| `RECOMP_USB=1` | OHCI controller and emulated pad |
| `RECOMP_AUTOPRESS=1` | Send START+A through the emulated pad (needs `RECOMP_USB`) |
| `RECOMP_AC97_READY=1` | MCPX APU and host audio output |
| `RECOMP_AUDIO_WAV=path` | Capture the mixed output to a WAV file |
| `RECOMP_SYNC_IO=1` | Complete `NtReadFile` inline (pre-audio-fix behaviour) |
| `RECOMP_AUTOPRESS=1` | Tap START then A on the emulated pad (needs `RECOMP_USB`); `RECOMP_AUTOPRESS_PERIOD_MS` sets the A-tap period |
| `RECOMP_PB_GL=0` | Keep the CPU rasteriser instead of the OpenGL back end |
| `RECOMP_PAD_SCRIPT=@file` | Drive the pad from a timeline of `<ms>:<button>` entries |
| `RECOMP_XF_THREADS=<n>` | Vertex-transform workers (default cores-1, max 8) |
| `RECOMP_DISPATCH_GATE=1` | Serialise DISPATCH_LEVEL and interrupts; set by default for this title |
| `RECOMP_GL_COMB=<n>` | Print the register-combiner registers, once per distinct configuration **and screen position**, for 2D batches; each record ends with the batch's screen box, `box x0..x1 y0..y1` |
| `RECOMP_GL_STATE=<n>` | Print the render state handed to the back end for a spread of batches |
| `RECOMP_GL_NO_CULL` / `_NO_DEPTH` / `_NO_BLEND` / `_NO_ALPHA` / `_ALLCHAN` | Force one raster gate open, to bisect a scene that draws too little |
| `RECOMP_GL_FLAT=1` | Paint every fragment that survives depth, cull and alpha magenta — the difference between "not submitted" and "not painted" |
| `RECOMP_GL_NO_COMB=1` | Skip the register combiners and modulate stage 0 by the vertex colour, the back end's original pixel stage |
| `RECOMP_GL_COMB3=<n>` / `RECOMP_GL_COMB3_MIN=<n>` | The same combiner record for 3D batches (default: batches of 300+ vertices, i.e. the world) |
| `RECOMP_GL_PROBE=x,y[,n[,minverts[,minflip]]]` | The draws whose triangles actually cover pixel `x,y`, with state, and the pixel before/after each: the answer to "which draw should have painted this pixel" when it reads as the clear colour |
| `RECOMP_GAP_LOG=<n>` | The first n screen-space batches the CPU rasteriser paints *without* the register combiners while the back end runs them — stage modes, which stages hold a texture, the first vertex and the colour |
| `RECOMP_CULL_LOG=<n>` | The first n batches that enable culling, with their flip and cull/front values |
| `RECOMP_GL_FRONT_MIRROR` | Force the opposite front-face convention, to settle it on a capture |
| `RECOMP_FOG_TRACE=<n>` | The first n fog evaluations: mode, `SET_FOG_PARAMS`, the program's distance and the factor |
| `RECOMP_FOG_ONE` | Hand every pixel the no-fog factor, to separate a fog unit that never stops fogging from a combiner that reads the register backwards |
| `RECOMP_TEX_DUMP=<prefix>` | Write every distinct texture the executor samples as a BMP, through the sampler |
| `RECOMP_PB_GL_TEXREFRESH=<n>` | Re-upload bound textures every n frames, for a title that rewrites one in place |
| `RECOMP_XF_FLAT=<n>` | Print the batches that lose every triangle and land on screen, with their inputs and vertex program |
| `RECOMP_VSH_DUMP=1` | Disassemble each vertex program the title uploads |

On a headless box, run the window under `Xvfb`.

## Capture evidence

A bug report needs the exact stderr, not a paraphrase.

$ RECOMP_PB_EXEC=1 RECOMP_USB=1 ./build-linux/bin/otogi1 2>stderr.txt


Audio, when the symptom is silence:

$ RECOMP_AC97_READY=1 RECOMP_AUDIO_WAV=/tmp/out.wav ./build-linux/bin/otogi1


Every dumped frame is announced on stderr as `[FB] frame <path> <w>x<h>`.
The combiner records deduplicate, so they are a set rather than a timeline;
this line is what places one against the frame on disk — "which batches had
appeared by the frame with the white rectangle" becomes a grep.


## Smoke test

$ ./tools/smoke.sh build-linux
$ SMOKE_SECS=30 ./tools/smoke.sh build-linux


Re-checks the README's claims against an existing build: runs the title twice,
once with audio capture and once with `RECOMP_USB=1`, and asserts the `[AV] SetDisplayMode`
line, that the title is still running when the timeout stops it (no early exit, no segmentation fault), non-silent audio, and that the USB controllers appear only
in the second run. Needs `game/` extracted.

## Differential debugging with xemu

Run the same scene in xemu under its GDB stub to learn what an address *means*
when the recompiled build disagrees. xemu reports Otogi as "Playable, minor
issues" — a reference target ([xemu compatibility page](https://xemu.app/titles/46530002/), community report, last tested 2024-06-29). Title details:
[`TITLE_OTOGI.md`](TITLE_OTOGI.md).

### Getting xemu to boot this disc
The AppImage (v0.8.120 here) is not broken when it "does not load the ISO".
Two settings in `~/.local/share/xemu/xemu/xemu.toml` decide that, and the first
one is not obvious:

- **`hdd_path` must be empty.** xemu boots the hard disk in preference to the
  DVD, so with any HDD configured the console boots that disk and the disc is
  ignored. `hdd_path = ''` makes it boot the disc.
- **`dvd_path` must point at the `.iso`,** and the image has to start at
  sector 0 of the *filesystem*, not of the disc. Otogi's USA image carries its
  video partition in front of the filesystem -- the XDVDFS volume descriptor
  sits at `0x18310000` here, i.e. a partition base of `0x18300000` -- so the
  image xemu is given must have that prefix cut off:

  ```
  dd if='Otogi - Myth of Demons (USA).iso' of=otogi_fs.iso \
     bs=2048 skip=$((0x18300000 / 2048))
  ```

With both in place the disc is read, but this title still stops on the
"Your Xbox requires service" page: the 32 KB *XBE sector* the console reads to
find `default.xbe` is part of the disc's leading area, so slicing the
filesystem out leaves it behind. A bootable image has to be rebuilt from the
extracted tree by a writer that emits that sector (`xdvdfs`'s `pack`, not
`tools.xiso`, which only reads), and that is a separate job -- the recompiled
build itself needs no emulator to run.

### Pad-script runs need the audio device

A `RECOMP_PAD_SCRIPT` capture must also set `RECOMP_AC97_READY=1`. Without it
the title reaches the main menu, stops presenting in the middle of
`d:\menu\bin\mainmenu_us.med` and never returns: the guest is waiting on the
APU, so the run looks like a hang in the menu with no error anywhere. Two
capture runs on 2026-10-28 were lost to this before it was spotted; the tell is
that `[FB] frame` stops while the kernel-ordinal counters keep climbing.

## Captures: `tools/capture.sh`

A capture is a named pad timeline, a frame dump and the environment that makes
the run deterministic, so it survives a reboot instead of living in `/tmp`:

```
tools/capture.sh to-stage 200       # front end -> the stage, standing in the grove
tools/capture.sh stage-walk 260     # ... and then walk, camera aimed at the floor
STRIDE=1 tools/capture.sh to-stage 90
EXTRA_ENV="RECOMP_GL_NO_DEPTH=1" tools/capture.sh stage-walk 200
```

The scenario names a timeline in `tools/pads/<scenario>.txt`, in
`RECOMP_PAD_SCRIPT` syntax (`<ms>:<button>[+<button>...][:<hold ms>]`, counted
from the title's first pad read). `stage-walk` is `to-stage` followed by
`stage-walk`, because the second only means anything after the first. Timelines
may be documented with `#` lines: the runner strips them, since the toolkit's
parser stops at the first entry it cannot read.

`to-stage` reaches publisher logos, the title screen, the main menu, New Game,
the pre-stage movie, the landing cutscene and gameplay in 93 s on this machine;
its A taps stop at 85 s because an A after the stage is up is an attack and an
A inside a cutscene skips it. `stage-walk` then alternates `lup` (walk) and
`rdown` (pitch the camera down) rather than holding both, because Otogi locks
the camera during its own cinematics.

The env the runner sets is not optional, and each line is explained in the
script; the one that bites silently is `RECOMP_AC97_READY=1` (see below). The
project's `src/game/main.c` now turns the executor, the USB host and the AC'97
codec on by default, so a bare `./build-linux/bin/otogi1` behaves like the
runner; an explicit environment value still wins.

Review a run without a window: the frames are BMPs, so

```
ffmpeg -framerate 30 -i runs/to-stage/f%05d.bmp -pix_fmt yuv420p out.mp4
```

gives the whole capture at a glance, and `docs/RUNNING.md`'s differential
switches (`RECOMP_GL_FLAT`, `_NO_COMB`, `_NO_DEPTH`, `_NO_CULL`) can be run
against the same scenario through `EXTRA_ENV` for an A/B pair.

For driving a screen whose arrival time cannot be predicted, `RECOMP_PAD_LIVE`
beats a fixed timeline: append a press to the file, look at the newest dumped
frame, repeat (the toolkit's `usb_gamepad.c` documents the syntax; the
burnout3 project's `docs/running.md` is the worked example of the loop).
