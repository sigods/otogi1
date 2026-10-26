# Changelog

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - unreleased

First technical preview. [`README.md`](README.md) § Status is the
authoritative statement of what works.

### Added

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

### Known issues

- The stage world is not drawn: the player's 3D model is drawn on the start-game stage, and only bamboo is visible.
- Black screen after New Game: seen in two 2026-10-24 runs without `RECOMP_AC97_READY=1`; not seen in later repeated runs with it. Cause not established.
- The DSP is a passthrough stub, so audio effects are not emulated.
- The MSVC 2022 path is untested.
