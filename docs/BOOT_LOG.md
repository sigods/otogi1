# Boot Log — append-only findings ledger

Rules: append only. One entry per finding. Always include the exact stderr line,
the RVA, and what you changed. Never rewrite history — add a correcting entry.

Template:

```
## YYYY-MM-DD — <short symptom>
- Phase: CRT / hardware init / assets / menus / gameplay
- Symptom: <exact stderr line(s)>
- RVA / target: 0x........
- Diagnosis: ICALL miss | bad memory access | unimplemented kernel fn | bad vtable | other
- Fix: file + change (or "upstream: xboxrecomp issue #N")
- Result: <new state; next blocker>
```

---

## 2026-10-19 — project scaffold created

- Phase: pre-boot
- Status: scaffold only. No XBE processed yet.
- Next: run `./regen.sh`, capture Step 2 constants, fill `src/game/main.c`.

## 2026-10-19 — first boot past entry point; SIGSEGV at call #1560

- Phase: startup / CRT bring-up
- Symptom: `[otogi1] entry point: 0x000E3EC7` and `[otogi1] starting game`; process then `exit 139` (SIGSEGV). stderr captured to `/tmp/otogi1-stderr.txt` (503 lines).
- RVA / target: `0x00180100`
- Diagnosis: post-entry SIGSEGV; unresolved ICALL VA `0x00180100` (1560 ICALL dispatches total). Guest asset opens failing `ERROR_PATH_NOT_FOUND` for `d:\media\discerr.dds`, `d:\font\fe_font.ccm`, `d:\sound\effect\dspinfo.xdi`, `d:\sound\system\system.xss`.
- Fix: none yet (first boot; crash after entry expected).
- Result: binary exists and executes past `xbe_entry_point()`; loop criterion PASS; next blocker is the post-entry fault / asset path mapping.

## 2026-10-19 — wait crash fixed; boot reaches display mode, then GPU/fence stall

- Phase: hardware init / D3D bring-up
- Symptom: before the fix, SIGSEGV in `wait_single` at host `0x10668C+8`, reached from `bridge_KeWaitForSingleObject`; guest KEVENT VA `0x0010668C` (D3D device `0x00104CF0` + `0x199C`) was passed to the host wait unresolved.
- RVA / target: `0x0010668C` (KEVENT header); wait PC `0x000F7560`; fence PUT `device+0x2C`, GET pointer `device+0x30`.
- Diagnosis: bad memory access — an untagged guest VA was treated as a `w32_object*`. The title-built notification KEVENT and its fence completion were not mirrored, and the vector-3 D3D ISR that signals it is not delivered.
- Fix: toolkit — `ke_resolve_sync_object` resolves title KEVENT / shadow / tagged / pseudo-handle only and rejects guest VAs (`STATUS_INVALID_HANDLE` for waits, no-op for setters); added `xbox_KeSetGuestEvent`; added `xbox_Nv2aMirrorFenceEvent` so the fence mirror signals an embedded KEVENT when PUT changes. Title: `xbox_EnableTitleEvents(1)` plus `xbox_Nv2aMirrorFenceEvent(0x00104CE8, 0x2C, 0x30, 0x199C)` before boot, with `RECOMP_TITLE_KEVENTS` still honored.
- Result: 12 s run exits 124 (timeout, no crash), stderr 66,587 bytes. `NV2A fence mirror: signals KEVENT at device+0x199C`; `[KERNEL] event 0x0010668C built by the title: host event created (notification)`; boot then advances to `[AV] SetDisplayMode mode=0x080F0D12 format=0x00000012 pitch=2560 fb=0x00404000` and opens `d:\movie\otogi_us_promo.xmv`. One pre-creation mirror tick logs `xbox_KeSetGuestEvent: unresolved guest object 0x0010668C` (event header did not exist yet) and is harmless.
- Next blocker: a later wait on the same event still blocks in `sub_000F7560` → `bridge_KeWaitForSingleObject` → `wait_single`; guest SignalState `0x0010668C+4 = 0`; PUT == GET == `0x17E5` (`device+0x2C`, `[0x80002000]`). The fence no longer advances, so this is NV2A push-buffer/vector-3 interrupt progress, not handle resolution.

## 2026-10-19 — title KEVENT wakes; boot reaches SetDisplayMode, then XMV demux spin

- Phase: hardware init / movie bring-up
- Symptom: with the event signal tied to PUT changes, `sub_000F7560` still blocked at PUT == GET == `0x17E5` with `0x0010668C+4 = 0`; after the idle re-signal, a 12 s run burns ~9.3M kernel calls (ordinals 160/161 `KfRaise/LowerIrql`, 277/294 `RtlEnter/LeaveCriticalSection`) right after opening `d:\movie\otogi_us_promo.xmv` and one 155648-byte read.
- RVA / target: fence `device+0x2C` / `+0x30`; event `device+0x199C`; XMV demux `0x001461C8`; read callback `0x001458D4`; read dispatch `0x000E1731`; read wrapper `0x000E1D6A`.
- Diagnosis: the mirror signalled only on a PUT change, but the title clears the event and then waits after the last change, so the signal was consumed and never repeated. For XMV, the packet needs `0x28000` bytes (`ctx+0x50`) while the read asks for `0x26000` (`ctx+0x8C`); `ctx+0x74` stays 0 and the demux retries.
- Fix: `/tmp/xboxrecomp/src/kernel/xbox_memory_layout.c` — when GET == PUT and the embedded event's guest SignalState is 0, re-signal it (idle completion in the synchronous runtime).
- Result: the wait wakes; boot advances to `[AV] SetDisplayMode mode=0x080F0D12 format=0x00000012 pitch=2560 fb=0x00404000`, then opens the promo XMV and spins. Removing the movie does not help — the missing-file path retries the same loop.
- Next blocker: the XMV demux expects asynchronous read completion; the bridge completes inline, so the caller never observes the in-flight/short-read transition.

## 2026-10-20 — checkpoint: proto-only, headless main loop; switching to retail

- Verified all work to date used the **prototype** disc: `game/default.xbe` MD5 `2db14237f3f88d55d147e031b604483a` matches the proto hash in `TITLE_OTOGI.md`. Retail has never been tested.
- Working: builds on Linux x86-64; boots through kernel/AV init; loads 111 asset files; with `RECOMP_NOMOVIE=1` the movie is skipped and the title's own loop runs headless at ~540 `sub_000D03A0` calls/s (25 s run, no crash).
- Not working: no visible rendering (out of scope per user); after the skip the title idles with no further asset loads; guest pad cache stays `0xFFFFFFFF` and pad polling is only seen during boot (`recomp_0001.c:35518`, 6 calls).
- Temporary diagnostics live in `gen/` only: `[BOOT]` (`loc_000D0646`), `[FRAME]` (`loc_000D03A0`), `RECOMP_NOMOVIE`, movie retry cap `0x7FFFFFFF`, bypass after init. Regenerated, do not preserve.
- Toolkit delta: `~/otogi-checkpoint/xboxrecomp-linux.patch` (base `6376eb8`), plus full tree in `~/otogi-checkpoint/otogi1-checkpoint.tar.gz`.
- Next: regenerate against the **retail** XBE — all seed addresses and `game/otogi1_analysis.json` are proto-derived — then re-run the same boot harness before further state-machine work.

## 2026-10-20 — retail: guest D3D runs and submits continuously

- Retail `default.xbe` (base `0x00010000`) carries its own D3D/D3DX as guest sections — `D3D VA=0x000F3EE0 vsize=80100 (BSS)`, `D3DX VA=0x001077E0 vsize=115976 (BSS)` — so no host D3D8 shim is needed; the hot `0x0F4xxx–0x0FCxxx` code is the title's own D3D driver.
- Fence-mirror event offset corrected to `device+0x199C` (`0x00106B6C`) in `src/game/main.c:315,425`; the title no longer waits forever after `AV_SetDisplayMode`.
- `RECOMP_WAIT_LOG=1` run: zero `wait poll` / `long wait` lines during the steady state, and `RtlEnter/LeaveCriticalSection` counts are identical (6697) across 15/20/25 s runs — no kernel activity at all after the stall point, so the loop is entirely guest-side.
- `perf` on the live process: broad spread of guest functions in the D3D range (not a spin); `strace -c -f` over 6 s: 264k `clock_gettime`, 66k `sched_yield`, 591 `clock_nanosleep` (guest `GetTickCount`/`SwitchToThread` polling, helper threads sleeping properly).
- Live `/proc/pid/mem` samples (`g_xbox_mem_offset = 0x100000000`): `PUT(device+0x2C)` advances linearly `0x50BB6 → 0x1A7E33` over 55 s, consumer index at `0x80002000` tracks it (`GET = PUT+4`), free space stays `0x3FFE00` — the title submits ~450 KB/s of DMA with no backpressure and no deadlock.
- Guest state advances (356 bytes changed in the device block over 3 s). Remaining gap is command decoding/rendering, which stays out of scope per the user.
- New diagnostics: `RECOMP_DPC_LOG` (null/failed `KeInsertQueueDpc`) in `src/kernel/kernel_sync.c`; DPC path is never reached.

## 2026-10-20 — release readiness: v0.1 technical preview

- Phase: documentation / packaging
- Status: documented as v0.1 technical preview. Boot, kernel, file I/O, and
  thread paths are working natively; input and audio are experimental;
  rendering is not implemented (guest GPU command stream not decoded, no
  picture).
- Verification: `cmake --build build-linux -j8` succeeds (ninja: no work to
  do). `timeout 25 ./build-linux/bin/otogi1` from the project root reaches
  `[AV] SetDisplayMode mode=0x080F0D12 format=0x00000012 pitch=2560
  fb=0x00404000` and exits 124 on the timeout, with no crash. Running the same
  binary from another directory fails at `Cannot open XBE: game/default.xbe`.
- Packaging: 14 files are tracked. `game/`, `*.xbe`, `*.iso`, and
  `src/game/recomp/gen/` are gitignored; no game data or generated C is
  shipped. The repository has no top-level `LICENSE` or `NOTICE`; the
  `xboxrecomp` checkout provides them.
- Blocker: GPU command decoding remains the gating item for any visual build.

## 2026-10-20 — release audit: audio fixed, packaging completed

- Phase: verification / packaging
- Audio root cause: the title's streamed-BGM loop only advances when
  `NtReadFile` completes with `STATUS_PENDING`, but the bridge completed reads
  inline with `STATUS_SUCCESS`. The guest therefore read the 2048-byte
  `d:\sound\stream\menu_bgm.dat` header and never submitted PCM. Fixed by
  defaulting async completion in `src/kernel/kernel_bridge.c`
  (`RECOMP_SYNC_IO=1` restores inline completion). The DSP GP/EP stub was not
  the cause.
- Audio verification: `RECOMP_AC97_READY=1 RECOMP_AUDIO_WAV=...` over 13 s gives
  625,664 frames, peak 32,014, RMS 4,948, 69.3% non-zero samples. Before the
  fix the same capture was all zero. A/B check: `RECOMP_SYNC_IO=1` returns to
  silence.
- Boot verification: `timeout 15 ./build-linux/bin/otogi1` reaches
  `[AV] SetDisplayMode` and the title-built KEVENT line, exits 124, no crash.
- Rendering: the push-buffer executor runs behind `RECOMP_PB_EXEC` and clears
  the real surfaces, but the title's vertex program writes `w = inf`
  (`out0 pos 64.5312 208.531 0 inf`), so `raster_xf_clipped` rejects every
  triangle and the frame stays black. Root cause recorded; not fixed in v0.1.
- Input: `xbox_OhciInit()` had no caller, so the OHCI trap was never armed and
  the guest read plain zeroed RAM at `0xFED00000` and concluded there was no
  controller. It is now called at the end of `xbox_kernel_bridge_init()`, after
  the guest memory is mapped, still opt-in behind `RECOMP_USB`. With the flag
  the controller reports `operational after 20 ms`, the pad enumerates and
  input reports are served (20 s run: 9 periodic transfers, 9 reports, 29
  control setups, 39 control completions). The title does not yet poll the pad
  — its state block stays `0xFFFFFFFF` — so controls are still not usable in
  v0.1, but the device path is now live instead of absent.

- Input, remaining gate (same day, timeboxed): with the USB model live the
  device enumerates and reports are served, but the title still never reads
  the pad. The gate is its own controller object pointer at `0x1120EDC`: the
  menu input code at `recomp_0014.c` does `if (MEM8(0x1120ED8) & 0x10) ... else
  if (MEM32(0x1120EDC) != 0) call sub_000301A0` (the pad-state reader), and at
  runtime both `0x1120ED8 = 0x0000` and `0x1120EDC = 0x00000000`, so the read
  is skipped every frame. The state block `0x1120F6C..0x1120F94` is cleared to
  `0xFFFFFFFF` by `sub_0002FB90` (1520 calls in 15 s) and never filled. No
  generated code assigns `0x1120EDC` directly, so the object is registered by
  the title's XAPI when it opens the device — and that open is not completing
  even though the USB stack has enumerated the pad. That is the next thread to
  pull; it is not a one-line fix.
- Packaging: the runtime changes are committed on the toolkit branch
  `otogi1-linux-v0.1` and shipped as `xboxrecomp-linux.patch` (applies cleanly
  to upstream `v0.15.0`). `CMakeLists.txt` and `.gitignore` are now tracked;
  run artifacts (`*.log`, `*.wav`, captures, debugger dumps) are ignored.


## 2026-10-21 — Win32 cross-build

- The mingw-w64 cross-build now compiles and links:
  `cmake -B build-win -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
  -DXBOXRECOMP_DIR=../xboxrecomp` → `build-win/bin/otogi1.exe`, 18,479,962 bytes.
- Two portability gaps were in the way. `mmio_decode_x86_64.h` only knew the
  POSIX `ucontext_t`; it now takes a Win32 `CONTEXT` as well (same register
  indices, `Rip`/`Rsp`/`Rax..R15` via the CONTEXT fields), and the OHCI and APU
  VEH call sites use that decoder instead of the older `mmio_emulate` copy.
  `dlfcn`/`dladdr` in `kernel_bridge.c` and the read(2) silent-failure dump in
  `xbox_memory_layout.c` are now behind `#ifndef _WIN32`.
- Under wine the exe reaches the same milestone as Linux: XBE loads, memory
  layout maps, fence mirror registers, `[AV] SetDisplayMode` appears, kernel
  ordinals resolve, no crash (stopped by the timeout).
- This is not part of the v0.1 Linux release; it just means the tree no longer
  fails to build when `WIN32` is defined.

## 2026-10-21 — rendering: the zero-w screen-space convention

- Every vertex of the title's 2D quads carries `w = 0` (`91.5 85.5 0 0`,
  `603.5 85.5 0 0`, ...), and its own vertex program inverts that component
  (`rcp`) to build the homogeneous position. A zero makes the reciprocal
  infinite, so `raster_xf_clipped`'s non-finite guard rejected every triangle:
  the pushbuffer advanced, the clears wrote the surface, and the screen stayed
  black.
- The same title stores a real `w` on its 3D vertices, so the zero is a
  property of the 2D path rather than data the program can consume.
  `fetch_attr` now supplies `1.0` for attribute 0 when a float4 position
  arrives with a zero fourth component; other attributes are passed through
  untouched.
- With the fix a 12-instruction program runs 195 batches / 780 vertices per
  capture with no triangles dropped. What reaches the screen is still fades
  and solid panels (3–5 distinct colours), because the only vertex program the
  title uploads writes constant texture coordinates (`oT0 = v4.wwww`), so no
  textured content is drawn through the program path. Fixed-function batches
  and texture binding are the next thing to look at, not the interpreter.
