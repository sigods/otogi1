# Known issues

One entry per open blocker. Each records the symptom as it appears in a log, the evidence behind it, the current status, and where a fix belongs. Milestone status is in [`../ROADMAP.md`](../ROADMAP.md); the raw history is in [`BOOT_LOG.md`](BOOT_LOG.md).

## The stage world is not drawn

- **Symptom:** on the start-game stage the player's 3D model, the HUD and the bamboo are drawn; the rest of the stage world is not. Large flat polygons show where terrain and props should be. Reported from repeated maintainer runs and reproduced headlessly on 2026-10-26 (see `BOOT_LOG.md`).
- **Evidence:** `docs/img/start-stage.png` (CPU rasteriser) and the 2026-10-26 capture through the OpenGL back end. The 2026-10-26 census on the stage: 2.68M draws, 401M vertices, 3.7k textured fixed-function batches, and 26.9M triangles dropped behind the eye. Depth, culling and texture binding all execute (the character's own textures sample correctly), so this is not "the back end drew nothing": it never was, before the missing `glUseProgram` was found on 2026-10-26.
- **Status:** open. Both rasterisers (CPU and GL) show the same missing world from the same decoded batches, so the fault is in what the executor builds, not in either rasteriser.
- **Next lead:** the batches that should be the world are being drawn but carry geometry that collapses — the census counts 26.9M "triangles dropped behind the eye" out of ~400M vertices. Check the near-plane clip in `raster_xf_clipped` (a w that is zero or negative after the vertex program), and the register combiner path: the world's materials go through `nv2a_rc_eval` on the CPU and through a diffuse-times-texture approximation in GL, which would not hide the geometry either. The vertex-format list on the stage adds `v1:6/1` (packed 11:11:10 with size 1) and `v5:5/2` (signed short), which the fetch path handles but no healthy scene was compared against.
- **Where the fix belongs:** the toolkit's push-buffer executor (`src/kernel/nv2a_pb_exec.c`), most likely the program-batch clip, then the back end if the combiner approximation turns out to matter.

## Closed on 2026-10-26 — the input stall

- **Symptom (history):** with `RECOMP_USB=1` the title stopped flipping after 16-24 flips; log signature `irq held off by guest IRQL (depth 1, status=0x02)`.
- **Cause:** the guest's USB software queue self-links when two guest threads are inside the enqueue at once (`sub_0017302C`/`sub_00172F70`). The console has one CPU; this runtime has sixteen host threads, so that mutual exclusion has to be supplied. `taskset -c 0` makes the stall disappear, which is what identified it.
- **Fix:** `RECOMP_DISPATCH_GATE=1`, now set by default in `src/game/main.c`. See `BOOT_LOG.md`, 2026-10-26.

## Input stall recorded on 2026-10-22 (history)

- **Status:** closed on 2026-10-26 — see "Closed on 2026-10-26 — the input stall" above. The remaining notes are kept because two of them are still worth not retrying.
- **Evidence that it was a guest single-CPU race, not a queue bug:** `taskset -c 0` removes it entirely (1585 flips at 47.95 fps where the unpinned run stops at 17). The fixed answer is `RECOMP_DISPATCH_GATE=1`, now the project default.
- **Falsified, so do not retry:** a kernel-object wait, a done-queue self-link, a controller stuck in reset, ISR reentrancy, a DPC-versus-timer race, and the memory model fabricating `0xFFFFFFFF`.
- **Guest lock:** `RECOMP_GUEST_LOCK=1` (the toolkit's one-CPU lock, `src/kernel/kernel_guest_cpu.c`) did not help on its own: the lock leaves device callbacks free to run beside guest code, which is the path the USB ISR takes. `RECOMP_DISPATCH_GATE` is the one that matters for this title.

## Black screen after New Game

- **Symptom:** with a physical gamepad the pad reaches the New Game menu; after the menu pan-out delay the screen goes black. The log keeps updating, but the tail shows `[GPU] 0.00 fps (779 flips)` unchanged, `[PADS] … btn=0000 A=0` on every line, and `[KERNEL] ordinal 175 (slot 98)` climbing about 1,400 per block.
- **Evidence:** `BOOT_LOG.md`, 2026-10-24: two runs with `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1`, neither with `RECOMP_AC97_READY=1`: one plain, one with `RECOMP_GUEST_LOCK=1`. Repeated maintainer runs with `RECOMP_AC97_READY=1` reached the save screen and the later menus without the black screen.
- **Status:** open. No run without `RECOMP_AC97_READY=1` has been recorded since 2026-10-24. That switch is the only difference recorded, so it is a correlation, not an established cause. Stderr from a black-screen run has not been captured, and kernel ordinal 175 is not named yet.
- **Where the fix belongs:** unknown until stderr is captured and ordinal 175 is named.

## Saves: creation and loading work; file location not recorded

- **Symptom:** none. Creating and loading a save work in the menus, in the maintainer's runs (2026-10-25).
- **Evidence:** the save-creation screenshot `docs/img/save-screen.png`. Earlier versions of these docs said saving is not implemented; the 2026-10-25 runs supersede that.
- **Status:** open. Where the save is written, what it contains, and whether it persists across runs are not recorded.
- **Where the fix belongs:** not applicable until a failure is recorded.

## The MSVC 2022 build is untested

- **Symptom:** none recorded; the path has never been run.
- **Evidence:** [`../README.md`](../README.md) § Step by step, "MSVC 2022 — supported, untested".
- **Status:** open. A successful build and boot would close it.
- **Where the fix belongs:** the build configuration, if anything fails.

## Audio effects are not emulated

- **Symptom:** BGM plays, but the DSP effects are absent.
- **Evidence:** [`../README.md`](../README.md) § Status — "The DSP is a passthrough stub, so effects are not emulated". `BOOT_LOG.md`, 2026-10-20, records that the DSP GP/EP stub was not the cause of the earlier silence.
- **Status:** open, not started.
- **Where the fix belongs:** the toolkit's APU emulation.
