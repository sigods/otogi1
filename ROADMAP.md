# Roadmap

☑ done · ◐ partial · ☐ not started

| Milestone | Exit condition | Status | Evidence |
|---|---|---|---|
| M0 — Pipeline runs | `regen.sh` completes; `main.c` constants filled | ☑ | `README.md` § Step by step, step 4 |
| M1 — CRT startup | Reaches `main()`/`WinMain()` without crashing | ☑ | `docs/BOOT_LOG.md`, 2026-10-20 — reaches `[AV] SetDisplayMode`, exits 124, no crash |
| M2 — Hardware init | D3D device + DirectSound + input created | ☑  | `README.md` § Status |
| M3 — Asset load | Reads data from `game/` without SKIP-READ | ☑ | `README.md` § Status, File I/O — all XBE sections, assets and packed archives read |
| M4 — Menus | Title screen renders | ☑ | `README.md` § Screens — title, New Game, save creation and loading, main menu and pause menu reached in maintainer runs |
| M5 — Gameplay | Playable level at stable framerate | ◐ | player's 3D model drawn on the start-game stage; stage world not drawn (only bamboo visible); no playable level. `docs/KNOWN_ISSUES.md` § The stage world is not drawn |
| M6 — Polish | Audio, save/load, rendering fixes, widescreen | ◐ | BGM works; DSP effects not emulated; create and load work in maintainer runs; widescreen not recorded |

## Non-goals

- No emulator and no CPU interpreter: the game's own x86 is translated to C and
  compiled natively (`AGENTS.md` § 1).
- No hand-written game logic, menus, fake screens or substitute renderer
  (`AGENTS.md` § 1).
- No distributed game data, disc image, XBE or generated recompiled source
  (`AGENTS.md` § 2.3–2.4).
- No hand edits to `src/game/recomp/gen/`; it is an output, like a `.o` file
  (`AGENTS.md` § 1).

## How to pick up work

Open an issue naming the milestone and the exit condition you intend to move,
then read [`AGENTS.md`](AGENTS.md) before touching anything: § 1–§ 2 give the
hard rules and the three places a fix may go, and § 5 lists the pipeline gates
that fail silently. [`docs/INDEX.md`](docs/INDEX.md) says which document covers
the area you are entering, and [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md)
lists what currently blocks M2 and M5.
