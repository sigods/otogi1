# Running

## Build

```
cmake -S . -B build-xr -G "Visual Studio 17 2022" -A x64
cmake --build build-xr --config Release --parallel 2
```

Output: `bin/otogi1.exe` (name comes from `project()` in `CMakeLists.txt`).

## Run

```
bin\otogi1.exe 2>stderr.txt
```

The game data lives where `OTOGI_DIR` points in `main.c` (default `game/`),
relative to the `.exe`.

## Useful env vars

| Variable | Effect |
|---|---|
| `RECOMP_ABI_CHECK=1` | Assert lifted signatures vs recovered ABI |
| `RECOMP_PB_EXEC=0` | Disable software NV2A push-buffer executor |
| `RECOMP_USB=1` | Enable USB OHCI + emulated pads |
| `RECOMP_PAD_LIVE=1` | Drive headless from a live pad |
| `RECOMP_FRAME_TRACE=<flagfile>` | Frame tracing (flag file, not `1`) |

## Differential debugging with xemu

Run the same scene in xemu under its GDB stub to learn what an address means.
Xemu reports Otogi as "Playable, minor issues" — a solid reference target.
