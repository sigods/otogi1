# Known issues

One entry per open blocker. Each records the symptom as it appears in a log, the evidence behind it, the current status, and where a fix belongs. Milestone status is in [`../ROADMAP.md`](../ROADMAP.md); the raw history is in [`BOOT_LOG.md`](BOOT_LOG.md).

## The stage's lit geometry was drawn inside-out, torn and painted the fog colour

Closed on 2026-10-28 in three parts; kept because each symptom pointed at a

different fault and the first two fixes only looked like each other.

- **Symptom:** on the start-game stage the bamboo trunks read as hollow black
  wedges with a green rim, the player model as a dark inside-out silhouette,
  and the ground was the same blue as the sky. In close-up cutscene shots the
  player tore into spiky triangles instead of a smooth model. The HUD, the
  leaves, the haze planes and the light shafts drew.
- **The inside-out half (fixed):** the OpenGL back end mirrored `SET_FRONT_FACE`
  in `apply_state()`; mirrored, every closed mesh kept its back faces (the
  inside). It now maps 0x0900/0x0901 straight through.
- **The torn half (fixed):** `NV_PRIM_TRIANGLE_STRIP` was decomposed as three
  consecutive vertices in ascending order. The hardware reverses every other
  triangle, so every odd one faced backwards -- invisible where nothing culls,
  and a mesh shredded into its flipped triangles wherever the title does.
  `RECOMP_CULL_LOG` shows Otogi's model arriving as strip batches with culling
  on, alternating cull-front and cull-back.
- **The blue wash half (fixed):** the fog unit subtracted a constant from
  `fog_param0 + coord * fog_param1`. Otogi enables fog and leaves both params
  at 1.0, which saturates to "no fog" on hardware; the subtraction turned the
  program's own `oFog` into the fog factor instead, and painted the ground and
  the foliage with the fog colour. `RECOMP_FOG_TRACE` caught it:
  `[FOG] mode 2601 en 1 px 1 py 1 d -nan -> f 1`. The linear path no longer
  subtracts anything. The exponential paths still do (see `BOOT_LOG.md`).
- **Verification:** the stage now draws dark green foliage, teal trunks and a
  textured dark ground under the haze band, with the player model whole in
  close-ups, culling on.
- **Where the fixes belong:** `apply_state()` in `src/nv2a/nv2a_pb_gl.c`, and
  the primitive decompositions and `fog_factor()` in
  `src/kernel/nv2a_pb_exec.c`.

## Closed on 2026-10-28 — the effect planes drew as hard-edged wedges

- **Symptom (history):** on the start-game stage the light shafts and cloud
  cards rendered as translucent wedges with visible straight edges, where the
  CPU rasteriser's captures of the same stage show soft haze.
- **Cause:** the executor's texture sampler had no case for `D3DFMT_G8B8`
  (0x28) and no linear twin for it, so the 256x256 effect texture failed to
  decode and the OpenGL back end bound white — an opaque quad where the
  title's combiner wants that texel's own alpha. `RECOMP_TEX_DUMP` and the
  `[PBGL] … will not decode` line named it.
- **Fix:** `sample_tex()` and `linear_twin()` in `src/kernel/nv2a_pb_exec.c`.
- **Verification:** a full stage run no longer logs a texture that will not
  decode.

## The title's movie draws black, or as two coloured strips

- **Symptom:** the title's only XMV shows nothing usable on the OpenGL back
  end. The looping attract movie is black; the pre-stage Raikoh cutscene draws
  its movie quad as **two full-height strips, one pure green and one pure
  yellow**, over the bamboo. The CPU rasteriser plays both correctly (no black
  frame, no strips), and that is the current differential.
- **Evidence:** the title opens the file and reads it throughout; it binds a
  640x480 YUY2 texture (`0x24`) and a 640x480 `LIN_X8R8G8B8` (`0x1E`) one, and
  decodes each frame into the same surface. `RECOMP_GL_FLAT` over the attract
  period is a uniform magenta field, so the full-screen quad is submitted and
  passes every gate, and its painted answer is black. `TEXUSE` records the
  640x480 linear texture being sampled by hundreds of batches per frame
  (`BOOT_LOG.md`, 2026-10-28). The strips appear at t≈45-50 s and again at
  ≈190 s of a `RECOMP_PAD_SCRIPT` run, i.e. exactly where the pre-stage movie
  plays; the same build with `RECOMP_PB_GL=0` draws that cutscene correctly
  (maintainer report, 2026-10-28).
- **Two separate faults, most likely:** (1) the pixels: the CPU sampler
  re-decodes the texture from guest memory on every draw, while the GL back
  end caches it by offset/size/format and uploads once, so an in-place movie
  surface stays black/stale (`RECOMP_PB_GL_TEXREFRESH` did not lift the attract
  movie, which suggests the refresh path misses this bind, not that the cache
  is innocent); (2) the quad: green/yellow strips instead of a frame mean the
  vertices or the texture coordinates handed to GL for that batch are not the
  ones the CPU path uses -- the colours are a 32-bit read of a half-written
  surface, not a blend error.
- **Status:** open, not scheduled. It is the title's only movie, so it can be
  skipped for gameplay work (the START taps a pad script issues do exactly
  that); the pre-stage movie is skipped the same way.
- **Where the fix belongs:** the OpenGL back end's texture cache and the
  screen-space batch path (`src/nv2a/nv2a_pb_gl.c`), or the executor's texture
  state if the batch is handed the wrong buffer. A dirty-page hook on guest
  writes to the texture's range is the general fix for (1).

## A depth patch on the title screen

- **Symptom:** a rectangular region over the "PRESS START" area shows a
  polygon pattern that is not part of the artwork, over the falling petals.
- **Evidence:** maintainer report, 2026-10-28, on both the CPU rasteriser and
  the OpenGL back end. `RECOMP_GL_FLAT` draws the title screen as pure
  magenta — every pixel of it is geometry that passes every gate — so the
  patch is depth, not missing or extra geometry.
- **Status:** open, not investigated. `RECOMP_GL_NO_DEPTH=1` bisects it, and
  `RECOMP_GL_STATE=<n>` prints the depth state of the batches involved.
- **Where the fix belongs:** unknown until the depth state of the petals'
  pass and of the pass it occludes have been printed.

## Closed on 2026-10-27 — the stage world was not drawn

- **Symptom (history):** on the start-game stage the player's 3D model, the HUD
  and the bamboo were drawn; the rest of the stage world was not. Large flat
  polygons showed where terrain and props should be. Recorded from maintainer
  runs on 2026-10-24/26.
- **Cause:** two separate gaps, both since closed. The batches were reaching the
  rasteriser and being dropped, and the reason recorded here ("geometry that
  collapses") turned out on 2026-10-27 to be the title's own culling: 6771 of
  6776 all-dropped batches lose every triangle to the eye/near-plane test, the
  position buffers hold real data, and the few on-screen ones carry a
  `SET_VERTEX_DATA_ARRAY_FORMAT` of stride 0 — a deliberate no-op strip (see
  `BOOT_LOG.md`, 2026-10-27). What actually hid the world was the pixel stage:
  the OpenGL back end drew `diffuse * stage 0` where the title's register
  combiners say otherwise, and the back end now evaluates them.
- **Verification:** `docs/img/gl-stage.png` — bamboo grove, ground haze, light
  shafts, HUD and the player's model, from a headless `RECOMP_PAD_SCRIPT` run
  through the OpenGL back end.
- **Residual:** the effect planes' hard edges, above.

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
