# Known issues

One entry per open blocker. Each records the symptom as it appears in a log, the evidence behind it, the current status, and where a fix belongs. Milestone status is in [`../ROADMAP.md`](../ROADMAP.md); the raw history is in [`BOOT_LOG.md`](BOOT_LOG.md).

## The stage world is not drawn

- **Symptom:** on the start-game stage the player's 3D model is drawn; the stage world is not. The only scenery visible is bamboo. Reported from repeated maintainer runs; see the 2026-10-25 entry in `BOOT_LOG.md`.
- **Evidence:** the screenshot `docs/img/start-stage.png`. The 2026-10-22 census in `BOOT_LOG.md` (169 program batches, 0 fixed-function batches, one texture bound, `0x81470000`, 512x256) was taken before the pad reached New Game, so it does not cover the stage.
- **Status:** open. The path that draws the player's 3D model is not recorded, so do not attribute it to the push-buffer executor without checking.
- **Next lead:** fixed-function batches and texture binding, measured on the stage rather than on the title.
- **Where the fix belongs:** not known yet. The 2026-10-22 entry pointed at the toolkit's push-buffer executor.

## Input stall recorded on 2026-10-22 (history)

- **Symptom (2026-10-22):** the title did not react to a pad. Log signature `irq held off by guest IRQL (depth 1, status=0x02)`; the done-queue drain `sub_00172F70` ran at about 1 kHz while the foreground loop stopped advancing.
- **Evidence:** `BOOT_LOG.md`, 2026-10-22, "the stall is a guest queue self-link". Two guest threads submitted the same request (`0x01121038`) into one software queue; `RECOMP_QTRACE=1` captured `+24: 00000000 -> 01121038` with `head=tail=01121038`.
- **Status:** not reproduced in the maintainer's runs since 2026-10-24, when a physical pad with `RECOMP_USB=1` reached New Game. No record names the change that cleared it. The toolkit submodule was pinned to `d795f01` by commit `13a0142` (2026-10-22) and is at `adfc7e6` at 79df37e.
- **Falsified, so do not retry:** a kernel-object wait, a done-queue self-link, a controller stuck in reset, ISR reentrancy, a DPC-versus-timer race, and the memory model fabricating `0xFFFFFFFF`.
- **Guest lock:** `RECOMP_GUEST_LOCK=1` (the toolkit's one-CPU lock, `src/kernel/kernel_guest_cpu.c`) made no difference to the black screen on 2026-10-24. It is not needed for the menus. Do not claim it works.
- **Where the fix belongs:** the `xboxrecomp` toolkit, once the cause is confirmed.

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
