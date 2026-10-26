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

**Reference run:** `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 RECOMP_AC97_READY=1 ./build-linux/bin/otogi1` with a physical gamepad reaches New Game, save creation and loading, the main menu, the start-game stage and the pause menu. On the stage the player's 3D model is drawn; the stage world is not.

## Switches

Each is an environment variable, read once at startup. These are the ones these
instructions use; `AGENTS.md` § 7 has the complete set.

| Variable | Effect |
|---|---|
| `RECOMP_FB_WINDOW=1` | Open the SDL framebuffer window and present frames — Linux only |
| `RECOMP_PB_EXEC=1` | Software NV2A push-buffer executor |
| `RECOMP_USB=1` | OHCI controller and emulated pad |
| `RECOMP_AUTOPRESS=1` | Send START+A through the emulated pad (needs `RECOMP_USB`) |
| `RECOMP_AC97_READY=1` | MCPX APU and host audio output |
| `RECOMP_AUDIO_WAV=path` | Capture the mixed output to a WAV file |
| `RECOMP_SYNC_IO=1` | Complete `NtReadFile` inline (pre-audio-fix behaviour) |

On a headless box, run the window under `Xvfb`.

## Capture evidence

A bug report needs the exact stderr, not a paraphrase.

$ RECOMP_PB_EXEC=1 RECOMP_USB=1 ./build-linux/bin/otogi1 2>stderr.txt


Audio, when the symptom is silence:

$ RECOMP_AC97_READY=1 RECOMP_AUDIO_WAV=/tmp/out.wav ./build-linux/bin/otogi1


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
