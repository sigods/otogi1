# Architecture

Three layers. Know which one owns a behaviour before you "fix" it.

| Layer | Lives in | Owns |
|---|---|---|
| Generated C | `src/game/recomp/gen/` (gitignored) | The game's own lifted x86. Never edited. |
| Runtime | `xboxrecomp/src/` | Kernel shim, D3D8 translation, DirectSound, NV2A GPU, MCPX APU, USB OHCI, XMV video. |
| Title glue | this repo: `config/`, `src/game/` | Boot constants, seeds, manual overrides, docs. |

The runtime is platform-split, and that split is where platform bugs live:

- **Windows** — Direct3D 11, DXGI and XAudio2 from the Windows SDK. No SDL2.
- **Linux** — SDL2 and libepoxy for the video, window and audio path.

Behaviour that differs between platforms almost always belongs in that split,
not in the title glue.

## Boot sequence (already wired in `main.c`)

1. Install the VEH crash handler.
2. Load the XBE.
3. `xbox_MemoryLayoutInit`
4. `xbox_kernel_init`
5. `xbox_path_init`
6. `xbox_kernel_bridge_init`
7. `g_esp = XBOX_STACK_TOP`
8. `recomp_dispatch_init()`
9. `xbe_entry_point()` — the generated function named after the entry-point VA.

## Where this project stands

Boot, hardware init, asset load and the title screen all work; gameplay does
not. [`../ROADMAP.md`](../ROADMAP.md) has the milestone status and
[`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) has the open blockers with the layer each
one belongs to. Read both before assuming a layer is broken: the input stall is
in the toolkit's USB software queue, and the missing 3D is in the push-buffer
executor.

## Typical game phases (attack one at a time)

CRT startup → hardware init (D3D/DirectSound/input) → asset loading →
menus → gameplay.

## Where a fix belongs

- Wrong lifted instruction → the toolkit's lifter, in the `xboxrecomp` fork.
- Missing kernel function → toolkit `kernel/`, or a stub here if title-specific.
- GPU MMIO access fault (`0xFD......`) → NV2A init in the runtime.
- APU MMIO access fault (`0xFE......`) → APU init in the runtime.
- One function misbehaves → `src/game/recomp_manual.c`.
- A function needs seeding → `config/seed_functions.json`.

General fixes go to the fork first and are offered upstream as separate
commits; `AGENTS.md` § 2 has the rule and `xboxrecomp/UPSTREAM.md` records the
delta.
