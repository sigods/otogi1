# Changelog

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - unreleased

First technical preview. [`README.md`](README.md) § Status is the
authoritative statement of what works.

### Added

- An OpenGL 3.3 push-buffer back end for the NV2A executor
  (`src/nv2a/nv2a_pb_gl.c`): the GPU rasterises the title's batches into an
  off-screen target and the frame is read back into the title's colour
  surface, so the window present, `RECOMP_FB_DUMP` and the title's own
  framebuffer reads keep working. `RECOMP_PB_GL=0` keeps the CPU rasteriser.
- The software vertex stage is decoded once per program, fetches only the
  attributes the program reads, and runs on a work-claiming thread pool
  (`RECOMP_XF_THREADS`). The start-game stage went from ~12 fps to ~40 fps on
  the same scene; the menus run at 90-120 fps.
- `RECOMP_AUTOPRESS` on POSIX hosts: taps START then A through the emulated
  pad, which drives a headless run from the logos to the start-game stage.
- Static recompilation of the retail Xbox XBE into native Linux and Windows
  x86-64 executables, built with the `xboxrecomp` toolkit.
- Boot to the title screen: logos, looping attract video, and the title's own
  main loop.
- 2D UI rendering through the software NV2A push-buffer executor
  (`RECOMP_PB_EXEC=1`).
- Streamed menu BGM with measured non-zero PCM (`RECOMP_AC97_READY=1`).
- Native Linux build, a verified MinGW-w64 cross-build, and a documented but
  untested MSVC 2022 path.
- Input through the emulated USB pad (`RECOMP_USB=1`). New Game, save creation and loading, the main menu, the start-game stage and the pause menu were reached in maintainer runs.
- The player's 3D model is drawn on the start-game stage; the stage world is not drawn.
- `tools/smoke.sh build-linux`.

### Fixed

- The input stall that stopped the title after 16-24 flips with a pad present:
  a guest USB software queue self-links when two guest threads are in the
  enqueue at once. `RECOMP_DISPATCH_GATE=1` supplies the console's single-CPU
  ordering; `src/game/main.c` now sets it by default for this title.

### Known issues

- The stage world is not drawn: the player's 3D model is drawn on the start-game stage, and only bamboo is visible. The player character, HUD, menus, cutscene and attract loop render through the OpenGL back end.
- Black screen after New Game: seen in two 2026-10-24 runs without `RECOMP_AC97_READY=1`; not seen in later repeated runs with it. Cause not established.
- The DSP is a passthrough stub, so audio effects are not emulated.
- The MSVC 2022 path is untested.
