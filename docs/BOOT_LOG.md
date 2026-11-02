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

## 2026-10-21 — USB: the boot gate is a descriptor retired twice

- The title stops progressing with the main thread pinned in `sub_00172BD2`,
  a copy helper the drain loop `sub_00172F70` calls once per retired TD. It is
  not a wait: `RECOMP_GATE_TRACE` logged 1944 gate lines, 1943 of them our own
  fence event being signalled and exactly one wait, so nothing blocks.
- At the fatal iteration the helper is entered with `eax=0xFED00000` (the OHCI
  aperture), `ecx=0`, `edx=0`, `esi=0`, `edi=0x01121038`. A zero count makes
  the copy underflow to 2^32 iterations, each touching the aperture, which is
  why the PC never moves and the title looks hung.
- The count is **passed in, not computed there**. On the first entry of the
  same helper the arguments are live: `eax=01121038 ecx=80000724
  edx=800014C0 esi=80000724`, the last being an ED pointer with the halted bit
  set. Call chain: `0x1700EB -> 0x170738 -> 0x173068 -> 0x17302C ->
  0x172F70 (drain) -> 0x172BD2 (retire)`.
- The input model itself is correct: 87 descriptors complete with 22 setups,
  and the pad pipe (`ED 00001490`, `info=00205081`, device address 1) is armed
  with `cbp=0x25B be=0x26E len=20` and receives a full 20-byte report
  (`report pad 0 gave 20 bytes, the descriptor allows 20`). Reports stop at
  7-9 only because the driver stops re-arming the pipe after the bad
  retirement (`head=tail=0`).
- The last events before the spin: the report TD is retired
  (`done 00001E70 cc=0 cbp=0 be=026E info=011406C1`), two controller registers
  are read (`+0x10`, `+0x0C`), the ISR claims the interrupt, and the title
  returns through `cli`/`sti` at `0x00171BA5`/`0x00171BB6`, which the
  recompiler compiles to no-ops.
- Falsified on the way, so nobody repeats them:
  - a kernel object wait — see the gate trace above;
  - a done-queue self-link — the guard in `ohci_walk_eds` fired 0 times;
  - a controller left in reset — `ctl=0x000000BE` is `HCFS=10`
    (UsbOperational) with `PLE|CLE|BLE` set, and `cmd=0x00000006` is
    `CLF|BLF`, both normal; `HCR` self-clears correctly;
  - ISR reentrancy through the no-op `cli`/`sti` — suppressing the interrupt
    (`RECOMP_OHCI_NO_IRQ`) did not remove the spin, it starved the pipe
    (`reports: 0`) and produced an unrelated dereference of the `0x7FFFFFF8`
    sentinel.
- Conclusion: a TD is reaching the drain loop a second time with its fields
  already cleared. `ohci_walk_eds` writes the done-queue link into `td+8`
  (`NextTD`) before the driver has finished traversing the ED chain, so a
  descriptor can be visible on both lists. The next look is the walk order and
  that field, not the guest.
- Diagnostics left in place, all opt-in and silent by default:
  `RECOMP_GATE_TRACE` (waits and signals), `RECOMP_USB_TDLOG` (descriptors as
  armed, as completed, and the report length actually copied), and
  `RECOMP_FAULT_ABORT` (abort on 1000 unclaimed faults instead of retrying
  forever).
- Consequence for v0.1: boot, BGM and UI are solid; controls and 3D stay
  blocked behind this, because both wait on the same pipe.

## 2026-10-21 — USB: corrected, the gate is a null controller context

- The entry above is withdrawn. The descriptor is not retired twice: the OHCI
  model matches the QEMU reference over all 87 completions, the 20-byte pad
  report arrives in full, CBP clears to zero, and data-toggle handling
  matches. A second retirement would need a second descriptor; none exists.
- At the fatal iteration the context is null, not duplicated. `sub_00172BD2`
  is entered with `ecx=0`, `edx=0`, `esi=0`, `eax=0xFED00000`,
  `edi=0x01121038`, `ebp=0x01D0EDBC` and `MEM32(ebp-40)=8`; on the first
  entry the same arguments are live (`ecx=80000724`, `edx=800014C0`).
- `sub_00172F70` reads the done-queue head at `esi+0x41C` and finds zero,
  through a null base: `esi` is the controller pointer taken from `ecx`. The
  length derived from it is zero and wraps into a 2^32-iteration copy over
  the OHCI aperture.
- Chain: `0x1700EB -> 0x170738 -> 0x173068 -> 0x17302C -> 0x172F70 ->
  0x172BD2`. No stage tests the controller for null before adding `0x41C`;
  the only guards are the queue-empty test, made through the bad pointer, and
  a byte comparison that does not involve the base.
- Remaining lead: a null controller-instance pointer in the guest ISR/drain
  path. The host enters the guest interrupt routine with the register file
  cleared (`ohci_call_isr`, `irq_run_line`), consistent with the all-zero
  context seen. Next check: the register context at that entrance and at the
  first call into the drain.

## 2026-10-22 — USB: what is actually established

- The descriptor model matches the QEMU reference: 87 completions, a full
  20-byte pad report, CBP cleared to zero on a completed transfer, and the data
  toggle now advanced per the reference.
- The chain `sub_001700EB -> sub_00170738 -> sub_00173068 -> sub_0017302C ->
  sub_00172F70 -> sub_00172BD2` was instrumented live: 335, 333, 20,504
  sampled entries respectively, all carrying a live controller pointer
  `ecx = 0x80000724`, with a sane `ebp`. No caller passes a null controller.
- The all-zero register state seen at earlier stops is spill state observed
  after a fault, not a null pointer. The "null controller context" explanation
  in the preceding entry is withdrawn.
- Preserving guest registers across guest-ISR entry changed nothing (pad
  reports 5 to 7, same spin); that experiment has been reverted.
- `HcRevision` (OHCI offset 0) is already owned and decoded, and the x86-64
  decoder already handles `C7 /0`; the reports that this write was unclaimed do
  not hold.
- The freeze reproduces in 18 s, pad reports stop at ~7, and `cli`/`sti` pairs
  repeat.
- UNRESOLVED CONTRADICTION: the fatal instruction was attributed to
  `sub_00172BD2+11676` with `rax = mem_offset + 0xFED00000`, but the same
  instruction also disassembles as a store through `ESP`
  (`mem_offset + (g_esp + 8)`), which would only alias `HcRevision` if `g_esp`
  were `0xFECFFFF8`, while the recorded `g_esp` was `0x01D0ED88`. Which reading
  is correct is open.
- Next step to try: re-read the faulting instruction with `x/gxi` at the stop
  and dump `g_esp` and `rax` in the same sample, so the two readings can be
  reconciled.

## 2026-10-22 — USB: contradiction resolved, the fault is a normal trap

- Direct measurement at the SIGSEGV stop settles the reading: `pc =
  sub_00172BD2+11676`, instruction `movl $0x2,(%rax)`, with
  `rax - g_xbox_mem_offset = 0xFED00008` and `g_esp + 8 = 0x01D0ED90`. The
  earlier "store through ESP" interpretation is withdrawn; this is a genuine
  OHCI register store.
- Offset `+8` is `HcCommandStatus`, and the stored value `2` is `CLF`
  (ControlListFilled). The model already claims this register at
  `src/usb/ohci.c:255`, consumes CLF at `:1273`, and walks the list on the tick
  rather than only on CLF (`:1228-1232`), so the write is a normal, handled
  access.
- gdb stops on SIGSEGV before the handler emulates the access, so those stops
  were the ordinary trap-and-emulate path, not failures.
- 20,504 entries into `sub_00172BD2` over 20 s is about 1,025 per second, a
  1 kHz poll loop rather than a hang; the pipe goes quiet after about 7 pad
  reports.
- New lead: the title appears to be waiting for something the host never
  supplies, plausibly a pad press. Next experiment: drive `RECOMP_AUTOPRESS`
  and see whether the state advances.

## 2026-10-22 — rendering: what fixed the black screen

- Phase: menus / title screen
- Symptom: the pushbuffer advanced and the clears wrote the surface, but the
  screen stayed black; every triangle was rejected before rasterisation.
- Cause: every vertex of the title's 2D quads carried `w = 0`. The title's own
  vertex program takes the reciprocal of that component to build the
  homogeneous position, so a zero produced an infinite value and the
  rasteriser's non-finite guard rejected every triangle.
- Fix: the toolkit's `fetch_attr` path now supplies `1.0` for vertex attribute
  0 when a float4 position arrives with a zero fourth component, and resolves
  attributes through `dma_resolve` so tiled and physical addresses are handled.
- Other attributes are passed through untouched.
- Result: a 12-instruction program runs 195 batches / 780 vertices per capture
  with no triangles dropped.
- Later census: 169 program batches / 676 vertices, 0 fixed-function batches,
  0 textured-without-uv, and one texture bound (`0x81470000`, 512x256, format
  `0x06`).
- The title's only vertex program writes constant texture coordinates, so each
  quad samples a single texel. Flat coloured panels are the correct output, not
  a rendering bug.

## 2026-10-22 — current state

- Works: the game boots the retail XBE, renders the logos, and reaches the
  title screen. The title screen loops the attract video and plays BGM.
- Works: 2D UI rendering. The pushbuffer is decoded and the UI quads rasterise.
- Works: audio through `RECOMP_AC97_READY=1`, with measured non-zero PCM.
- Works: native Linux builds and the MinGW cross-build for Windows; the MSVC
  path is documented. `tools/smoke.sh build-linux` passes.
- Does not work: no 3D scene is rendered.
- Does not work: input is not accepted. Injected pad presses reach the guest —
  USB descriptor completions rise from about 107 to about 115, and pad reports
  from about 7 to about 9 — but the title screen does not react.
- The game does not progress past the title screen.
- Video playback is the title's own XMV decoder. The toolkit's host video
  player is a POSIX stub and is not required for the attract video to play.

## 2026-10-22 — open lead: guest IRQL depth stays at 1

- Status: open investigation, not solved.
- Log signature: `irq held off by guest IRQL (depth 1, status=0x02)`
- Log signature: `guest IRQL never dropped (depth 1); delivering anyway`
- The guest's IRQL depth stays at 1 and is never lowered.
- The done-queue drain keeps running at about 1 kHz.
- The foreground loop stops advancing.
- USB enumeration itself completes normally.
- Current investigation: why the guest never lowers IRQL, and whether that is
  what stalls the foreground loop rather than a USB or pad-handling fault.

## 2026-10-22 — USB: the stall is a guest queue self-link

- Phase: menus / title screen
- Symptom: input is not accepted; log signature `irq held off by guest IRQL
  (depth 1, status=0x02)`; the drain `sub_00172F70` runs at about 1 kHz and
  the foreground loop stops advancing.
- Cause: two guest threads submit the same request (`0x01121038`) into one
  software queue. `sub_00173068` (`src/game/recomp/gen/recomp_0024.c:31861`)
  clears `request+0x24` (`:31898`), then `sub_0017302C` (`:31813`) takes its
  non-empty branch off the tail word at `device+0x420` and executes
  `[tail+0x24] = request` (`:31837-31838`). When `tail == request` that write
  stores the node's own address, linking it to itself.
- The drain `sub_00172F70` (`:31646`) then never terminates: `head` never
  reaches 0.
- Evidence: `RECOMP_QTRACE=1` captured the transition `+24: 00000000 ->
  01121038` with `head=tail=01121038`. A gdb breakpoint on `sub_00173068`
  caught thread 3 entering while the node was already the whole queue
  (`[SUB] n=1 thr=3 head=01121038 tail=01121038`); an earlier stack had
  thread 1 in the same submit chain
  (`sub_00170738 -> 00173068 -> 0017302C`).
- Supersedes the earlier entry `## 2026-10-21 — USB: the boot gate is a
  descriptor retired twice`, which is retained as history but is wrong; its
  follow-up reading (a null controller context) was withdrawn in the
  2026-10-22 entries above.
- Falsified along the way, so they are not retried:
  - a kernel-object wait;
  - a done-queue self-link;
  - a controller stuck in reset;
  - ISR reentrancy;
  - a DPC-vs-timer race — serialising the DPC body and the timer routine
    left the pin unchanged;
  - the memory model fabricating `0xFFFFFFFF` — it zero-fills decommitted
    pages and faults on unmapped ones, and `MEM32` is a direct load, never a
    sentinel.
- The IRQL depth of 1 is a symptom, not the cause: the guest DPC that drains
  the corrupted queue raises DISPATCH and never returns to lower it. The
  count arithmetic itself is correct.
- Likely fix: the toolkit's existing one-CPU guest lock at
  `src/kernel/kernel_guest_cpu.c`, enabled with `RECOMP_GUEST_LOCK=1`; its
  header describes exactly this bug class. Status: under test, not yet
  proven — do not claim it works.

## 2026-10-24 — physical pad reaches New Game; black screen after

- Build: commit `2c823a0`, `build-linux`.
- Command: `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 ./build-linux/bin/otogi1`
- `RECOMP_GUEST_LOCK` was **not** set.
- Input: a physical gamepad was used. The pad got past the title and reached the New Game menu.
- Symptom: selecting New Game shows a black screen.
- Status: one run. stderr not yet captured, so the cause is unknown.
- This does not match the 2026-10-22 "current state" entry, which says the title screen does not react. Both are kept; this one is the newer observation.
- The guest lock was not needed for this run. One run does not show it is never needed, so the 2026-10-22 lead stays "under test".

## 2026-10-24 — guest lock retest: no change; black screen after New Game

- Build: commit `2c823a0`, `build-linux`.
- Command: `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 RECOMP_GUEST_LOCK=1 ./build-linux/bin/otogi1`, physical gamepad.
- Result: same as the run without the lock. Pad reaches New Game; the screen goes black after the menu pan-out delay.
- Conclusion: `RECOMP_GUEST_LOCK=1` does not fix this. The 2026-10-22 lock lead is not the cause of the post-menu black screen.
- Log tail (stderr, same run shape):
  - `[GPU] 0.00 fps (779 flips)` repeats unchanged; the flip count stops at 779.
  - `draws 8695`, `0 pixels written` and `[PB] 0 pushbuffer calls followed` do not change between blocks.
  - `[PADS] … btn=0000 A=0` on every line: the pad is served, but no button bits reach the guest in the tail.
  - `[KERNEL] ordinal 175 (slot 98) esp=0x0154FF88` dominates and climbs about 1,400 per block, consistent with a polling loop. Once, ordinal 119 appears at `esp=0x0158FFCC` and then the loop resumes.
  - The run does not exit and does not crash.
- Not yet known: the name of ordinal 175, and whether `[PB] 0 pushbuffer calls followed` is per-run or per-frame.

## 2026-10-25 — save-creation screen reached with RECOMP_AC97_READY=1

- Build: commit `da71693`, `build-linux`.
- Command: `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 RECOMP_AC97_READY=1 ./build-linux/bin/otogi1`, physical gamepad.
- Result: the run passed New Game and reached the save-creation screen. The earlier black screen after New Game did not appear in this run.
- Conclusion: `RECOMP_AC97_READY=1` is the only switch added compared with the 2026-10-24 black-screen run. One run is not proof that it is the cause.
- Not implemented: saving. The save-creation screen is reached, but the save is not written.
- Screenshot: `docs/img/save-screen.png` (if present in the repository).

## 2026-10-25 — state after repeated maintainer runs (docs update; no new stderr)
- Phase: menus and start-game stage.
- Observed (maintainer reports from repeated physical-gamepad runs): create and load save work; the main menu is reached; the start-game stage draws the player's 3D model but not the stage world (only bamboo is visible); the pause menu works.
- RVA / target: not recorded.
- Diagnosis: not applicable (observation only).
- Switches: not recorded for these runs. The README captions assume `RECOMP_USB=1 RECOMP_FB_WINDOW=1 RECOMP_PB_EXEC=1 RECOMP_AC97_READY=1` with a physical gamepad.
- Earlier docs: README and KNOWN_ISSUES at 79df37e say saving is not implemented. These reports supersede that for create and load. Where the save file is written is not recorded.
- Open: stage world not drawn; black-screen cause unknown; kernel ordinal 175 unnamed; the change that let the pad reach New Game (2026-10-22 to 2026-10-24) is not recorded.
- Fix: none (docs only).
- Result: README, KNOWN_ISSUES, ROADMAP, CHANGELOG, FAQ, NOTICE, CONTRIBUTORS, CONTRIBUTING, RUNNING, AGENTS, PIPELINE, ARCHITECTURE and TITLE_OTOGI updated to this state.

## 2026-10-26 — the input stall is a guest single-CPU race; the dispatch gate clears it

- Phase: menus / gameplay (any run with a pad present).
- Symptom: with `RECOMP_USB=1` the title stops flipping after 16-24 flips. The
  `[GPU]` report repeats `0.00 fps (17 flips)` and `[GPU] draws 30` unchanged,
  the kernel counters show `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
  (ordinals 277/294) climbing, and a debugger finds two guest threads inside
  `sub_00172BD2`, called from the USB software-queue drain `sub_00172F70`
  (`sub_00173068 -> sub_0017302C -> sub_00172F70 -> sub_00172BD2`). One thread
  is in the submit path, one in the drain, both inside the same enqueue.
- Evidence that it is concurrency and not a queue bug: `taskset -c 0` (the whole
  process on one core, so guest threads can no longer run at the same instant)
  runs the same command for 30 s at 47.95 fps with 1585 flips and no stall.
- RVA / target: 0x0017302C (enqueue), 0x00172F70 (drain), 0x00172BD2 (start
  transfer). The self-link itself is the one recorded on 2026-10-22: when
  `tail == request`, `[tail+0x24] = request` points the node at itself and the
  drain never terminates.
- Diagnosis: other — guest code that assumes the console's single CPU.
- Fix: `RECOMP_DISPATCH_GATE=1`. The toolkit's gate serialises the transitions
  into DISPATCH_LEVEL and delivers device interrupts through the gate holder,
  which is the property the USB stack was relying on. Measured: 1739 flips at
  18.27 fps in 25 s with the gate, 17 flips and no progress without it.
  `src/game/main.c` now sets it on by default (`otogi_set_default_env`), because
  no run with a pad survives without it; an explicit
  `RECOMP_DISPATCH_GATE=0` still overrides.
- Result: the title runs with a pad for minutes instead of seconds. Next
  blocker: reaching the stage needed input, see the next entry.

## 2026-10-26 — headless pad input: RECOMP_AUTOPRESS on POSIX

- Phase: menus / gameplay.
- Symptom: none; the Windows backend had `RECOMP_AUTOPRESS` and the POSIX one
  did not, so a headless run could not leave the title screen. The guest
  reaches a pad only through the emulated USB host, so there was no other way
  in without hardware.
- Fix: `src/input/xinput_device.c`, `autopress_state()` — a START tap at 1.5 s
  and 3.5 s, then an A tap every `RECOMP_AUTOPRESS_PERIOD_MS` (default 1200).
  A tap holds the button for a quarter of the period, so the driver sees an
  edge. START is deliberately not left on the schedule: with START and A held
  together for the whole run, a 280 s run ended back in a menu instead of in
  the stage.
- Result: `RECOMP_PB_EXEC=1 RECOMP_USB=1 RECOMP_AUTOPRESS=1` boots, starts a
  new game, and reaches the start-game stage with the player character
  rendered, in about 90-120 s of wall time, with no pad and no window.

## 2026-10-26 — OpenGL back end: it was drawing nothing at all

- Phase: menus / gameplay. Build: this tree, `build-linux`.
- Symptom: every frame captured with `RECOMP_FB_DUMP` was uniformly black or
  uniformly white, on a machine where the backend itself reported
  `[PBGL] OpenGL 4.6 (Core Profile) Mesa 26.2.4, AMD Radeon RX 570 Series`,
  created its targets and uploaded its textures. The census looked healthy
  (`22972894` indices, `12 textured` batches) while the picture was empty.
- Diagnosis: a missing `glUseProgram`. `nv2a_pb_gl.c` built the program and
  cached its uniform locations, but never made it current, so every uniform
  write and every draw was an error and no fragment was produced. The draws
  were counted by the executor either way, which is what made it look like a
  rendering difference rather than a no-op.
- Fix: `nv2a_pb_gl.c` binds the program in `gl_ready()` (once per thread, with
  the context), and remembers that the context is current for the thread so a
  draw no longer pays a `SDL_GL_MakeCurrent` per batch.
  Also in the same file: triangles with non-finite or off-scale (>1e6 px)
  vertices are dropped, as the CPU rasteriser's span test effectively does —
  `[GPU] transforms:` reports 1.6M such vertices, and GL was rasterising the
  wedges they made over the scene.
- Result: logos, the front-end menus, the intro cutscene and the start-game
  stage all render through GL. `docs/img/*` were captured through it.

## 2026-10-26 — vertex programs: 12 fps to 40 fps on the stage

- Phase: gameplay (stage), also the intro cutscene. Build: this tree.
- Symptom: the stage and the cutscene ran at 11-13 fps with ~68k vertices per
  frame. `perf record -F 199 -g` on the run put `nv2a_vsh_run` at 20.5% and
  `read_src` at 16.8% of every cycle in the process (37% together), and the
  guest's own D3D kickoff (`sub_000FA0C0`, which spins on the PFB flush bit at
  0x100410 until the walker clears it) at 44% — the guest was waiting for the
  CPU vertex stage.
- Diagnosis: other — the software vertex shader was the frame.
- Fix, all in the toolkit:
  - `nv2a_vsh_interp.c`: the program is decoded once (a generation counter
    bumps on every program write) into a `VshIns` form with the source mux,
    swizzle, negate and constant index resolved, instead of four bitfield
    extractions per source per vertex.
  - `nv2a_vsh_input_mask()` + `nv2a_pb_exec.c`: the executor fetches only the
    attribute streams the program reads, not all sixteen.
  - `nv2a_pb_exec.c`: the transform runs on a small work-claiming pool
    (`RECOMP_XF_THREADS`, default cores-1, max 8). The decoded program is
    read-only, attributes are guest-memory reads and each vertex writes only
    its own output slot, so no lock is needed.
- Result, same scene: 12.35 fps at flip 8097 before, 39.87 fps at flip 12241
  after (2.68M draws, 401M vertices, 26.9M triangles dropped behind the eye).
  Menus and the attract loop: 90-120 fps. Intro cutscene: 25-29 fps.
  `[GPU] draws 30`-style stalls are gone; the title sustains a frame loop.
- Next blocker: the stage world is still not drawn, see `KNOWN_ISSUES.md`.

## 2026-10-27 — the register combiners reach the OpenGL back end

- Phase: menus (pause menu), gameplay (start-game stage). Build: this tree,
  `build-linux`, captured with `RECOMP_FB_DUMP` from a `RECOMP_PAD_SCRIPT` run.
- Symptom: through the OpenGL back end every text run in the pause menu had a
  black rectangle behind it — "Restart", "Back to Main Menu", "Retire", the
  description line, the OK/BACK labels. The same menu through the CPU
  rasteriser (`docs/img/pause-menu.png`) drew the text straight onto the panel.
- Diagnosis: other. `nv2a_backend.h` said the back end's pixel stage was
  "diffuse times stage 0" and that register-combiner state was not passed on.
  The title's pixel stage *is* the register combiner, and `RECOMP_GL_COMB`
  dumps show the draws are not that product: stage 0 reads `T0` and `v0`, the
  final combiner takes `R0`, and several batches take their colour from the
  alpha channel. The CPU rasteriser has evaluated these registers since
  `nv2a_combiner.c` landed, which is why the two rasterisers disagreed on the
  same batches.
- Fix, all in the toolkit:
  - `nv2a_backend.h`: `Nv2aBatch` carries the combiner (the same `Nv2aCombiner`
    the CPU path evaluates), all four stage textures and the fog colour;
    `Nv2aVertex` carries colour 1, the fog factor and each stage's texture
    coordinate, so the shader has the inputs `nv2a_rc_eval` has.
  - `nv2a_pb_exec.c`: `backend_flush()` fills them from the live state and
    `backend_from_xf()` fills the per-vertex half.
  - `nv2a_pb_gl.c`: the fragment shader evaluates the combiners — the same
    arithmetic on the same registers, decoded to uniforms once per batch. The
    stage modes it does not implement (cubemap, clip plane, bump, dot product)
    leave the batch on the old modulate rather than on a wrong result.
- Result: the pause menu renders with no black boxes behind its text
  (`docs/img/gl-pause-menu.png`), and the stage keeps its bamboo, ground haze,
  light shafts, HUD and player model (`docs/img/gl-stage.png`).
- Still open: effect planes (light shafts, cloud cards) draw as hard-edged
  translucent wedges where the CPU captures show soft haze. Not investigated;
  `KNOWN_ISSUES.md` records it.

## 2026-10-27 — "collapsed geometry" is the title culling, not a lost transform

- Phase: menus and stage. Build: this tree, `RECOMP_XF_FLAT` and the flat-batch
  census (`[GPU] flat batches`).
- Question: `KNOWN_ISSUES.md` recorded the stage world as missing because its
  batches "carry geometry that collapses". The census now counts exactly those
  batches — batches that reach the rasteriser and lose every triangle.
- Evidence, stage run: 6776 of 316043 batches lost every triangle (2.1%).
  6771 of them lost them to the eye/near-plane test — the title's own culling
  of geometry behind the camera. Five lost them to zero-area triangles.
  Drilling into the on-screen ones (`RECOMP_XF_FLAT` reports only those now):
  the position buffer holds real data (`-65 0 65`, and different values at
  different indices), the fetch succeeds, and the batch's
  `SET_VERTEX_DATA_ARRAY_FORMAT` carries **stride 0** — every index reads the
  same element, so the title is drawing a deliberate no-op strip. D3D8 hides
  geometry that way; the hardware would collapse it identically.
- Fix: none needed. The diagnostics stay: `RECOMP_XF_FLAT=<n>` (with
  `RECOMP_XF_FLAT_AT=<batch>`) prints the attributes, the program's inputs, the
  disassembly of the program that collapsed them, and the first outputs.
- Result: the transform was never losing the world. The world draws; what it
  drew wrongly was the pixel stage (entry above).

## 2026-10-28 — the missing polygons are painted, not missing

- Phase: start-game stage and the title screen. Build: this tree,
  `build-linux`, headless `RECOMP_PAD_SCRIPT` runs with `RECOMP_FB_DUMP`.
- Symptom: polygons look missing on the player model and the bamboo, most
  visibly through the OpenGL back end.
- Diagnosis: `RECOMP_GL_FLAT=1` paints every surviving fragment one colour
  (magenta) after depth, cull and alpha have all been applied. With it the
  stage draws **all** of it: the bamboo stalks, the canopy, the ground, and
  the player model standing on the path, in one solid magenta silhouette
  (`/dev/shm/gF`, same pad script and report interval as the normal run, so
  the frames line up). The title screen is likewise pure magenta, petals
  included.
- Conclusion: no geometry is dropped, culled or depth-rejected. What is
  missing is the pixel stage's answer for those batches -- they are painted
  with the background's colour. "Polygons missing" is therefore a shading
  defect, and the next lead is the vertex colour and texel inputs the
  combiner is given, not the transform or the primitive assembly.
- Ruled out on the way, so it is not retried:
  - The register combiner's fallback. `RECOMP_GL_NO_COMB` (the back end's
    original modulate, no combiner) makes the stage *darker* -- mean pixel
    11205 against 18955 with the combiners on -- so the combiners are not
    what blacks the scene out.
  - An unimplemented stage mode. The new `[PBGL] stage modes` census counts
    every `SET_SHADER_STAGE_PROGRAM` slot per run: 0 x2242885, 1 x1286954,
    4 x284427, 6 x270 by frame 6000. Only mode 6 is outside the set the back
    end used to refuse, and it accounts for 270 stage uses in six hundred
    thousand.
  - The G8B8 texture (below), which was real but only touched the effect
    planes.
- Fix in this build, all in the toolkit, all matching what the CPU rasteriser
  already did (`nv2a_pb_exec.c`'s `rc_stage_fetch`):
  - `nv2a_pb_gl.c`'s `rc_stage()` now implements the whole mode table --
    none, projected 2D, cubemap as a 2D lookup, passthrough, clip plane --
    and returns zero for the bump-env and dot-product modes. The clip-plane
    stage discards the fragment, which is what killing the pixel means.
  - `rc_upload()` no longer refuses a batch whose stage uses a mode the
    shader used to lack. Refusing it disabled the combiner for that batch
    and painted it `v0` times a texel, which is black for a lit model.
  - `Nv2aBatch` carries `clip_plane_mode`, filled from the executor's state.
- Diagnostics added: `[PBGL] stage modes` (once per run at frames 300, 3000
  and 6000), `RECOMP_GL_NO_COMB`, and the PVIDEO bank probe below.

## 2026-10-28 — G8B8 textures did not decode

- Phase: start-game stage, the light shafts and cloud cards.
- Symptom: `[PBGL] texture 81453000 256x256 fmt 28 will not decode; drawn
  untextured`, four times per run -- the GL back end bound white for that
  texture, so the effect planes came out as opaque hard-edged quads instead
  of the soft alpha the title's combiner asks for (the open item in
  `KNOWN_ISSUES.md` since 2026-10-27).
- Cause: `sample_tex()` had no case for `D3DFMT_G8B8` (0x28), and
  `linear_twin()` did not map it to its linear twin `D3DFMT_LIN_G8B8` (0x17),
  so a swizzled G8B8 texture fell through to the unsupported-format path.
  G8B8 is two 8-bit channels, green in the high byte and blue in the low,
  with no alpha -- the name reads high-to-low like every other one in that
  switch.
- Fix: both cases, in `src/kernel/nv2a_pb_exec.c`. `will not decode` is now
  empty for a full stage run.

## 2026-10-28 — the attract movie: no PVIDEO, and a black quad

- Phase: the looping `d:\movie\otogi_us_promo.xmv` attract movie, which the
  title starts about 6 s after boot and plays until a button is pressed.
  It needs a run with **no** pad input: the START taps every 1.5 s that the
  stage runs use skip it.
- What is established:
  - The title opens the file (both opens succeed), reads it in ~160 KB
    chunks for the whole period, and allocates a 1200 KB contiguous buffer
    -- 640x480x4, `0x818E9000`. `RECOMP_TEX_DUMP` shows it binding two
    640x480 textures: `81853000` fmt 0x24 (YUY2) and `818E9000` fmt 0x1E
    (LIN_X8R8G8B8). Both read back all-zero at first bind.
  - **PVIDEO is never programmed.** The new probe in `pvideo_write()` prints
    the first four `NV_PVIDEO_BUFFER` writes with a bank pending; a full
    attract run prints none. So the overlay is not how this title shows its
    movie, and the fact that `fb_present.c` composites PVIDEO only in the
    Win32 presenter does not bear on it.
  - `RECOMP_GL_FLAT` over the same period is a **uniform magenta field**
    (mean 43690, the exact value of pure magenta, for every frame): the
    movie is drawn as a full-screen quad that reaches the rasteriser and
    passes every gate. With the combiners on, that same quad paints pure
    black.
  - `RECOMP_PB_GL_TEXREFRESH=15` does not lift it, so it is not the texture
    cache holding the first (undecoded) copy.
- Status: open. The quad is submitted and its answer is black; the CPU
  rasteriser samples the same texture live and shows the frame, missing one
  triangle at the bottom. Both halves point at the texture the batch is
  handed, and the next step is the `RECOMP_GL_COMB` record for that batch
  (it is a 2D batch, so the dump covers it -- it just has not been caught
  yet, because `RECOMP_GL_COMB=8` spends its budget on the boot screens).
- Where the fix belongs: the OpenGL back end's texture path
  (`src/nv2a/nv2a_pb_gl.c`), if the decode is wrong, or the executor's
  texture state if the batch is handed the wrong buffer.

## 2026-10-28 — stage 3D: the back end culled the wrong side of every mesh

- Phase: gameplay (start-game stage, OpenGL back end)
- Symptom: bamboo trunks drew as hollow black wedges with only a green rim,
  the player model was a black inside-out silhouette, and the ground was not
  visible at all. The CPU rasteriser drew the same stage differently again
  (less geometry), so neither path was the reference.
- RVA / target: n/a (render state, not a lifted function)
- Diagnosis: the OpenGL back end mirrored `SET_FRONT_FACE` in `apply_state()`
  on the theory that the vertex shader's y flip had to be paid back. It does
  not: the flip makes window-space winding match the NV2A's screen-space one,
  so the title's own name carries over. Mirrored, every closed mesh kept its
  back faces, i.e. the *inside*, which is what the black wedges were.
- Evidence: `RECOMP_GL_NO_CULL=1` on the same cutscene (`3fps` capture,
  `/tmp` scrub) drew the trunks solid and green and removed every black
  wedge; with the mapping corrected the same is true with culling on.
  `RECOMP_GL_STATE` on a 400-batch sample shows the title mostly runs with
  culling disabled (`cull 0`), so the mistake only showed on the meshes that
  do enable it, which is why menus looked right and the grove did not.
- Fix: `src/nv2a/nv2a_pb_gl.c`, `apply_state()`: `glFrontFace()` maps
  0x0900/0x0901 straight through instead of swapping them.
- Ruled out in the same session (do not retry): running the loaded vertex
  program for fixed-function batches (`RECOMP_VSH_ALWAYS=1`) — the title then
  issues 1.7M draw calls and stops flipping; the movie strips are not 3D
  geometry but the title's XMV quad (see `KNOWN_ISSUES.md`); the missing
  ground is not culled geometry — `RECOMP_GL_NO_CULL=1` leaves it just as
  invisible.
- Result: trunks solid, player no longer inside-out. Still open: the player
  model tears in close-up cutscene shots, and the ground still reads as the
  sky colour on both back ends. Both point at the pixel/vertex-colour the
  lit stage batches are given, not at culling.

## 2026-10-28 — close-up player tore into shards: triangle strips wound every other triangle backwards

- Phase: gameplay / cutscene (OpenGL back end)
- Symptom: in close shots the player model was covered in black triangular
  shards and its silhouette was spiky; `RECOMP_GL_NO_CULL=1` made the model
  whole, which pointed at the cull test rather than at the transform.
- RVA / target: n/a (primitive decomposition in the executor)
- Diagnosis: `NV_PRIM_TRIANGLE_STRIP` was decomposed as three consecutive
  vertices in ascending order. The hardware reverses every other triangle
  (v0v1v2, v2v1v3, v2v3v4, ...) so that the whole strip faces one way; taken
  in ascending order, every odd triangle faces the other way. Nothing culls,
  nothing shows; with culling on, each pass keeps a different half of the
  triangles and the mesh shreds into exactly the triangles that flipped.
- Evidence: `RECOMP_CULL_LOG=25` on a stage run shows the model arriving as
  `prim 6` (strip) batches with culling on, and consecutive batches
  alternating `face 404` (cull front) and `face 405` (cull back) with the
  same `front 900` -- the two-pass scheme whose union has to be the whole
  mesh. `RECOMP_GL_STATE` alone never showed this: it samples the first
  batches of a frame (the sky), which all run `cull 0`.
- Fix: `nv2a_pb_exec.c`, both decomposition paths (`raster_batch_program` and
  the screen-space `raster_indexed`): odd strip triangles are emitted as
  (i+1, i, i+2). The CPU rasteriser fills either winding, so this only
  changes what a culling back end keeps.
- Also added (diagnostics, off by default): `RECOMP_CULL_LOG=<n>` prints the
  first n batches that enable culling, with flip and cull/front values;
  `RECOMP_GL_FRONT_MIRROR` forces the opposite front-face convention, to
  settle that question on a capture; `RECOMP_FOG_ONE` hands every pixel the
  no-fog factor, to separate a fog unit that never stops fogging from a
  combiner that reads the fog register the other way round.
- Result: the close-up model is whole and detailed with culling on. Open: the
  ground. `RECOMP_GL_FLAT=1` paints the whole lower half of the stage frame
  magenta, so the ground *is* submitted and passes every raster gate -- it is
  being shaded to the fog/sky colour, not culled or missing. That is a
  combiner/fog question now, not geometry.

## 2026-10-28 — the green/yellow/red full-frame panels are the wipe quad

- Phase: gameplay / transitions (OpenGL back end)
- Symptom: solid green and yellow vertical bars, or a solid red half-frame,
  over an otherwise correct scene.
- Diagnosis: the panels move like a wipe -- a full-screen quad the title draws
  for scene transitions -- and they are flat because the quad samples a
  texture this back end decodes to a constant. Same family as the movie quad
  in KNOWN_ISSUES.md: the batch is drawn correctly, the texels are not.
- Status: open. Next step is the `RECOMP_GL_COMB` record for the wipe batch,
  which names the texture and format it was handed.

## 2026-10-28 — the fog convention is right; the ground/foliage question is fog strength

- Phase: gameplay (OpenGL back end, register combiners)
- Symptom: the stage reads as a blue wash; the ground never separates from
  the sky and the bamboo foliage is pale cyan where the CPU reference is
  dark green.
- Diagnosis (settled): the combiner is not reading the fog register the wrong
  way round. `RECOMP_GL_COMB3` on the stage's big 3D batches shows the final
  combiner as `final 130C0300/00001C80` -- A = 0x13 (fog.a), B = 0x0C (r0),
  C = 0x03 (fog.rgb), D = 0 -- i.e. `out = fogcolour*(1-f) + colour*f`, which
  is the same encoding `nv2a_combiner.c` evaluates and the same one
  `d3d8_combiners.c` writes at the D3D level (`lerp(fog_color, result, r_fog.a)`).
  So f = 1 is "no fog", the factor `fog_factor()` hands out when
  `SET_FOG_ENABLE` is clear is right, and no inversion is needed.
- What is left is *how much* fog the stage gets: the same record shows
  `fogf 0.429` on a batch of foliage that is only a few units from the camera,
  and the fog colour for the stage is 0xFFFFAA78 -- the pale blue the scene is
  painted with. `RECOMP_GL_NO_COMB=1` (diffuse times stage 0, no fog) shows the
  foliage and the bark as the dark green they should be, which is what the CPU
  reference `docs/img/gameplay.png` looks like in the foreground.
- Status: open, and the next measurement is the fog unit itself: the
  `SET_FOG_PARAMS` pair and `SET_FOG_MODE` the stage sets, against the oFog
  distance the vertex program writes, for a batch the record names. The
  formula in `fog_factor()` follows xemu's vsh, but its two end constants
  (`-1.0` linear, `-1.5` exp) are the part to verify against a known
  start/end pair.
- Not the cause, checked: culling (`RECOMP_GL_FLAT=1` paints the whole lower
  half, so the ground is submitted and passes every gate); the fog colour
  unpack (R/B swap matches the CPU path); the front-face mapping (bamboo,
  ground and player all agree under the pass-through).

## 2026-10-28 — the fog unit's offset painted the stage the fog colour

- Phase: gameplay (start-game stage, both back ends)
- Symptom: the ground never separated from the sky and the bamboo foliage was
  pale cyan where the CPU reference (`docs/img/gameplay.png`) is dark green.
  Nothing about culling, depth or the combiner explained it: `RECOMP_GL_FLAT=1`
  painted the whole lower half, so the ground was submitted and passed every
  gate, and `RECOMP_GL_NO_COMB=1` (diffuse times stage 0, no fog) drew the
  foliage and bark the right dark green.
- RVA / target: n/a (a fog unit constant, not a lifted function)
- Diagnosis: `fog_factor()` subtracted a constant from the NV2A's
  `fog_param0 + coord * fog_param1` (1.0 linear, 1.5 exponential). Otogi
  enables fog, leaves both params at 1.0 and writes its own factor into oFog,
  so the subtraction turned that factor straight into the fog factor:
  px = py = 1 gives `f = d`, where hardware saturates `1 + d` to 1 -- no fog,
  which is what the title wanted. The record that caught it:
  `[FOG] mode 2601 en 1 px 1 py 1 d -nan -> f 1`, and a foliage batch a few
  units from the camera at `fogf 0.429`, i.e. 57% fog colour on geometry the
  title leaves unfogged.
- Fix: `fog_factor()` in `src/kernel/nv2a_pb_exec.c`, linear path only:
  `f = px + d * py`. The exponential paths keep their `-1.5f` -- this title
  runs mode 0x2601 (LINEAR) and nothing here exercises exponential fog, so
  there is no evidence for changing it and the old constant stands until a
  title with one does.
- Also in this pass: `RECOMP_FOG_TRACE=<n>` prints the first n fog evaluations
  (mode, params, distance, factor) -- the capture that settled this, and cheap
  enough to leave on. `RECOMP_GL_COMB3`/`RECOMP_GL_COMB3_MIN` do the same for
  the combiner record of the *big 3D* batches; that record is what confirmed
  the final combiner is `fogcolour*(1-f) + colour*f`, so the convention was
  never the problem and the factor was.
- Result: the stage draws dark green foliage, teal trunks and a dark ground
  under the haze band, and the player model is whole in close-ups with culling
  on. Still open: the transition wipe quad (below) and the movie.

## 2026-10-28 — the stage's blue cast is not the fog unit

- Phase: gameplay (start-game stage, OpenGL back end)
- Symptom: with the fog offset fix in, the stage's frames still read strongly
  blue: over the ordinary run the frames average R 18.3-35.0, G 60.2-76.8,
  B 91.8-114.9, and the foreground band R 27.0 / G 67.3 / B 103.1, against
  R 35.9 / G 51.9 / B 62.2 for the foreground of the reference
  `docs/img/gameplay.png` -- a red deficit and a blue excess.
- RVA / target: n/a (a shading question, not a lifted function)
- Diagnosis: not fog, and this entry is the measurement that closes it.
  `RECOMP_FOG_ONE=1` hands every pixel the no-fog factor, so it removes the fog
  register's contribution whatever `fog_factor()` would otherwise say. Over a
  full stage run (29 dumped frames) it changes nothing measurable: R 19.9-36.3,
  G 63.1-84.3, B 95.4-132.5, i.e. the same spread as the ordinary run within
  run-to-run variance. The blue therefore comes from what the combiner is fed
  with -- the title's lighting, ambient and colour constants -- or is the
  title's own night palette.
- Fix: none. A hypothesis closed, not a bug found; nothing was changed.
- Result: the next measurement is a like-for-like CPU capture. Two attempts at
  it are recorded here because both failed the same way: `RECOMP_PB_GL=0` with
  the pad script that puts the OpenGL run on the stage never reached the stage
  (a 220 s run sat on the damask screen, a 420 s run ended on the `#003264`
  fade with `sd = 0` for its last twenty frames), so the two back ends were
  never looking at the same frames and no colour differential can be quoted
  from them. Whoever picks this up should drive the CPU path until it is
  *on* the stage first, then gate the dump.

## 2026-10-28 — the full-frame colour panels are the title's, on both back ends

- Phase: gameplay (stage transition)
- Symptom: a frame that is one flat colour with only the HUD on it -- `#003264`
  (0,50,100), `sd` 0.048 with the HUD in it -- among otherwise normal stage
  frames; the same runs also produce a perfectly flat `(102,53,0)` panel.
  These were read as the "transition wipe quad" of `docs/KNOWN_ISSUES.md`.
- RVA / target: n/a
- Diagnosis: two independent readings say the title draws them itself.
  (1) `RECOMP_GL_FLAT=1` over the same period gives a `#003264` frame whose only
  magenta is the HUD's region: FLAT paints magenta everything that is submitted
  and passes depth, cull and alpha, so a screen-filling quad would be magenta,
  and it is not -- nothing is drawn over the clear colour then.
  (2) The CPU rasteriser, `RECOMP_PB_GL=0`, produces the identical frames: `sd`
  exactly 0 at (0,50,100) for twenty frames, and a flat `(102,53,0)` panel
  before them. That is the path this project treats as the reference.
- Fix: none. Documentation only: the flat full-frame panels are removed from
  the wipe's bill.
- Result: `docs/KNOWN_ISSUES.md` gains an entry saying so, and `ROADMAP.md`
  stops naming the wipe. The movie's two green/yellow strips are a different
  symptom on a different surface and stay open.

## 2026-10-28 — the screen-sized surface is constant on both back ends

- Phase: gameplay / cutscene (the wipe and movie investigation)
- Symptom: a 640x480 `A8R8G8B8` (0x12) surface at `0x80AD5000` is bound as a
  texture by up to 3166 batches a frame (`[TEXUSE] 0x80AD5000 640x480 fmt
  0x12 lin: 3166 batches`) and `RECOMP_TEX_DUMP` writes it out uniform:
  `t08_80AD5000_fmt12.bmp`, mean = sd = 0.501961, i.e. every texel 0x80808080.
  A screen-sized surface sampled that often and reading as one flat colour is
  exactly the shape of the "wipe quad's texels decode to a constant" note.
- RVA / target: n/a (texture content, not a lifted function)
- Diagnosis: not ours. The same dump under `RECOMP_PB_GL=0` produces the byte
  identical picture -- same file name, same address, mean 0.501961, sd 0. So
  the CPU rasteriser, which is the reference and which writes its surfaces into
  guest memory directly, sees the same constant. The title's own content there
  is 0x80808080; the GL texture cache (`tex_find`/`tex_upload` in
  `src/nv2a/nv2a_pb_gl.c`) is not holding stale data for it, and no readback of
  a render target is missing.
- Fix: none. A candidate cause removed, not a bug found.
- Result: this lead is closed. Two things are still open on the movie: the
  strips did not reproduce in this capture (a 210 s `RECOMP_PAD_SCRIPT` run
  gated at 42 s shows the black attract frames -- `sd` 0.0127 at R 0.14 /
  G 2.97 / B 6.16 -- and then a clean stage, no two-band frame anywhere in the
  28 dumped frames), and the 640x480 YUY2 (0x24) and `LIN_X8R8G8B8` (0x1E)
  surfaces KNOWN_ISSUES names were never sampled in that window, so the movie
  did not draw there at all. Whoever continues needs a capture that actually
  contains a strip frame before anything else can be measured.

## 2026-10-28 — the two-strip movie frame did not reproduce; the effect band does

- Phase: cutscene / stage (the wipe and movie investigation)
- Symptom: three captures were taken to reproduce the two full-height strips,
  one green and one yellow. None contains a two-band frame:
  (a) a 210 s `RECOMP_PAD_SCRIPT` run gated at 42 s, 28 frames, 42-210 s;
  (b) a 320 s run gated at 150 s with the pad script truncated after its 160 s
      tap so the pre-stage cutscene is not skipped, 29 frames, 150-320 s;
  (c) a 260 s `RECOMP_AUTOPRESS=1` run gated at 30 s, 11 frames.
  What all three do contain is the *black* movie: `sd` 0.0127 at R 0.14 /
  G 2.97 / B 6.16, and the YUY2 (0x24) and `LIN_X8R8G8B8` (0x1E) surfaces the
  issue names are never sampled in any of them (`RECOMP_TEX_DUMP`, 64 distinct
  textures, none of either format), so the pre-stage cutscene never draws in
  these runs at all.
- RVA / target: n/a
- Diagnosis: the strips cannot be fixed or verified without a capture that
  contains one. Nothing in the three runs reaches that batch.
- What the same runs *do* show, and it is the effect-plane symptom rather than
  the movie: a flat **olive full-height band** across the bamboo (stage frame
  `f00026`, lower third), i.e. a soft light-shaft card painted one colour. The
  batches that draw the full-screen effects are in the combiner record:
  six vertices, `stpar 000004C1`, `final 0000000E/00001C80`, and three stages --
  `tex0 81453000 256x256 fmt 28 pitch 2560` (G8B8), `tex1 80AD5000 640x480
  fmt 12 pitch 2560` (the surface that is uniformly 0x80808080 on both paths,
  entry above) and `tex2 81473000 256x256 fmt 19`. The G8B8 mask itself decodes
  to concentric rings, which may be what it is authored as (a distortion
  mask), so the band is not proof of a decode fault -- but the three-stage
  full-screen batch is the one to instrument next.
- Fix: none this entry. `RECOMP_GL_NO_COMB=1` on a stage run separates "the
  combiner paints the band" from "the card is a flat quad".
- Result: the wipe entry stays open on the *movie* half and is now explicitly
  un-reproducible; the effect-plane band is the live, reproducible symptom.

## 2026-10-28 — the wipe frame caught: half a screen and a pure-white quad

- Phase: area transition (attract loop -> cutscene -> stage)
- Symptom: `RECOMP_AUTOPRESS=1`, 260 s, gated at 30 s, frame 25 and 31 of 33:
  the image is split by a hard vertical seam, the left half dark and the right
  half the blue stage, and a **pure white rectangle** (255,255,255 measured on
  a 120x120 crop at +510+350) sits over the lower right of the frame with a
  small purple crescent `#6B5091` inside it and the HUD orb missing. Left/right
  half means differ by 0.34 (L 0.062, R 0.400). This is the two-strip symptom
  of the wipe entry, in its own colours: one half of the transition, and a
  quad drawn white.
- RVA / target: n/a (a batch, not a lifted function)
- Diagnosis: white is the documented failure colour of a texture stage on both
  paths -- `sample_tex()` returns 0 and the combiner takes white, and the GL
  back end binds `s_white` for a stage that did not bind (`stage_bind` returns
  0). The crescent is the only part of that quad's texture that arrives, which
  is the "32-bit read of a half-written surface" note in the issue. It is *not*
  a decode refusal: `[PBGL] ... will not decode` is absent from the run
  (`grep -c` = 0), and `RECOMP_TEX_DUMP` never writes a texture that is mostly
  white, so the stage is not bound at all for that batch rather than bound and
  undecodable.
- Fix: none yet -- this entry is the capture the issue was missing. The next
  step is `RECOMP_GL_COMB=<n>` on this exact run and reading the batch whose
  `stpar` has stage 0 off (the record prints `(none)` for a stage with no
  texture), then following that batch's `SET_TEXTURE_*` methods back to the
  register the executor and the GL path disagree about.
- Result: the wipe/movie entry now has a reproducible stage frame instead of a
  description, in `RECOMP_AUTOPRESS` runs, with the exact defect signature.

## 2026-10-28 — the wipe quad's combiner state, caught in the log

- Phase: area transition (same `RECOMP_AUTOPRESS` window as the entry above)
- Symptom: the transition's untextured quad identified. `RECOMP_GL_COMB=60` on
  the run that produces the seam records exactly one two-triangle screen quad
  with **no stage-0 texture at all**:
  `[GLCOMB] batch n 6 col 80000000 spec FFFFFFFF uv 0 0 fogf 1 stpar 00000000
  ctl 00011102 final 0000000C/00001C80 f0 00000000 f1 00000000 fog FFFFAA78`
  followed by `tex0 (none)`. `col 80000000` is black at 50% alpha;
  `stpar 00000000` says every texture stage is off; `final 0000000C/00001C80`
  is A = r0 (0x0C), B/C/D = 0, so the pixel stage passes r0 straight out and
  neither the fog colour it carries (`0xFFFFAA78`) nor a texture can reach the
  pixel. The quad is therefore drawn as a flat black 50% veil by construction.
- RVA / target: n/a
- Diagnosis: the question is no longer "which texture decodes to a constant"
  but "why is `SET_TEXTURE_FORMAT`/`SET_TEXTURE_CONTROL0` for this draw's
  stage 0 zero when the title draws a transition quad". `stpar` is the
  executor's own record of the stage-enable bits, so the registers are zero at
  submit time on our side -- the title either really does fade with a plain
  veil, or it sets those bits with a method the executor does not mirror.
- Fix: none. This is the exact register state to work from; a breakpoint on
  `record_tex_reg()` during the transition shows whether the methods arrive.
- Result: the transition quad is now identified in one log line, which is what
  the wipe entry has been missing; nothing was changed in the renderer.

## 2026-10-28 — the pad script can move the camera; the ground is still behind the haze

- Phase: gameplay (start-game stage)
- Symptom: every capture so far had the camera tilted up, because the scripts
  only tapped START/A, so the ground could not be looked at. `usb_gamepad.c`
  already parses stick entries (`lup`/`ldown`/`lleft`/`lright` and the right
  stick), so `190000:rdown:6000` was appended to the stage script and a 235 s
  run gated at 185 s took 31 frames with the camera moved.
- What that frame shows (frame 19): the bamboo canopy from below, the mist
  band, the HUD orb, and the **olive full-height effect band** of the entry
  above, standing in front of the trunk line. The lower part of the frame is
  blue-grey haze with no terrain silhouette or texture, so the ground is
  submitted (FLAT paints it) but reads as haze at this camera angle -- the
  same "ground never separates from the sky" signature the fog fix was meant
  to clear, at an angle the earlier captures never sampled.
- Diagnosis: open. Two candidates remain, and they are separable: the haze
  band itself (an effect plane drawn over the ground) or the ground's own
  shading. `RECOMP_FOG_ONE` changed nothing, so it is not the fog factor.
- Fix: none. The script change (a stick hold) is the useful part -- it is how
  the next run gets a camera that looks at the ground at all.
- Result: ground assessment is now possible from a capture instead of blocked;
  the frame to start from is `/tmp/gGND/f00019.bmp` in this session's scratch.

## 2026-10-28 — the white quad is not any of the recorded six-vertex sprites

- Phase: area transition
- Symptom: with the combiner record now carrying each batch's screen box, the
  transition's pure-white rectangle -- measured at **136x136 at +499+309** on
  `f00030` -- can be looked up. No record covers it. The 2D records nearest it
  are `box 411..626 330..379` (a 215x49 strip) and `box 450..575 386..445`
  (125x59), and neither is 136x136 nor sits at 499,309; the full-screen ones
  (`1..641 1..481`) are the fade quads.
- Diagnosis: the white rectangle is not a six-vertex screen-space sprite, so it
  is either a vertex-program batch (drawn through the 3D path, which the same
  record covers only with `RECOMP_GL_COMB3`) or several sprites overpainting
  each other. That is the next lookup: `RECOMP_GL_COMB3` with a low
  `RECOMP_GL_COMB3_MIN` on the same run, matching the box.
- Fix: none; the diagnostic that makes the lookup possible was added
  (`xboxrecomp` `62e72d1`, `comb_report()` prints and hashes the box).
- Result: "which quad is white" is now a query with an answer in one log line
  instead of an inference. The box is printed only when `RECOMP_GL_COMB` or
  `_COMB3` is set, so nothing changes for a normal run.

## 2026-10-28 — the texture cache ignored the pitch

- Phase: rendering, all scenes (texture cache)
- Symptom: none visible on this title's current scenes -- found by reading the
  cache while chasing the white quad, not by a failed frame. Worth an entry
  because it is a wrong-answer bug rather than a missing feature.
- Diagnosis: `GlTex` in `src/nv2a/nv2a_pb_gl.c` carried offset, width, height,
  format and levels; `tex_find()` matched on exactly those. For a **linear**
  format the pitch is the row stride the decoder walks (`sample_tex()` does
  `mem + base + v * t->pitch`), so the same offset and size bound with a
  different pitch is a different image: the cache returned the first decode
  and the second bind sampled a stride it was never decoded with. This title
  binds 640x480 `A8R8G8B8` surfaces at pitch 2560 (`[GLCOMB] tex1 off 80AD5000
  640x480 fmt 12 pitch 2560`), so the fields are live in its register stream.
- Fix: `GlTex` carries `pitch`, `tex_find()` matches on it, `tex_upload()`
  stores it (`xboxrecomp` `4c600d0`, noted in `UPSTREAM.md`).
- Result: a 235 s stage run before and after the change produces the same
  frames to the digit (`f00030`: sd 0.136441, R 37.93 / G 42.16 / B 49.83 both
  runs), which is what a correction with no live collision should look like --
  the bug was latent here, not the cause of the white quad.

## 2026-10-28 — the two rasterisers disagree on screen-space batches

- Phase: rendering, pixel stage (all scenes that draw pre-transformed batches)
- Symptom: none on its own -- found while chasing the transition's white
  rectangle, which never reproduced in today's captures. The question was
  "which batch paints (560,370) white", and the answer needed to know where
  the back end's pixel stage and the CPU rasteriser's part company.
- RVA / target: not a guest address; `raster_batch()` in
  `xboxrecomp/src/kernel/nv2a_pb_exec.c`.
- Diagnosis: other -- a structural divergence between the two rasterisers.
  `raster_batch()` routes a batch three ways: a vertex-program or
  fixed-function batch goes to `raster_batch_program()`, whose per-triangle
  path sets `T.use_rc = s_gpu.rc_seen && !no_rc` and evaluates the register
  combiners; a batch that is neither goes down the **screen-space** path
  (`raster_indexed()` -> `raster_triangle()`), which never calls
  `nv2a_rc_eval()` at all -- with texture coordinates it puts the sampled
  texel, without them it puts the vertex colour. The back end, meanwhile, is
  handed `batch.combiner = s_gpu.rc_seen ? &s_gpu.rc : NULL` for *every* batch
  (`backend_flush()`), so `nv2a_pb_gl.c` runs the combiners on the very batch
  the CPU painted without them. Any screen-space batch whose combiner answer
  differs from "texel, or vertex colour" is a frame the two paths render
  differently, and a combiner that resolves to a constant paints a flat
  rectangle over the scene.
- Fix: none yet -- this is recorded because it is a live divergence, not a
  theory. A diagnostic landed with it: **`RECOMP_GAP_LOG=<n>`** prints the
  first n screen-space batches that reach `backend_flush()` with combiners
  seen, with the state that decides the colour (the four 5-bit
  `SHADER_STAGE_PROGRAM` modes, which stages hold a valid texture, the bound
  stage-0 offset and size, the first vertex, the vertex colour).
  It fires on an ordinary run:
  `[GAP] screen-space batch 8685 flip 757 n 134 textured 1 modes 1/0/0/0 texvalid 1000 tex0 812A3000 32x64 v0 -109.908 -76.2728 col FF3F7F00 rc 1`
  i.e. stage 0 in PROJECT2D mode with a real 32x64 texture bound, painted by
  the CPU as a bare texel and by the back end through the combiners.
- Result: named and instrumented. The next step is to decide which side is
  right -- hardware runs every pixel through the combiners (`nv2a_combiner.h`
  says so), so the CPU path is the one under-implementing -- and then to make
  `raster_triangle()` evaluate them. Not attempted: it is a per-pixel change
  in the reference path and needs its own capture to verify against.

## 2026-10-28 — the white rectangle did not reproduce; the detector that said
## it did was reading a white screen

- Phase: area transition / cutscene (the white 136x136 rectangle at +499+309)
- Symptom: a scan of today's captures reported 81 frames containing the
  rectangle. They are the **SEGA "Published By" logo** -- a solid white screen,
  so every 136x136 crop of it is pure white.
- Diagnosis: other -- a bad test, not a rendering fault. The detector now
  requires the ring around the box to be more than half non-white, which the
  logo screen fails and the real rectangle passes.
- Fix: none; the corrected scan found **zero** such frames in 1474 frames of a
  200 s `RECOMP_AUTOPRESS` run, 1468 frames of a 150 s run, and 798 frames of
  a `RECOMP_PB_GL=0` run. The rectangle did not appear in any of them, so
  today adds no evidence about it either way; the earlier captures (gWIPE,
  gAUTO) remain the only ones that contain it.
- Result: the hunt is blocked on a capture that contains the event. What makes
  that a query rather than a guess now exists: `[FB] frame <path>` is printed
  for every dumped frame, so a back end record can be placed against the frame
  on disk, and the box field added earlier in `comb_report()` names where each
  reported batch sits.

## 2026-10-28 — flips stop mid-run again, with the dispatch gate on

- Phase: gameplay / any long run
- Symptom: four of today's runs stopped presenting partway through and never
  resumed: 299 dumped frames then silence (gW9), 247 (gGAP), 301 (gGAP2), 1474
  (gG3). The guest stops as well as the dumps -- `[GPU] draw` freezes (e.g.
  `draw #10800` unchanged across 10 s) and the log stops growing except for the
  periodic `[KERNEL] ordinal` summary, while the process keeps burning CPU.
- Diagnosis: the input-stall signature again (BOOT_LOG, 2026-10-26), which
  `RECOMP_DISPATCH_GATE=1` -- set by default in `src/game/main.c` -- was meant
  to remove. No `irq held off by guest IRQL` line appears in these runs, so the
  old signature is not being printed, only the shape of the failure.
- Fix: none. A run that dumps every frame (stride 1) reached 1474 frames before
  it stopped; a run that dumped with a stride stopped sooner, so frame dumping
  is not what stalls it. `taskset -c 0` and a longer pad script are the two
  things not tried today.
- Result: every capture today is truncated at 1-3 minutes of wall clock, which
  is why no run reached the ~190 s point where the transition and the movie
  are known to play. Until a run survives past that point, the white rectangle
  and the movie strips cannot be measured.

## 2026-10-28 — the title screen's depth patch: what the state of those frames is

- Phase: title screen / intro (the rectangular patch over PRESS START)
- Symptom: maintainer report (2026-10-28, `KNOWN_ISSUES.md`): a rectangle over
  the PRESS START area shows a polygon pattern that is not part of the artwork,
  over the falling petals; `RECOMP_GL_FLAT` paints the whole title magenta, so
  the patch is depth and not geometry.
- RVA / target: none -- `RECOMP_GL_STATE=<n>` on a headless title run.
- Diagnosis: open, but the state is now on record. In the title window
  (flips 300-600) 912 sampled batches split:
  - 444 `depth 0 func 203 wmask 0`, 464 `depth 0 func 203 wmask 1` -- depth
    testing off, so most of the screen does not read the depth buffer at all;
  - 3 copies of one batch: `n 5766 depth 1 func 203 wmask 0 ... z 1.56704e+07
    rhw 0.00669077 col 80000000 uv 119.496 133.733` -- depth test **on**
    (LEQUAL) with no depth write, a world-space mesh of 5766 vertices in
    half-alpha black;
  - **one** batch that both tests and writes depth: `batch 4349 flip 579
    n 216 depth 1 func 203 wmask 1 ... v0 708.156 -91.0903 z 1.40982e+07
    rhw 0.0160521 col FF808080 uv 8 32`.
  So exactly one draw per sampled frame writes depth while a 5766-vertex mesh
  depth-tests against it: that pair is where a rectangle can be carved out of
  a mesh that would otherwise cover the screen, which is the shape of the
  reported patch. Its box was not printed -- the record carried only `v0`,
  which for that batch is off-screen (x 708) and says nothing about the region.
- Fix: none yet. The diagnostic was extended instead: `[GLST]` now ends with
  `box x0..x1 y0..y1`, computed from the batch's vertices, so the next run
  answers "which rectangle does the depth-writing batch cover" in one grep and
  can be matched against the patch's position on a captured frame.
- Result: next step is one command -- `RECOMP_GL_STATE=8000` on a title run,
  grep `wmask 1` inside `depth 1`, and compare its box with the patch. The
  depth divisor is not the fault: both the clear (`zclear_value()`) and the
  back end (`z_divisor()`) read `SURFACE_FORMAT` bits 4-7 the same way and both
  fall back to 2^24 for anything that is not Z16, so a format the table does
  not name still gives every draw and every clear the same units.

## 2026-10-28 — the title does send CLEAR_RECT, and it is the whole screen

- Phase: title screen / intro (the rectangular patch over PRESS START)
- Symptom: the entry above left two candidates for a hard rectangular
  boundary -- the back end has no scissor, and `SET_CLEAR_RECT` is honoured
  only on swizzled surfaces -- and neither was exercised by any log, because
  the method histogram prints only its top ten.
- RVA / target: `SET_CLEAR_RECT_HORIZONTAL` 0x1D98 / `_VERTICAL` 0x1D9C.
- Diagnosis: other -- answered, and both candidates fall. The executor now
  prints the rectangle when it arrives (with `RECOMP_GL_STATE`), and a title
  run reports exactly one change:
  `[GLST] clear rect 0..640 0..480 swz 0 set 3`
  i.e. the title does program the clear rectangle, but it is the **entire
  surface**, on a non-swizzled one. The CPU's linear clear ignores the rect
  and clears the whole clip; the back end clears the whole target. Both clear
  the same pixels, so a partial clear cannot be what carves the patch, and the
  missing scissor cannot be what bounds it for this title either.
- Fix: none needed for this title. The back end is still not told about
  CLEAR_RECT, which stays a latent gap for a title that sends a partial one on
  a swizzled surface -- recorded here rather than fixed, because nothing in
  this title can show the difference.
- Result: of the two rectangular-boundary candidates, both are closed, so the
  patch is left with the pair named in the entry above -- the 5766-vertex mesh
  that depth-tests and the draws that ignore the depth buffer entirely --
  plus whatever else the `[GLST]` box now makes visible. The next run should
  diff a frame with `RECOMP_GL_NO_DEPTH=1` against one without, at a fixed
  petal position, rather than across whole animations.

## 2026-10-28 — the rectangle over PRESS START was the bump-env stage, unimplemented

- Phase: title screen / intro
- Symptom: `KNOWN_ISSUES.md`, "a depth patch on the title screen" -- a hard
  rectangular band over the PRESS START area showing something that is not
  part of the artwork, seen on both rasterisers. Two days of depth
  investigation (previous three entries) had ruled out the depth divisor, the
  clear rectangle and the scissor without naming it.
- RVA / target: none -- `SET_SHADER_STAGE_PROGRAM` and
  `SET_TEXTURE_SET_BUMP_ENV_MAT`.
- Diagnosis: other -- the stage is implemented, the stage *mode* was not. The
  title programs `stpar 000004C1` on the title screen: stage 0 PROJECT2D
  (a 256x256 `fmt 1C` bump map), **stage 1 BUMPENVMAP**, stage 2 PROJECT2D
  (the 640x480 `A8R8G8B8` screen surface), stage 3 off. Its batches sit exactly
  where the artifact is -- `box 287..359 374..402` and `305..341 381..395` on a
  640x480 frame. `rc_stage_fetch()`'s switch sent every mode past 5 to the
  "reads zero" default, and the back end's `rc_stage()` did the same for
  `mode >= 6u`, so that stage contributed `(0,0,0,0)` to a combiner that
  expects the warped screen sample: the quad painted a hard-edged dark band
  instead of the water/heat-haze warp over the artwork.
- Fix: `BUMPENVMAP` (6) and `BUMPENVMAP_LUMINANCE` (7), in both rasterisers,
  following xemu's `psh.c`:
  - `nv2a_pb_exec.c`: `rc_stage_fetch()` takes the stage results so far. Mode 6
    reads the previous stage's texel as a normal map -- `sign3(b)`, `sign3(g)`
    -- runs it through the stage's 2x2 bump matrix and adds the result to this
    stage's normalised coordinates before sampling; mode 7 multiplies the
    sample by `scale * luminance + offset`. `sign3()` and
    `sign3_to_0_to_1()` are xemu's.
  - `nv2a_pb_exec.c`: `SET_TEXTURE_SET_BUMP_ENV_MAT` (0x1B28..0x1B34, four
    floats), `_SCALE` (0x1B38) and `_OFFSET` (0x1B3C) are recorded per stage
    into `Nv2aCombiner` (`nv2a_combiner.h`), with a `bump_valid` mask; an
    unprogrammed stage keeps an identity matrix. `RECOMP_BUMP_LOG=<n>` prints
    the first n writes as floats.
  - `nv2a_pb_gl.c`: the same arithmetic in the fragment shader -- `sign3`,
    `rc_bump_dir()`, modes 6 and 7 in `rc_stage()` -- with `uRcBump` /
    `uRcBumpSO` uploaded from the combiner state; identity where unprogrammed.
- Result: the band is gone. Same frame of a 70 s `RECOMP_AUTOPRESS` capture,
  region (287,374)-(359,402): before mean 3.4 / sd 3.05, after mean 85.4 /
  sd 56.4 -- the artwork continues through the area, where it used to stop at a
  straight edge. The other four vertices of `stpar 000004C1` batches, which
  span `-1315..2156`, are the same effect elsewhere on the screen.
- Open: the title writes **zero** into every bump matrix word
  (`[BUMP] stage 1 mat[0..3] 0`), so the sample is not displaced at all and the
  effect is a plain screen-space read of tex1. Either the title relies on the
  scale/offset registers alone, or the matrix lives at other method addresses
  on this title -- the four words are the matrix by the register map, and a
  nonzero value would settle it. Until then the matrix multiplies by zero,
  which is what the registers say.

## 2026-10-29 — the stage's landing ends on the Retire prompt

- Phase: gameplay / intro
- Symptom: a scripted run reaches the stage at 93 s (bamboo grove, HUD, light
  shafts, the player mid-air in a landing pose at frame ~3200 of a 5311-frame
  capture) and then, with no further pad input, the title itself puts up
  "再開 / Restart / Back to Main Menu / Retire" over the stone-lantern scene.
- RVA / target: none -- no fault, no missing asset.
- Diagnosis: other. Measured, not guessed: the pad log shows exactly two
  START presses in the whole run (t=1500 and t=3503 ms, the ones the timeline
  sends) and no other START, so the prompt was not the pad's. The backdrop
  behind the prompt is drawn live -- two frames eight seconds apart differ in
  5407 of 115200 pixels with mean 1.0 per channel, which is animated fog, not
  a still -- and it shows a wet reflective floor with the lanterns mirrored in
  it, so the ground is not missing everywhere, only under the player.
- Fix: none yet.
- Result: the open question is now specific. Two frames stand against each
  other: the pause backdrop draws a floor the scene camera does not, and the
  prompt follows a landing. `tools/capture.sh stage-walk` (new) drives that
  same path with the camera pitched down, and `RECOMP_GL_NO_COMB` /
  `_NO_DEPTH` / `_NO_CULL` through its `EXTRA_ENV` split the pixel stage from
  the geometry in one A/B pair. Those three runs on the old timeline all left
  the same prompt on screen, so the timeline, not the switch, is what to fix
  first.

## 2026-10-29 — the stage's ground is not in the frame, and NO_BLEND's "ground" is not it

- Phase: gameplay (start-game stage, OpenGL back end)
- Symptom: the stage's floor never appears under the player; the bottom of the
  frame is exactly the clear colour `srgb(0,50,100)` at (320,470), (50,440)
  and (600,450) in a frame where the player stands. The pause backdrop draws a
  floor, so the title is not simply missing a mesh.
- What was measured, in order:
  - `RECOMP_GL_FLAT=1` paints the lower half magenta, so *something* passes
    depth/cull there. That is not the ground: FLAT skips the alpha test, so a
    full-screen effect quad paints solid magenta too, and the stage has one
    (the haze/screen pass).
  - `RECOMP_GL_NO_BLEND=1` fills the frame with a grey rock image and hides the
    player and the bamboo behind it. The user report settles what that is: it
    sits *above* the player and the bamboo and its position and texture do not
    match the reference, so it is a screen-space pass drawn opaque, not the
    terrain.
  - A new `RECOMP_GL_PROBE=x,y[,n[,minverts[,minflip]]]` (`nv2a_pb_gl.c`) does
    the point-in-triangle test against every batch and prints the state of the
    draws that really cover the pixel, plus the pixel before and after each
    draw (`pre`/`post` RGBA). On a stage frame where the player stands, the
    only batch covering (320,440) is a 525-vertex program batch with
    `z 9.88e9` (`0x50169...`), `rhw 1`, `blend 0`, `wmask 1`, culling front;
    its depth is far past the divisor, so it cannot paint. No terrain batch
    covers the pixel at all.
  - `RECOMP_GL_NO_DEPTH=1` does not restore the floor on the same scene, and
    `RECOMP_GL_NO_CULL=1` only reveals the player's own back faces. The floor
    is not depth- or cull-culled.
- Conclusion: the stage's terrain is either not submitted or is submitted and
  loses every triangle, and what a screenshot shows under NO_BLEND is a
  screen-space pass. The next measurement is the census the executor already
  keeps: `RECOMP_XF_FLAT=<n> RECOMP_XF_FLAT_AT=<batch id>` on a stage run. The
  run that reached the stage ended at 772,461 batches, so the filter start has
  to be below that (the first attempt used 1,000,000 and printed nothing --
  that is the whole reason it produced no records).
- Tried and reverted: deriving the fixed-function transform from NV2A's
  projection (0x0440) and model-view (0x0480) when D3D's composite is absent
  (`RECOMP_FFP_COMPOSITE`, an opt-in experiment). It never engaged -- those
  registers are zero on the stage's MODE 0 batches, so the title's transform
  is not there either; no `fixed-function transform:` line appeared and the
  frames were unchanged. `RECOMP_BATCH_CLASS` does show large MODE 0 batches
  taking the SCREEN path with model-space vertices (a 291-vertex strip at
  v0 y = 8000.01, and 1848-vertex prim-5 meshes at v0 -40 -359), so the next
  run should use `RECOMP_BATCH_CLASS=200` to reach the stage and see whether
  the floor is one of them.
- The dropped batches are **not** the terrain. A new `RECOMP_XF_DROP=<n>`
  reports the first n fixed-function batches thrown away as "not screen-space",
  with the reason (fetch-failed / off-surface / object-space span) and the full
  transform context. On a `to-stage` run all eight reported drops are
  `fetch-failed` (`v0 0 0 0 1`, span 0 x 0, `inline 0`): stride-0 vertex arrays
  the title submits as deliberate no-op strips, exactly the shape the 2026-10-27
  census recorded (`SET_VERTEX_DATA_ARRAY_FORMAT` of stride 0). So the 68,601
  "skipped as not screen-space" batches in a 150 s stage run are those strips,
  not geometry waiting on a transform. Whoever continues should stop chasing a
  composite matrix for them.
- What the ground likely is, from `RECOMP_GL_PROBE` (2026-10-29, above): the
  only batch covering a ground pixel was a 525-vertex **program** batch
  (`mode 0x2`) with `z 9.88e9` (`0x50169...`) and `rhw 1`, i.e. a depth far past
  the 2^24 divisor, so it loses every depth test and cannot paint. That points
  at the **vertex program's depth output** (or the divisor) for the terrain,
  not at a missing or untransformed mesh. The next measurement is the
  `RECOMP_XF_BAD` / `[GLST]` record of the program batches whose transformed z
  is out of range on the stage, matched against a ground pixel.
- **2026-10-29, the transform measured.** `RECOMP_XF_BAD` (extended to flag
  out-of-range `z`, and to print the MVP rows `c[112..115]` plus the fetched
  `v0` *and* the output for the vertices either side of the bad one) settles
  it. The terrain inputs are **smooth, real terrain** — consecutive vertices
  differ by ~100 model units: `(4021,-18221,-2866)` -> `(3947,-18114,-2942)` ->
  `(3727,-16790,-961)` — so it is **not** a position-fetch bug and **not**
  stacking at the input. What goes wrong is the transform's `w`: it
  **straddles the eye plane**, swinging from `-1822` (behind the eye) to `+37`
  (just in front) between adjacent vertices. At `w` near zero the perspective
  divide explodes (screen `x` reaches 311912) and `z` lands past the far plane
  (1.77e7 > 1.677e7), so those triangles fail the depth test and are dropped
  (4,054,005 "dropped behind the eye" in one run). Under normal rendering the
  ground is therefore invisible; `RECOMP_GL_NO_BLEND` / `_FLAT` reveal only the
  mispositioned slivers the near-plane clip lets through. The MVP rows are
  present and plausible (small rotation + translation), so the matrix is not
  missing — the open question is whether the near-plane clipper in
  `raster_xf_clipped` handles the eye-plane straddle the way the hardware does.
  Next: A/B `RECOMP_CLIP_NEAR_Z=0` against the default on a ground-facing
  frame.
- Also closed in this pass: the D3D8→D3D11 question. This title statically
  links XDK D3D and never calls the toolkit's D3D8 layer (the layer is for a
  title that calls the API; `docs/technical/pushbuffer-executor.md` says so).
  A Windows D3D11 back end would sit behind the *same* executor decoder and
  would not answer a missing-batch question; the CPU rasteriser is already the
  reference by project rule, and `RECOMP_PB_GL=0` is how to reach it. Not done.
- **2026-10-29, the transform exonerated, the gate is fragment alpha.** Three
  measurements close the previous entry's open question and open a narrower
  one.
  1. *The terrain program is correct, and so are its constants.* The program
     computes `clip = (dot(v0,c[112]), dot(v0,c[113]), dot(v0,c[114]))`, divides
     by `dot(v0,c[118])`, applies viewport `c[58]/c[59]`, and leaves
     `oPos.w = dot(v0,c[115])`; `c[118] == c[115]` exactly (same w row by
     design). Hand-recomputed for the flagged vertex of batch 82781: inputs
     `(-7123,-18650,-5890)`, w = -61.8687, and
     `x = 2309.28/-61.8687*320+320.5 = -11623.6` = the reported output to four
     digits; same for y (125158) and z (4.390e7 = clip.z/w * 2^24). The
     projection is `z/w = 1 - 100/w`: **near plane 100, far infinity** — which
     is also why rows `c[114]` and `c[115]` look near-identical (differ by
     exactly the 100-unit near translation). The "bad" vertices XFBAD flags
     are behind-eye vertices that the clipper must cut; their z past far is
     arithmetic, not corruption. The earlier "w straddles the eye" reading was
     right about the straddle and wrong about who is at fault: ground under a
     close camera legitimately straddles the eye, and clipping it is normal.
  2. *Clip order is not the gate.* `RECOMP_CLIP_ZFIRST` (near plane before
     `w=1e-3`) and `RECOMP_CLIP_ZONLY` (near plane alone) were A/B'd against
     the default on ground-facing frames (`rup` pitch): no visible difference;
     `zfirst` still leaves ~20k fan vertices at `w < 1e-2`, `zonly` ~12k. The
     `w`-plane does produce the x1000 z blow-up the GL_PROBE saw (9.88e9 =
     9.88e6 x 1000 at `NV_CLIP_W`), but cutting those triangles differently
     does not paint the ground, so it is a secondary artifact, not the gate.
     New counters in the census: `clip: N vertices at w < 1e-2, M clipped with
     z past far`.
  3. *The gate is fragment alpha, and it comes from the stage-0 texture.* The
     evidence chain, all from matched (`rup`, stride 2, 170 s) captures:
     `RECOMP_GL_FLAT` paints the ground (its magenta path returns before the
     alpha test — FLAT does *not* prove the alpha test passes, a correction to
     the comment on `state_env_init`), `RECOMP_GL_ALPHA_ONE` (new: forces
     `c.a = 1` after the combiner) makes the ground mesh **appear** as a pale
     blue surface, and `RECOMP_GL_NO_COMB=1` (modulate fallback) leaves it
     **invisible** — because the fallback alpha is the same product. Decoding
     batch 42838/70992's combiner: `final alpha = read_alpha(f1>>8) = r0.a`,
     `r0.a = v0.a * t0.a` (alpha ICW `D4D81010`), `v0.a` defaults to 1 in
     `nv2a_vsh_run` (`outregs[3][3] = 1`), so **fragment alpha = the stage-0
     texture's alpha plane**, blended `SRC_ALPHA/ONE_MINUS_SRC_ALPHA` with
     alpha test `GREATER 0`. The terrain batches' stage-0 textures (32x64 and
     128x256 A8R8G8B8/DXT5) sample as
     `alpha 0..201 (17 of 25 points zero), red 206..239` — plausible ground
     colour with an almost-empty alpha plane. RGB under the combiner is
     `v0.rgb * t0.rgb` (normal modulate), so colour is fine; only alpha
     collapses, which is exactly "invisible normally, opaque under FLAT or
     ALPHA_ONE".
  Next question (narrow): *why is that alpha plane empty* — is the texture's
  guest memory really alpha-0 (then the title wrote it through a path that
  loses alpha: render-target writeback or a clear missing the A byte), or does
  the sampler read the wrong bytes? The CPU rasteriser run (`RECOMP_PB_GL=0`,
  same pad) splits it: ground visible there = GL upload bug; invisible = data
  or title-side path. Diagnostics added this pass, all env-gated:
  `RECOMP_CLIP_ZFIRST`, `RECOMP_CLIP_ZONLY`, `RECOMP_GL_ALPHA_ONE`, XFBAD's
  constant dots + extra constants + combiner words + per-stage texture alpha
  sampling + blend/depth state line, and the two clip counters.


## 2026-10-30 — the attract movie is the wall: no run so far reaches the stage

- Phase: front end / pre-stage movie, OpenGL back end.
- What happened: every capture started with the `to-stage` timeline today
  stalls in the title's own attract movie. `terr4` (210 s, no frame dumps)
  spent the run reading the movie file (`[READ] from=0x000E1BDC` 752 times)
  and its GL mode histogram at frame 3000 is `0 x92562 1 x30606`, where a run
  that does reach the stage reports `0 x1273072 1 x726797 4 x161805 6 x270`
  (`drw1`, 2026-10-29). The movie is black on this back end
  (`KNOWN_ISSUES.md`), so the screen says nothing about which state the run is
  in — the histogram is the only marker that told these runs apart.
- Consequence for the record: the drop census those runs printed is
  *movie-era*, not stage-era. The reading "0 batches skipped as not
  screen-space, 0 vertices off-screen/NaN" describes the movie and does **not**
  settle the 2026-10-27 question of whether the stage's terrain is thrown away
  by `batch_is_screen_space()`. That still needs a census gated to a stage
  batch id; `RECOMP_XF_DROP_AT` and `RECOMP_BATCH_CLASS_AT` were added today
  for exactly that (every earlier census spent its budget in the menus).
- Consequence for the timeline: `tools/pads/to-stage.txt` presses A every
  1.2 s only up to 85 s and relies on one of those presses skipping the
  pre-stage movie. When the title reaches the movie later than that, the run
  sits in it for the rest of the capture. Until the movie is fixed — or the
  timeline is made to react to the screen — a stage capture is a coin toss.
- Two alpha A/B runs were started on 2026-10-29 and never read; both are
  unusable as written. `RECOMP_GL_NO_ALPHA=1` (`noalpha`) collapses the title
  to ~1.7 fps and `RECOMP_GL_ALPHA_ONE=1` (`aone2`) to 0.3 fps, because every
  fragment the alpha test normally discards now draws and blends; neither
  reaches a stage frame inside a 200 s timeout. The alpha question is still
  open at the stage, and the next attempt has to gate on a stage flip rather
  than on a wall clock.
- The X8R8G8B8 sampling change was reverted. Returning the X byte as alpha
  instead of 1.0 was left in the tree to explain the movie quad painting black
  over the film; the movie is still black with it in place (maintainer,
  2026-10-30), so it explained nothing and silently changed the semantics of
  every X8 texture in the run.
- Diagnostics restored to the tree (they were uncommitted, and a revert during
  this session dropped them): `RECOMP_TEXHEX`, `RECOMP_DRAW_LOG[_N]`, the
  XFBAD raw-bytes dump with c[0..3], and the XFDROP fetch-failed filter.
- Note on `RECOMP_DRAW_LOG`: it stops after 40 batches by default and those 40
  are spent in the front end. The 32x64 `A8R8G8B8` texture at `0x814C2000`
  that the 2026-10-29 entries call the terrain's was reported from *those menu
  draws*; its stage draws were never printed. Raise `RECOMP_DRAW_LOG_N` and
  gate the report to a stage batch before treating that texture as the ground.

## 2026-10-30 — the movie's 0.33 fps was the GL texture hash, run once per bind

- Phase: attract movie, OpenGL back end.
- Symptom: every capture made after the 2026-10-29 GL change crawled inside the
  movie at 0.3-6 fps and never reached the stage; `drw1` (22:31 that day, before
  the change) ran at 24-42 fps on the same timeline.
- Cause: `stage_bind()` in `src/nv2a/nv2a_pb_gl.c` hashed the bound texture's
  whole source (`pitch * height`) on **every** bind, to notice a buffer the
  title rewrites in place. A 640x480 surface is 1.2 MB with its pitch and the
  movie binds one several hundred times a frame, so the check cost hundreds of
  megabytes of hashing per frame.
- Fix: the hash is sampled at most once per texture per frame
  (`GlTex.hash_frame` / `live`), and a streamed (`YUY2`/`UYVY`) source is
  re-uploaded at most once per frame (`upload_frame`). Frame granularity is
  also the correct one: a decoded video frame or a written-back surface changes
  between frames, not between binds.
- Measured: same timeline, no frame dumps, 120 s — **18.5-22.6 fps**
  (1741-3603 flips) with 443 movie reads, against 6.2 fps decaying to 0.33 fps
  before the fix (`stage2`).
- Still open: the movie draws black (`KNOWN_ISSUES.md`) and the run still does
  not reach the stage with this timeline — the GL mode histogram at frame 3000
  is `0 x88581 1 x29279`, where the stage reports modes 4 and 6 as well.

## 2026-10-30 — correction: the per-bind hash is not what collapses the movie

- The entry above attributes the movie's 0.33 fps to the per-bind texture hash,
  on a comparison that was not like for like. The numbers do not support it:
  `terr4` (pre-fix binary, **no frame dumps**) ran at 19.7 fps, and `perf1`
  (post-fix, no dumps) at 18.5-22.6 fps. The 0.33 fps shows up only in runs
  with `RECOMP_FB_DUMP` set — `stage2` before the fix (6.2 fps decaying to
  0.33) and `stage3` after it (7.1 decaying to 0.34).
- So the once-per-frame hash stands as a cost reduction (sampling 1.2 MB per
  bind is wasteful whatever the frame rate) but it is **not** the cure, and
  the movie stall is still unexplained. `docs/KNOWN_ISSUES.md` keeps it open.
- Next measurement, narrow: the same timeline and pad script, `RECOMP_FB_DUMP`
  on and off, and `RECOMP_FB_DUMP_STRIDE=1` against `30`, to separate the dump
  itself from the flip it hangs off. `stage3` also ran the full 180 s of pad
  script while producing only 930 flips and never left the movie, so the title
  is stalled, not merely slow — the dump path is where to look, and a
  `RECOMP_GATE_TRACE` run in that state would say whether it is a wait.

## 2026-10-30 — START does not get past the movie; the title is stalled there

- Measured on the fixed build with frame dumping **off**, so the dump path is
  not in play: `perf2` ran 150 s at a steady 21 fps (3427 flips), read the
  movie file 409 times, and its GL mode histogram at frame 3000 is
  `0 x86763 1 x28673`. A run that reaches the stage reports modes 4 and 6 as
  well (`drw1`: `0 x1273072 1 x726797 4 x161805 6 x270`).
- The pad script pressed START every 6 s from 90 s to 180 s. `stage3` (same
  script, frame dumps on) consumed all 180 s of pad time while producing only
  930 flips. So START is not the lever, and the movie is not merely slow: the
  title does not leave it, whatever the frame rate. `RECOMP_FB_DUMP` makes the
  symptom worse by an order of magnitude but is not the cause either.
- This makes the XMV path the critical path for M5, not a side issue: no
  capture reaches the stage while it holds, and the 2026-10-30 timing above
  says the run's own clock keeps advancing while the title does not.
- `tools/pads/to-stage-long.txt` goes back to A taps (the START variant is
  recorded here as falsified, with the histogram, so it is not retried).
- Next measurement: `RECOMP_GATE_TRACE=1` on a run in this state, to see what
  the title is waiting on when it should be leaving the movie, and a
  `RECOMP_TEX_DUMP`/`DRAW_LOG` pass over the 640x480 surfaces the movie binds.

## 2026-10-30 — bisect: the stall is not from this session's or yesterday's edits

- `xboxrecomp` was rebuilt at `4e5f71c`, the submodule commit the last run that
  reached the stage came from, and run with the same no-dump timeline: GL mode
  histogram at frame 3000 `0 x85764 1 x28340`, 18.7-20.6 fps, 3644 flips, no
  stage modes — indistinguishable from the current build's `0 x86763 1 x28673`
  at 21 fps. The only toolkit-side differences between the two are the
  2026-10-29/30 uncommitted changes (texture hash, TEXHEX/DRAW_LOG), so those
  are not the cause of the stall.
- Maintainer observation, not yet measured: the title goes slow **after
  clicking Load save**, and that is the screen the A taps land on — the
  `noalpha`, `terr2` and `terr3` frame dumps all show "Load Game — Please
  select a save game". That redirects the suspicion from the movie to the save
  load: a kernel wait or a file read that times out, rather than the XMV path.
- `RECOMP_GATE_TRACE=1` over 90 s of this state produced 5623 lines, every one
  `[gate] signal object=0x00106B6C` — one object signalled continuously, no
  wait records. A trace taken across the Load save click, and
  `RECOMP_FRAME_TRACE`, are the next measurements.

## 2026-10-30 — the far plane is a clip, not a clamp; the wedges shrink, the floor stays missing

- Phase: gameplay (start-game stage, OpenGL back end), toolkit at 1c16a7d.
- **Change, and its reason.** `nv2a_pb_gl.c`'s vertex shader clamped window z to
  [0,1] before rebuilding clip space (`gl_Position.z = clamp(z,0,1) * w`). A
  vertex past the far plane then sits exactly on z = w: it is never clipped,
  and the triangle it belongs to is drawn pinned to the far plane instead of
  disappearing. Clipped by the hardware's own `z <= w` test it is gone. The
  clamp is removed, so GL far-clips the way D3D does.
- **Measured on the same pad timeline** (`stage-walk`, stride 2): a scan of
  every 4th frame for the share of strongly coloured pixels (max-min > 110)
  reports `s1` (before) **0.975** at its worst -- whole frames solid red with
  only the HUD and the haze left -- against `s8` (after) **0.35**. The
  full-frame solid-red captures are gone; the stage still reaches and renders
  (84 s of pad time, player, bamboo, HUD), so the change is not a regression.
- **What did not change:** the coloured wedges are still there on some frames
  (`s8` f01416/f01820: blue/green/red vertical bands across the frame, straight
  edges, vertex-colour interpolation). They are one huge triangle whose three
  vertices carry R, G and B, not a textured mesh -- a batch that should not be
  on screen at all. `RECOMP_GL_PROBE` at (100,100) on a 165 s run recorded
  1200 covering batches, and every full-screen one is a legitimate fade quad
  (colour `FFFFFFFF` easing to `19000000`, `alpha 204 ref 0`); the wedge was
  not caught on that run's timeline, so naming its batch is still open.
- **The floor is still absent on both back ends.** A `RECOMP_PB_GL=0` capture
  (CPU rasteriser, `cpu1`) never left the title screen in 135 s at ~4 fps, and
  the maintainer reports the CPU path draws no floor either -- so the question
  is upstream of the back end, in the executor or in the title's data.
- Next measurement, narrow: arm `RECOMP_FRAME_TRACE` on a frame that shows the
  wedge (touch the flag file while the artifact is on screen), which lists
  every batch of that frame with its texture, blend, triangle and pixel counts
  -- the repeated red frames in `s1` (pad t ~59 s) are the timeline to aim at.

## 2026-10-30 — the "terrain texture with empty alpha" is a petal sprite

- The 2026-10-29 chain ended on "the terrain's stage-0 texture samples as
  `alpha 0..201 (17/25 points zero)`", and that number has been the assumed
  gate ever since. `RECOMP_TEX_DUMP` on a stage run (`tx1`, 55 distinct
  textures) writes each sampled texture through the sampler, so the one that
  produces those samples can be looked at rather than argued about:
  - `814C2000` (32x64, fmt 06 SZ_A8R8G8B8) is a **single white-pink petal** on
    a transparent field -- the falling blossom sprite. Its alpha is empty
    everywhere except the petal, which is exactly the "0..201, mostly zero"
    reading. It is not terrain.
  - `826C1000` (128x128, fmt 06) is the HUD's snake medallion, `817D3000`
    (512x256) the SEGA logo, `807D4000` (1024x1024 DXT1) the font page,
    `81966000` (512x512 DXT3) the level-select thumbnails. Every one decodes
    to a coherent image, so the swizzle, the DXT1/DXT3/DXT5 blocks and the
    linear twins are all right.
- So the previous conclusion is **withdrawn**: the terrain is not being
  discarded by a transparent stage-0 texture. What that texture belongs to is a
  sprite drawn with the same combiner words the terrain uses, which is how it
  came to be mistaken for it.
- **The combiner decode was checked against xemu itself**, not from memory:
  `hw/xbox/nv2a/pgraph/glsl/psh.c` `parse_combiner_inputs()` takes
  `a = value >> 24`, `b = >> 16`, `c = >> 8`, `d = value & 0xFF`; the final
  stage is `rgb = d + mix(c, b, a)` with alpha `final_inputs_1 >> 8`, and
  `parse_input()` reads `reg = v & 0xF`, `chan = v & 0x10`, `mod = v & 0xE0`.
  `nv2a_combiner.c` and the GLSL copy match all of that field for field, so
  `r0.a = v0.a * t0.a` and `out.a = read_alpha(f1 >> 8)` are right.
- The one 2880-triangle batch that does cover the whole frame uses
  `tex0 80AD5000 640x480 fmt 12` -- a guest *surface*, not an asset -- with
  `out = r0 = t0` and alpha from that surface. Its dump is flat grey, i.e. the
  surface holds nothing when it is sampled. That is the next thing to read: is
  that pass a screen copy the runtime never filled, or an off-screen render
  whose colour surface is a different VA than the one the sampler reads.

## 2026-10-30 — the near-plane clip is not what hides the floor (A/B, negative)

- Phase: gameplay (start-game stage, OpenGL back end), toolkit at 1c16a7d plus
  this session's uncommitted executor/GL changes.
- **A/B, same pad timeline** (`to-stage` + `rdown`/`lup` from 93 s, camera on
  the ground): default near clip against `RECOMP_CLIP_NEAR_Z=0` (clip at
  `w = 1e-3` only). Both runs reach the walk (`nz`: 290 dumps, pad t = 122 s,
  29 fps, 5,025,862 triangles "dropped behind the eye" in the last block).
  **The floor is missing in both.** The near-plane clipper is therefore not the
  gate, and the earlier entry's "next: A/B `RECOMP_CLIP_NEAR_Z=0`" is answered:
  negative.
- **The stage-era probe budget was the mistake, not the probe.** `RECOMP_GL_PROBE`
  took its 80 records in the front end, where every hit is a full-screen menu
  or haze quad (flip 2-80: `n 6`, `depth 0`, `blend 1 302/303`, `alpha 1 204
  ref 0`, `tex0 817D3000` SEGA logo). Its fourth field is a **starting flip**:
  `RECOMP_GL_PROBE=320,440,60,1,1500` was run at the stage and the records at
  flip 1500 are the cutscene's effect pass (`n 6`, three textures including the
  `G8B8` 0x28 effect page, pre/post `00000000`). Aim it later than the landing
  cutscene or it reports the cutscene, not the ground.
- **New instrument, `RECOMP_PIXEL=x,y`** (`nv2a_pb_exec.c`): every fragment of
  that pixel is logged with `why` (`depth`, `alphatst`, `paint`), the
  interpolated z, the depth buffer's z, the depth/alpha state, the packed
  colour, the stage-0 texture offset/format, the combiner stage program and
  the triangle's box and vertex w. It is the executor's answer to
  `RECOMP_GL_PROBE` and it costs one comparison per pixel. It only fires on
  the **CPU rasteriser** path (`xf_rows`); with `RECOMP_PB_GL` on the executor
  hands vertices to GL and never enters that loop, so a run that wants its
  output has to be `RECOMP_PB_GL=0` — which does not reach the stage in a
  capture window at ~4 fps.

## 2026-10-30 — visibility queries answer 0 on the GL back end, and the pad aimed the camera at the sky

- Phase: gameplay / stage, OpenGL back end, toolkit at 1c16a7d plus this
  session's changes.
- **`tools/pads/stage-walk.txt` aimed the camera the wrong way.** Every ground
  capture since the file was written held `rdown` to "pitch the camera at the
  floor". The maintainer corrected this twice: on this title the right stick
  **up** (`rup`) pitches the view down; `rdown` looks at the bamboo canopy.
  The timeline is fixed (all `rdown` entries are `rup`, with the reason in the
  header) and the frames from `m1`/`gd` confirm it -- `rup` brings the player
  and the trunks into a downward view instead of the open sky. Any earlier
  capture read as "nothing covers the ground pixel" has to be re-read with
  this in mind: several of them were pointed at the canopy.
- **`GET_REPORT`'s `ZPASS_PIXEL_CNT` is always 0 when a back end draws.**
  `s_gpu.zpass_count` is incremented only by the CPU rasteriser
  (`xf_queue_flush` adds `cnt.zpass`); with `RECOMP_PB_GL` on, `xf_rows` never
  runs, so the counter stays at its reset value and every visibility test the
  title takes reads back "no pixels passed". A title culls what it is told is
  invisible. This is the same shape as the Burnout 3 occlusion-culling bug the
  report path was written for (`GET_REPORT` was never written at all there),
  one layer further in: the report is written, with a number that always says
  the opposite.
- **Fix, both halves in this tree:** `nv2a_pb_gl.c` wraps each batch's
  `glDrawArrays` in a `GL_SAMPLES_PASSED` query while the title has counting
  enabled (`SET_ZPASS_PIXEL_COUNT_ENABLE` now forwards to the back end through
  `nv2a_backend_zpass_enable`), and `nv2a_pb_exec.c` answers `GET_REPORT` with
  the CPU counter **plus** the back end's total; `CLEAR_REPORT_VALUE` resets
  both. With no multisampling a sample is a pixel, which is the unit
  `ZPASS_PIXEL_CNT` counts.
- **Bisect available:** `RECOMP_ZPASS_FORCE=<n>` answers every visibility test
  with `n`. A run that draws the floor with a large `n` and not without it is
  a title that culls on the query; a run that draws it neither way is not.

## 2026-10-30 — answering every visibility query with 1,000,000 does not draw the floor

- `RECOMP_ZPASS_FORCE=1000000` on the ground-facing timeline (`gd.pad`: the
  stage at ~93 s, `rup` to pitch the camera down, `lup` to walk): the run
  reaches the grove and the landing cutscene, 5,907 flips at 25-27 fps, and
  **the floor is still missing**. The title does not cull the stage's ground on
  the visibility query, so the corrected `ZPASS_PIXEL_CNT` answer (below) is a
  fix for a real wrong answer, not the missing-floor fix.
- Frames from the same run: bamboo, the player and an enemy stand in the
  grove with the clear colour where the floor should be -- the same symptom
  the CPU rasteriser shows, so the fault is still upstream of both back ends.
- `README.md`'s Screens section now carries two captures from this build
  (`docs/img/gl-menu-2026-10-30.png`, `docs/img/gl-stage-2026-10-30.png`),
  captioned with what is and is not drawn, and the known-limitations bullet
  says "floor" rather than "world" because the rest of the world does draw.

## 2026-10-30 — instruments added, and two more bisects closed (both negative)

- **`RECOMP_GL_BATCH=<minverts>,<minflip>[,<maxflip>]`** (`nv2a_pb_gl.c`,
  `batch_census`): one line per batch whose screen box reaches the frame's
  lower quarter, with the box, window z and w ranges, all four stage textures,
  depth/blend/alpha state, the colour mask and the stage program. This is the
  census that answers "is a sheet of geometry covering the floor at all"
  without a per-pixel probe. At flips 5200-5260 the only batches reaching the
  lower quarter are the shadow-volume passes (`mask 00000000`, `wmask 0`,
  drawn twice with `face 404`/`405`) and the screen-sized effect quad
  (`n 5766`, `tex0 80AD5000` 640x480, `stpar 00000001`).
- **`RECOMP_GL_MASK_FORCE=1` closed negative.** Forcing colour writes on for a
  batch whose colour mask arrived as 0 does not draw the floor. So the masked
  batches are not the floor's colour pass.
- **The GL probe at true gameplay flips.** `RECOMP_GL_PROBE=320,440,150,1,5200`
  (the fifth field is the starting flip -- without it the records are spent in
  the front end) lists, per frame, one `n 6` full-screen quad (`blend 1
  302/303`, `alpha 1 204 ref 0`, `mask 01010101`) and the two shadow-volume
  passes; the readback stays `00326400`, the clear colour. Nothing else covers
  that pixel on the GL path, which is why the floor is not painted there.
- **Hence the open question, stated as narrowly as the evidence allows:** the
  batches that would draw the floor either never reach the pixel (the census
  sees no terrain sheet in the lower quarter at gameplay flips, only the
  shadow passes) or arrive with every fragment killed. The next measurement
  has to name the batch the *title* draws the terrain with -- the D3D8 layer
  says this title emits pushbuffers itself, so it is the stream that decides --
  and that is where the next session should start.

## 2026-10-30 — the z-range cull is off by default; it was never verified

- `SET_ZMIN_MAX_CONTROL` is decoded (`s_gpu.zminmax`, `Nv2aRenderState`'s
  `zclamp_en`/`clip_range_set`), and the GL shader can cull or clamp fragments
  outside `CLIP_MIN..CLIP_MAX`. The **cull** mode is not verified on this
  title -- if those registers hold a depth-buffer range rather than a
  per-fragment one, culling on them removes visible geometry -- so it is now
  behind `RECOMP_GL_ZCULL=1` and the default keeps the previous behaviour. The
  clamp is behind `RECOMP_GL_ZCLAMP=1` for the same reason. A jump that makes a
  model vanish, or a floor that is missing, must not be *caused* by an
  unvalidated gate; this removes that variable from both.
- Next for the "model goes invisible while jumping" report: the player's batch
  is drawn near the eye plane, so the candidates are the near-plane clipper
  (`raster_xf_clipped`), the `a[3] <= 0` drop in `raster_xf_triangle`, and the
  `batch_is_screen_space()` fallback. `RECOMP_GL_ZCULL=0` (now the default)
  rules the z-range cull out first; `RECOMP_CLIP_NEAR_Z=0` separates the
  clipper; `RECOMP_XF_FLAT` / `RECOMP_GL_PROBE` on the airborne frame name the
  batch that disappears.

## 2026-10-30 — the frame-spanning coloured wedge is untransformed world geometry

- Symptom: single frames, mostly during transitions and the stage, painted with
  one huge triangle carrying R, G and B at its three vertices over the whole
  picture (the "wipe quad" in the captures).
- Cause: `raster_batch()`'s screen-space fallback. A MODE-0 batch with **no
  composite matrix** is neither a program batch nor a fixed-function one, so it
  reaches `batch_is_screen_space()`, whose test is only "does it overlap the
  surface, and is it bigger than object space". World-space corners in the
  thousands of units satisfy both, so the batch is drawn with its world
  coordinates used as pixels: one corner lands off one side of the frame, one
  off another, and the interpolated vertex colours fill what is left.
- Fix: that fallback is now suppressed **for batches of 64 vertices or more**
  (`batches_screen_fallback` counts them either way and the periodic report
  prints the count). Small ones -- sprites, menu quads, which is what actually
  arrives pre-transformed -- keep the old path. `RECOMP_NO_SCREEN_FALLBACK=1`
  suppresses all of them, `RECOMP_SCREEN_FALLBACK=1` restores the old
  behaviour for a bisect.
- **Measured**, same pad timeline and every 10th frame, share of pixels whose
  strongest channel differs from its weakest by more than 110 (the metric the
  2026-10-30 far-plane entry used):
  - baseline before this change (`s8`) worst frame **0.35**;
  - fallback fully off (`wq`, 519 dumps, stage reached at pad 125 s,
    731 big-batch fallbacks counted) worst frame **0.108**;
  - the shipped default (64-vertex threshold) is `wq2`, same timeline.
  The menus, the HUD, the grove and the haze are unchanged in both runs.
- **Not shipped as a default (2026-10-30, after the run).** Suppressing the
  fallback cut the metric but the maintainer reports the visible problem is
  still there on the same build, and the late stage frames of `wq2` show the
  world absent where a fade should be -- so the change is left **off by
  default**: `RECOMP_NO_SCREEN_FALLBACK=1` suppresses every such batch and
  `RECOMP_SCREEN_FALLBACK=1` suppresses only batches of 64 vertices and up.
  Neither is the answer to the wedges; the metric moving is not the same as
  the artifact going away, and this is the second time that distinction has
  cost a session.

## 2026-10-30 — the screen-space fallback is drawing world geometry, and it is not the floor

- `RECOMP_BATCH_CLASS=10 RECOMP_BATCH_CLASS_MIN=150` on the ground-facing
  timeline catches the batches that reach `raster_batch()`'s screen-space path,
  and they are world geometry, not pixels:
  `[CLASS] batch 109645 prg 109645 ffp 0: prim 6 n 291 -> SCREEN (mode 0x4
  composite 0 inline 0) v0 102.434 8000.01 176.74 1 clip 640x480`.
  A first vertex at **y = 8000** with w = 1 cannot be a pixel; drawn
  untransformed it becomes the frame-spanning triangle.
- The transform those batches wanted is in the vertex constant file, not in a
  composite method: the same record prints `CMAT` **all zeros** while
  `c[0]` holds a plausible matrix (`-0.00511 0.235472 0.971868 444.244`, …)
  and `c[8]` a rotation (`0.5 0.888889 …`). So a MODE-0 batch with no
  `SET_COMPOSITE_MATRIX` is not "pre-transformed screen space": it is a batch
  whose transform was uploaded as constants and which this executor has no rule
  for. Naming that rule -- which constant rows are world/view/projection for a
  MODE-0 draw, or whether `SET_TRANSFORM_EXECUTION_MODE` 0x4 is itself the
  program path -- is the next lead, and it is a better one than the fallback:
  suppressing the fallback changes the picture (measured 0.35 -> 0.108 on the
  strong-colour metric) but does **not** bring the floor, which is what the
  maintainer's run said.

## 2026-10-30 — running those MODE-0 batches as program batches makes the picture worse

- `RECOMP_FFP_AS_PROGRAM=1` (run loaded program for MODE-0 batches with no
  composite, 64 vertices and up, instead of drawing them untransformed) on the
  ground-facing timeline: pad 111 s, 4,596 flips, 26.9 fps, 319 dumps. The
  stage frames lose the grove entirely -- a thin sliver on a blue field where
  the bamboo and the player should be -- so the program loaded at that moment
  is not the transform those batches wanted. Closed negative; the switch is
  env-gated and off by default.
- That leaves the batch's transform as either the composite method the title
  never writes, or a constant file this executor does not read for MODE-0
  draws. The measured facts are in the entry above (`CMAT` all zeros, `c[0]`
  and `c[8]` full of plausible matrix rows, first vertex y = 8000 with w = 1).

## 2026-10-30 — stage reached in a GL run without the census, and a raw-world batch named

- Phase: gameplay (OpenGL back end), `to-stage` pads plus `rup`/`lup` walking,
  dump stride 3.
- Result: the pad timeline (unchanged) reaches the stage at pad t=93003 ms, and
  the walk is visible in the dumps: rocks, boulders, bamboo, the haze plane and
  the HUD draw; the ground sheet under them does not, and the lower half of the
  frame is the haze and the clear colour. So the timeline and the camera pitch
  are not what has been hiding the floor in earlier sessions: the run arrives
  and the floor is absent where the camera looks straight down at it.
- New: `RECOMP_GL_BATCH_MAX=<n>` raises the census print budget. The menu's big
  sprites exhausted the fixed 4000 lines by flip 2430, so a census aimed by
  flip number alone never reached the stage. `RECOMP_GL_BATCH=64,1120,1260`
  with the raised budget covers the first stage frames.
- Noted while reading the census: a MODE-0 no-composite batch of 867 vertices
  reaches the GL draw path with `w 1..1`, `z -4952.83..5253.85`, box
  `-1175..9155 x -22683..8000` in a 640x480 surface — world coordinates used
  as pixels, textured, depth test and depth write on. That is the raw-world
  batch class the screen-space fallback draws, and it is the source of the
  frame-spanning wedges. It is not yet identified as the terrain.
- Note on run cost: while the attract movie plays the title advances its pad
  clock at roughly 2.5 fps of game time per second of wall time, so a 330 s run
  only reaches pad 38 s. Runs that must reach the stage need the movie skipped
  or a ~5 min budget. `PAD: t=` in the log is the honest progress indicator.

## 2026-10-30 — stage-era census taken: no large batch reaches the lower quarter, and the dropped-batch program path does not bring the floor

- Two runs reached the stage (pad t=93000 ms is the transition; `PAD: t=` is the
  only honest progress line in the log).
- **The census is the finding — withdrawn, it was taken in the wrong place.**
  `RECOMP_GL_BATCH=64,1100,1300` printed no line at all, but flip 1100-1300 is
  the attract movie, not the stage: the stage arrives around flip 3500 in these
  runs (the dump count at pad t=93000 is ~1200 at stride 3), and the pad
  timeline runs out at 125 s, where the title puts up its own Retire prompt
  with no input arriving. A census line missing there says nothing about the
  terrain. `RECOMP_LOG_AFTER=<seconds>` now gates the census and the drop/class
  reports on wall time instead of flip number, and the pad timeline keeps
  playing past the stage with the A taps the Retire prompt needs.
- **The dropped batches, tested directly.** `RECOMP_DROP_AS_PROGRAM=1`
  (`nv2a_pb_exec.c`, in the `!batch_is_screen_space()` path) runs the loaded
  vertex program on a batch that would otherwise be thrown away as "not
  screen-space", instead of dropping it. On a full stage run this changed
  nothing visible: the lower half of the frame at pad 112 s is still exactly the
  clear colour `(0,50,100)`, with the same haze and rocks above it. The count of
  dropped batches at that point is 23,315, and putting every one of them
  through the program does not produce a floor. The switch stays off by default.
- **Texture decode is not the fault.** `RECOMP_TEX_DUMP` on a stage run writes
  64 distinct textures and every one decodes: the SEGA logo, the menus, the
  player, the bamboo, the gravel ground texture, the HUD medallion, the light
  shafts. Formats seen: `0x06/0x12` swizzled and linear 32-bit, `0x0C/0x0D/0x0E/
  0x0F` DXT, `0x28`. No format falls through to the failure fill.
- **Where this leaves it:** the terrain batches are gone before the transform
  path. The next step is on the title's side of the executor: the batch that
  should be the terrain must be named at the *pushbuffer* level (the title
  emits its own pushbuffers, so `SET_BEGIN_END`/attribute/DMA ordering decides
  what a batch is), not at the rasteriser. `RECOMP_XF_DROP_AT` with a batch id
  measured at pad t≈93 s is the instrument; the batch ids to start from are
  what the `[GPU]` summary prints as the drop counter.

## 2026-10-30 — the terrain is submitted and draws; the pixel probe says so

- Instrument: `RECOMP_GL_PROBE=<x>,<y>,<n>,<minv>,<minflip>` now honours
  `RECOMP_LOG_AFTER`, and `RECOMP_GL_COMB_TEX0=<hex>` prints the combiner
  record for batches whose stage-0 texture is that guest offset (the stage's
  ground texture is `824EA000`, 512x512 DXT1, which decodes as gravel).
- **What the probe shows at (320,440) during the walk (flip 5200).** In draw
  order: a series of `mask 00000000` depth-only passes (shadow volumes,
  `wmask 0`), a full-screen `n 6` blend quad, and then the terrain itself:
  `n 330`, `mask 01010101`, `depth 1/203 wmask 1`, `blend 302/303`,
  `alpha 1/204 ref 0`, `stpar 00020421`, `final 130C0300/00001C80`,
  textures `824EA000` (512x512 DXT1), `82586000` (1024x1024 DXT1) and
  `8255A000` (128x128 DXT5). Its vertices are where they should be
  (`v0 322.5 441.8 z 1.64034e+07`), i.e. bottom-centre of the screen.
- **The pixel changes when that batch draws**: `pre 00326400` (the clear
  colour) to `post 0E1919FF` — dark, and opaque. So the terrain is not missing
  from the stream, not culled, not clipped and not alpha-rejected: it paints a
  nearly black colour and is then indistinguishable from the haze behind it.
  Later ground batches paint `121317FF`, `152C2FFF`, `112A2EFF`; all dark.
- **This is a colour fault, not a submission fault.** The next step is the
  combiner evaluation for `stpar 00020421` (stages 0-2 program 1, stage 3
  program 4 = PASSTHRU) and, in particular, the final combiner's fog input:
  `final 130C0300` takes its A and C inputs from register 3 (fog), whose RGB
  this runtime fills with the fog *colour* and whose name the stage sets to a
  factor of 1.0 on these batches. `RECOMP_GL_COMB_TEX0=824EA000` prints that
  batch's stage words now, which is the measurement to take next.
- Caveat: several runs in this session were killed or stalled before the
  stage; `PAD: t=` is the only reliable progress line, and a census/probe
  armed without `RECOMP_LOG_AFTER` spends its budget in the attract movie.

## 2026-10-30 — the terrain's combiner decoded: its colour depends on two textures

`RECOMP_GL_COMB_TEX0=824EA000` on a stage run prints the terrain batch:

```
n 66 ... stpar 00020421 ctl 00011104 final 130C0300/00001C80 fog FF000000
  stage 0 c C4C80000/000000C0  a D4D81010/000000C0
  stage 1 c DA204B20/00000D00  a 00000000/00000000
  stage 2 c CCCD0000/000000C0  a 00000000/00000000
  stage 3 c C9CA20CC/00000C00  a 00000000/00000000
  tex0 824EA000 512x512 fmt 0C | tex1 82586000 1024x1024 fmt 0C
  tex2 8255A000 128x128 fmt 0F | tex3 (none)
```

Decoded with nv2a_combiner.h's rules (stage programs 1,1,1,4 = sample, sample,
sample, passthru; the title's vertex colour is white and the fog factor is 1,
which makes the final stage `out = r0`):

- stage 0: `r0.rgb = v0 * v0`, `r0.a = v0.a * t0.a`
- stage 1: `r1 = t2.a + (2*t3 - 1)` with `t3` the passthru coordinate
- stage 2: `r0 *= r1`
- stage 3: `r0 = t1 * t2 + r0`
- final: `out.rgb = fog.rgb * (1 - factor) + r0 * factor`, i.e. `r0` at factor 1

So the terrain's colour is `t1 * t2 + t2.a + 2*t3 - 1` where `t1` is a
1024x1024 DXT1 texture (`82586000`) and `t2` a 128x128 DXT5 (`8255A000`).
A black terrain therefore means `t1` (and/or `t2`) samples black -- the ground's
own 512x512 DXT1 (`824EA000`) is not even in the final colour, it only
contributes its alpha. Next measurement: `RECOMP_TEX_DUMP` those two offsets
and compare with the reference art; `82586000` is 1024 wide with pitch 2560,
which is the width of a 640-pixel 32-bit row, so the block addressing is the
first thing to check.

## 2026-11-01 — the "terrain" batch was the player; the floor class is a 960-vertex mesh the GL path drops on alpha

- **Instrument (toolkit, working tree only).** `RECOMP_TEX_DUMP` now writes
  every bound stage, not only stage 0 (`<seq>_s<stage>_<offset>_fmt<fmt>.bmp`),
  because a batch's colour can come from any of the four. Two additions:
  `RECOMP_TEX_DUMP_ALPHA=1` writes the alpha plane as greyscale (a DXT5 page
  whose alpha gates the draw is otherwise indistinguishable from a page the
  title wanted see-through), and `RECOMP_TEX_DUMP_ONLY=<hex>` keeps every other
  texture out of the 64-entry inventory so the one behind a named batch still
  gets dumped when the front end has filled the table.
- **`824EA000` is a character/armour atlas, not gravel.** Dumped on a stage
  run it is armour plates, a skull, a clawed gauntlet and red trim;
  `82586000` (1024x1024) is the *same atlas* in greyscale (its specular /
  detail page) and `8255A000` (128x128 DXT5) is a page of small glow spots.
  The batch recorded on 2026-10-30 as "the terrain" — `n 330` at (320,440),
  colour `t1*t2 + t2.a + 2*t3 - 1` — is therefore the **player**: colour
  page times specular page. That entry's decode describes armour shading and
  its "black floor" reading is withdrawn; the pixel it measured is the player
  standing in the lower centre of the frame.
- **The floor class, named.** With the pad timeline holding the stage
  (`tools/pads/floor-walk.txt`, `rup` = camera down) the GL probe
  `RECOMP_GL_PROBE=100,430` reports a **960-vertex / 933-triangle** mesh with
  its box over the lower frame (`194..424 x 180..411`, and a second one at
  `-90..549 10..649`), state `cull 0`, `depth 1 func 203 wmask 0`,
  `blend 302/1`, `alpha 1 204 ref 0`, `stpar 00000001`, `final
  0000000C/00001C80`. Its combiner words (`RECOMP_GL_COMB_TEX0=810AA000`):
  stage 0 `c 08040000/00010C00 a 18140000/00000C00`, one stage
  (`ctl 00000001`), which decodes to `rgb = 2 * t0 * v0` written to r0 and
  `a = t0.a * v0.a`, with the final stage passing r0 through (colour from D =
  r0, alpha from r0.a). Textures: `810AA000` 128x128 DXT5 (stage 0),
  `82649000` 256x256 linear, `8250A000` 256x256 DXT5.
- **That alpha plane is not empty.** `RECOMP_TEX_DUMP_ONLY=810AA000` with
  `RECOMP_TEX_DUMP_ALPHA=1`: 244 distinct levels over the page, 2,836 of
  16,384 texels zero. So "the page is empty" cannot explain a rejected
  fragment.
- **The GL path paints nothing there; forcing alpha paints it.** The probe
  brackets the draw with readbacks: `pre 00326400` and `post 00326400`, i.e.
  the batch is rasterised over the pixel and changes nothing. With
  `RECOMP_GL_ALPHA_ONE=1` the same probe reads `post FFC3C5FF`. On the GL
  side the gate is the fragment alpha, exactly as the 2026-10-29 note said —
  but see the next point before calling it an alpha bug.
- **The CPU rasteriser draws the ground on the same timeline.** `RECOMP_PB_GL=0`
  with `RECOMP_PIXEL=100,430` prints `paint` fragments at that pixel
  (`argb FF42054F`, `54A15A43`, ...) and the dumped frame at the landing
  cutscene shows the textured water the player stands on, with ripples,
  reflections and lanterns standing in it. So the executor and the data can
  produce the floor; the two back ends disagree about this pixel.
- **Not the render targets.** `RECOMP_TGT_LOG=1` lists four colour offsets the
  title ever renders into: `00404000`, `00530000`, `00D62000` (flip 2083) and
  `00CCC000` (flip 4701). `80AD5000` — the 640x480 surface the water and the
  movie sample — is **never** a draw target, so its constant grey cannot be
  fixed by a render-target path; it arrives by a copy or a CPU write.
- **Next measurement (narrow).** The GL probe needs to print, for the triangle
  it finds, the interpolated `v0`/`st0` and the stage-0 texel the CPU sampler
  would return, so the alpha chain can be compared with the CPU's `[PIXEL]`
  alpha for the same fragment. If they agree (both ~0), the CPU's painted
  pixel comes from a different, full-frame batch and the search moves to the
  batches the CPU paints with `box 0..640 0..480`; if they disagree, the GL
  combiner is evaluating this configuration differently from the CPU's.
- **Operational note.** A capture started as a plain background job is killed
  when the agent's shell is cleaned up (SIGTERM at ~70-90 s, which looks
  exactly like "the title never gets past PRESS START"). `setsid nohup ...
  < /dev/null &` keeps it alive to the stage; every stage capture in this
  session after that ran that way.

## 2026-11-02 — the ground is one back end's alpha, and the texture decode is not the difference

Continued from the 2026-11-01 finding (the 960-vertex ground mesh paints
nothing on the OpenGL back end and paints under `RECOMP_GL_ALPHA_ONE`).

- **The data is exonerated.** `nv2a_backend_decode_texture()` — the whole-texture
  decode the GL back end uploads — is a loop over `sample_texture()`, the CPU
  rasteriser's own sampler. Both back ends are therefore handed byte-identical
  texels for the ground's stage 0; a decode difference cannot be the cause. The
  same argument covers the combiner words, the vertex diffuse and the alpha
  test's reference, which `apply_state()` normalises (`alpha_ref & 0xFF` over
  255) exactly as the CPU's `alpha_test_pass()` compares them. What is left is
  the fragment path of one back end, not the inputs to it.
- **A real convention gap found on the way, and fixed.** Stage coordinates reach
  a back end in two different units and the format decides which: the CPU's
  `rc_texel()` multiplies by the size only for `tex_size_from_format()` formats
  (swizzled, DXT) and leaves a linear format's coordinates in **texels**;
  `fetch_texcoord()` says the same thing in words. The GL shader passed both
  through `texture(c.xy / q)`, which wants [0,1], so every **linear** texture
  wrapped and sampled the wrong corner. `gl_vert_fill()` now divides a stage
  whose texture has a pitch (the title programs none for swizzled or DXT, which
  have no rows) by that texture's size. The 640x480 linear movie surface is the
  headline victim of this bug — it is the "black attract movie / two coloured
  strips" entry — and the ground's DXT5 page is *not*, so this fix is recorded
  as the movie's, pending a capture that shows it.
- **Captured the stage on the GL back end** for `docs/img/gl-stage.png`
  (`floor-walk`, stride 20, 952 frames): the player model, bamboo, haze and HUD
  draw over the clear colour, with no ground under the player. That is the
  honest current state, and the picture the open entry describes.
- **Docs:** `MSVC_2022.md` was purged. Its build and boot record had already
  been taken into `KNOWN_ISSUES.md` and its prerequisites into `RUNNING.md`,
  and `README.md` carries the warning-wall and `/bigobj` notes; the file was a
  historical checklist with nothing left that no other page says.
