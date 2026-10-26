# Pipeline — exact commands

All Python steps run **from inside the toolkit clone**, because `tools/` lives
there. Every path below is relative to `xboxrecomp/`: the disc extracts to
`../game`, and the generated C goes to `../src/game/recomp/gen`.

`regen.sh` at the repository root performs that `cd` for you and runs `xbe_parser`, `disasm`, `func_id`, `abi_analysis` and `recomp`. Commands use `python3`; on Windows, `py -3` (the Python Launcher) is
equivalent.

## Step 0 — layout

- `xboxrecomp/` — the toolkit, a submodule of this repository. Builds
  libraries only. No `main()`, so it can never produce a game `.exe`.
- **this repo** — holds `main.c` and the generated C. Builds the `.exe`.

## Step 1 — extract

$ cd xboxrecomp
$ python3 -m tools.xiso unpack "/path/to/Otogi - Myth of Demons (USA).iso" -o ../game


Expect `../game/default.xbe` alongside `chr/`, `map/`, `sfx/`, `movie/` and the
rest — 1,121 files, about 2.2 GB.

Otogi's USA image carries a video partition in front of the filesystem, so a
reader that assumes the filesystem starts at offset 0 fails. `tools.xiso`
probes the known partition bases.

## Step 2 — parse the XBE (record the numbers)

$ python3 -m tools.xbe_parser ../game/default.xbe --json ../game/otogi1_analysis.json


Capture: entry point VA, section map, kernel imports, XDK version.
The `--json` file is **required by Step 3** — the disassembler reads the
section layout from it. Keep it next to the XBE.

## Step 3 — disassemble

$ python3 -m tools.disasm ../game/default.xbe --text-only -v \
--seed-functions ../config/seed_functions.json \
--extra-sections D3D,D3DX,DSOUND,XGRPH,XMV,XPP


Output: `tools/disasm/output/{functions.json,xrefs.json}`.
Sanity check: you should see thousands of functions.

## Step 4 — identify library functions

$ python3 -m tools.func_id ../game/default.xbe -v


Categories: CRT / RW / XDK / GAME / STUB. GAME code is your focus.

## Step 4.5 — ABI recovery (DO NOT SKIP)

$ python3 -m tools.abi_analysis ../game/default.xbe -v


Missing `abi_functions.json` ⇒ every signature silently becomes cdecl/0-param.

## Step 4.6 — optional but recommended: Ghidra names

Run **before** Step 5; re-apply after any Step 3 re-run.

$ XBE=../game/default.xbe tools/ghidra_naming/run_ghidra.sh
$ python3 tools/ghidra_naming/merge_names.py --apply


Expect a few hundred real names (CRT/XDK helpers), not thousands.

## Step 5 — recompile

$ python3 -m tools.recomp ../game/default.xbe --all --split 250 \
--gen-dir ../src/game/recomp/gen \
--exclude-manual ../src/game/recomp_manual.c


`--split 250` prevents OOM. `--gen-dir` is mandatory **and must be
`../src/game/recomp/gen`** — without the `../` it lands inside the toolkit
clone, where nothing compiles it.

Takes 5–15 min. Writes `recomp_types.h`, `recomp_funcs.h`, `recomp_dispatch.c`,
`recomp_stubs.c` and the numbered chunks.

## Step 6 — build (back at the repository root)

$ cd ..
$ cmake -S . -B build-linux -G Ninja
$ cmake --build build-linux -j8


Output: `build-linux/bin/otogi1`, about 19 MB.

On Windows, substitute the MinGW cross-build or the MSVC generator — both are
in [`RUNNING.md`](RUNNING.md). From Git Bash, pass parallelism via CMake
`--parallel`, **not** `-- /m` (Git Bash rewrites `/m` to `M:/`).

## Step 7 — run and read stderr

$ ./build-linux/bin/otogi1 2>stderr.txt


Run from the project root: the binary opens `game/default.xbe` relative to the
current directory.

In the current state it does not crash. It reaches `[AV] SetDisplayMode`, runs
the title's main loop, and continues until killed. `tools/smoke.sh` asserts
that, along with no early exit, no segmentation fault and non-silent audio.

Work on a blocker starts from [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md); the
debugging loop is `AGENTS.md` § 6.
