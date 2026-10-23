# Known issues

One entry per open blocker. Each records the symptom as it appears in a log, the
evidence behind it, the current status, and where a fix belongs. Milestone status
is in [`../ROADMAP.md`](../ROADMAP.md); the raw history is in
[`BOOT_LOG.md`](BOOT_LOG.md).

## Input is not accepted at the title screen

- **Symptom:** the title screen does not react to a pad. The log signature is
  `irq held off by guest IRQL (depth 1, status=0x02)`; the done-queue drain
  `sub_00172F70` keeps running at about 1 kHz while the foreground loop stops
  advancing.
- **Evidence:** `BOOT_LOG.md`, 2026-10-22 — "the stall is a guest queue
  self-link". Two guest threads submit the same request (`0x01121038`) into one
  software queue. `sub_0017302C` executes `[tail+0x24] = request` and, when
  `tail == request`, writes the node's own address into itself; the drain then
  never sees `head = 0`. `RECOMP_QTRACE=1` captured `+24: 00000000 -> 01121038`
  with `head=tail=01121038`.
- **Status:** open. The IRQL depth of 1 is a symptom, not the cause: the guest
  DPC that drains the corrupted queue raises DISPATCH and never lowers it.
- **Falsified, so do not retry:** a kernel-object wait, a done-queue self-link,
  a controller stuck in reset, ISR reentrancy, a DPC-versus-timer race, and the
  memory model fabricating `0xFFFFFFFF`.
- **Next lead:** the toolkit's one-CPU guest lock at
  `src/kernel/kernel_guest_cpu.c`, enabled with `RECOMP_GUEST_LOCK=1`. Under
  test, not proven — do not claim it works.
- **Where the fix belongs:** the `xboxrecomp` toolkit.

## No 3D scene is drawn

- **Symptom:** logos, the attract video and 2D UI render; no 3D content does.
- **Evidence:** `BOOT_LOG.md`, 2026-10-22 — "what fixed the black screen" and
  the census that follows it. The title's only vertex program writes constant
  texture coordinates (`oT0 = v4.wwww`), so each quad samples one texel. The
  census records 169 program batches, 0 fixed-function batches and one texture
  bound (`0x81470000`, 512x256).
- **Status:** open. Flat coloured panels are the correct output of the current
  path, not a rendering bug. Fixed-function batches and texture binding are the
  next thing to look at.
- **Where the fix belongs:** the toolkit's push-buffer executor.

## The MSVC 2022 build is untested

- **Symptom:** none recorded; the path has never been run.
- **Evidence:** [`../README.md`](../README.md) § Step by step, "MSVC 2022 —
  supported, untested".
- **Status:** open. A successful build and boot would close it.
- **Where the fix belongs:** the build configuration, if anything fails.

## Audio effects are not emulated

- **Symptom:** BGM plays, but the DSP effects are absent.
- **Evidence:** [`../README.md`](../README.md) § Status — "The DSP is a
  passthrough stub, so effects are not emulated". `BOOT_LOG.md`, 2026-10-20,
  records that the DSP GP/EP stub was not the cause of the earlier silence.
- **Status:** open, not started.
- **Where the fix belongs:** the toolkit's APU emulation.
