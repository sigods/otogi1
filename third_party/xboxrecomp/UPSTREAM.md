# Vendored xboxrecomp

This directory is a source snapshot of the MIT-licensed
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) runtime toolkit. It is
checked in so that a clone of this repository builds without fetching,
patching, or version-matching a separate checkout.

| | |
|---|---|
| Upstream base | `v0.15.0` (`c98a641ae09815d3cce48065223f3df2d6f8584f`) |
| Snapshot commit | `218547dfab81d38fac19761e16758fc0e35e3d03` ("otogi1-linux-v0.1") |
| License | MIT — see `LICENSE` in this directory |

## Local changes against `v0.15.0`

18 files, 1,687 insertions, 118 deletions. The ones that matter:

- `src/platform/mmio_decode_x86_64.h` — new. Decodes trapped x86-64 MMIO
  instructions on Linux (ucontext) and Windows (VEH `CONTEXT`). Without it the
  APU/AC'97 and OHCI apertures fault on x86-64.
- `src/apu/apu_mmio_hook.c`, `src/usb/ohci.c` — call the decoder on both hosts;
  the OHCI entry point previously referenced a function that did not exist.
- `src/kernel/kernel_bridge.c` — `bridge_async_io_enabled()` defaults to 1, so
  `NtReadFile` returns `STATUS_PENDING` and completes on the work queue; the
  timer-driven path is unchanged. Also calls `xbox_OhciInit()` once guest
  memory is mapped, so the USB aperture is trapped.
- `src/kernel/nv2a_pb_exec.c` — `fetch_attr()` resolves vertex attributes
  through `dma_resolve()`; position attribute `w == 0` is read as 1.0 for
  screen-space quads.
- `src/input/*`, `src/usb/usb_gamepad.c` — keyboard-to-pad bridge and synthetic
  pad for headless testing.

## Updating

There is no submodule and no patch file. To move to a newer upstream:

1. Check out the new upstream tag in a scratch clone.
2. Diff it against `v0.15.0` and re-apply the local changes above.
3. Replace this directory with the new tree, keeping this file's table current.

To hand a change back upstream, diff this directory against a pristine
checkout of the base commit:

    git diff --no-index /path/to/pristine-v0.15.0 ./third_party/xboxrecomp
