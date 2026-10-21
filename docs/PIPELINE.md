# Pipeline — exact commands

All Python steps run **from inside the toolkit clone** (`tools/` lives there).
`regen.sh` handles the `cd` for you. `py -3` is the Windows Python Launcher; use
`python3` on Linux/macOS.

## Step 0 — layout

- `xboxrecomp/` — the toolkit, a submodule of this repository. Builds
  libraries only. No `main()`, so it can never produce a game `.exe`.
- **this repo** — copied from `templates/new-game/`. Holds `main.c` and the
  generated C. Builds the `.exe`.

## Step 1 — extract

```
py -3 -m tools.xiso unpack "Otogi - Myth of Demons.iso" -o game/
```

Expect `game/default.xbe` alongside the game's data folders.

## Step 2 — parse the XBE (record the numbers)

```
py -3 -m tools.xbe_parser game/default.xbe --json game/otogi1_analysis.json
```

Capture: entry point VA, section map, kernel imports, XDK version.
The `--json` file is **required by Step 3** — the disassembler reads the
section layout from it. Keep it next to the XBE.

## Step 3 — disassemble

```
py -3 -m tools.disasm game/default.xbe --text-only -v
```

Output: `tools/disasm/output/{functions.json,xrefs.json}`.
Sanity check: you should see thousands of functions.

## Step 4 — identify library functions

```
py -3 -m tools.func_id game/default.xbe -v
```

Categories: CRT / RW / XDK / GAME / STUB. GAME code is your focus.

## Step 4.5 — ABI recovery (DO NOT SKIP)

```
py -3 -m tools.abi_analysis game/default.xbe -v
```

Missing `abi_functions.json` ⇒ every signature silently becomes cdecl/0-param.

## Step 4.6 — optional but recommended: Ghidra names

Run **before** Step 5; re-apply after any Step 3 re-run.

```
XBE=game/default.xbe tools/ghidra_naming/run_ghidra.sh
py -3 tools/ghidra_naming/merge_names.py --apply
```

Expect a few hundred real names (CRT/XDK helpers), not thousands.

## Step 5 — recompile

```
py -3 -m tools.recomp game/default.xbe --all --split 250 \
    --gen-dir src/game/recomp/gen
```

`--split 250` prevents OOM. `--gen-dir` is mandatory.
Takes 5–15 min. Writes `recomp_types.h`, `recomp_funcs.h`, `recomp_dispatch.c`,
`recomp_stubs.c` and the numbered chunks.

## Step 6 — build

```
cmake -S . -B build-xr -G "Visual Studio 17 2022" -A x64
cmake --build build-xr --config Release --parallel 2
```

From Git Bash, pass parallelism via CMake `--parallel`, **not** `-- /m`
(Git Bash rewrites `/m` to `M:/`).

## Step 7 — run and read stderr

```
bin\otogi1.exe 2>stderr.txt
```

It will crash. That is the start of Step 8 in AGENTS.md.
