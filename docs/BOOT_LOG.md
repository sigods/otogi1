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
