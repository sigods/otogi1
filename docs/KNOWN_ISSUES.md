# Known issues

One entry per open blocker. Each records the symptom as it appears in a log, the evidence behind it, the current status, and where a fix belongs. Milestone status is in [`../ROADMAP.md`](../ROADMAP.md); the raw history is in [`BOOT_LOG.md`](BOOT_LOG.md).

## The stage's floor never reaches the screen

Open. The start-game stage draws the bamboo, the haze, the light shafts, the
HUD and the player, and no floor under him: the pixels where the ground should
be are exactly the clear colour (`srgb(0,50,100)`), not a fogged or culled
surface.

- **Measured on 2026-10-29:** `RECOMP_GL_FLAT=1` paints the lower half magenta,
  but that is a full-screen effect/screen pass (FLAT skips the alpha test), not
  the terrain. `RECOMP_GL_NO_BLEND=1` fills the frame with a grey rock image
  that sits above the player and the bamboo with the wrong position and
  texture -- a screen-space pass drawn opaque, not the ground. The new
  `RECOMP_GL_PROBE=320,440` does the point-in-triangle test and finds **no**
  terrain batch covering the pixel: the only hit is a 525-vertex program batch
  at z 9.88e9 (past the depth divisor), which cannot paint. `NO_DEPTH` and
  `NO_CULL` do not restore the floor.
- **What it is not:** not the fog unit (the factor is 1 on the stage), not
  culling, not the depth buffer, and not the clear rectangle. The ground is
  either never submitted or loses every triangle in the executor.
- **Not the dropped batches (2026-10-29):** `RECOMP_XF_DROP=<n>` reports the
  fixed-function batches thrown away as "not screen-space", with the reason and
  the transform context. All sampled drops on a `to-stage` run are
  `fetch-failed` -- stride-0 vertex arrays the title submits as deliberate
  no-op strips (the 2026-10-27 census), not terrain waiting on a transform. So
  the 68,601 "skipped as not screen-space" batches in a 150 s stage run are
  those strips; do not chase a composite matrix for them.
- **The transform, exonerated (2026-10-29, second pass):** the program and
  its constants are correct end to end — `clip = (dot c[112], c[113],
  c[114])`, divide by `dot c[118]` (== `c[115]` == clip w), viewport
  `c[58]/c[59]`; a flagged vertex's outputs recompute from the printed rows to
  four digits. The projection is `z/w = 1 - 100/w` (near 100, far infinity),
  which is also why rows c[114]/c[115] look near-identical. Ground under a
  close camera legitimately straddles the eye; z past far on those vertices is
  arithmetic, not corruption. A/B'd `RECOMP_CLIP_ZFIRST` and
  `RECOMP_CLIP_ZONLY` against the default on ground frames: **no visible
  difference** — the `w=1e-3` clip does produce the probe's 9.88e9 z (x1000),
  but re-cutting those triangles paints nothing, so it is a secondary
  artifact.
- **The gate is fragment alpha (2026-10-29):** `RECOMP_GL_ALPHA_ONE` (forces
  `c.a = 1` after the combiner) makes the ground mesh **appear** (pale blue),
  while `RECOMP_GL_NO_COMB=1` (modulate fallback) keeps it **invisible** — the
  fallback alpha is the same product. Decoded from the terrain batch's
  combiner words: `final alpha = r0.a = v0.a * t0.a`, `v0.a` defaults to 1 in
  `nv2a_vsh_run`, so **fragment alpha = the stage-0 texture's alpha plane**,
  blended `SRC_ALPHA/INVSRC_ALPHA` with alpha test `GREATER 0`. The terrain's
  stage-0 textures sample as `alpha 0..201 (17/25 points zero), red 206..239`
  — plausible colour, near-empty alpha, which is exactly "invisible normally,
  opaque under FLAT/ALPHA_ONE".
- **Next measurement (narrow):** *why is that alpha plane empty* — does the
  texture's guest memory really hold alpha-0 (title-side write path: render
  target writeback or a clear missing the A byte), or does a sampler read the
  wrong bytes? A `RECOMP_PB_GL=0` capture on the same pad splits it (CPU
  visible = GL upload bug; CPU invisible = data/upstream), and
  `RECOMP_XF_BAD` now prints each stage's sampled alpha range per flagged
  batch to name the texture.
- **2026-10-30 — the "empty terrain alpha" is a petal sprite (withdrawn).**
  `RECOMP_TEX_DUMP` on a stage run names the texture behind the 2026-10-29
  `alpha 0..201` reading: `814C2000` 32x64 fmt 06 is a single blossom petal on
  a transparent field, and `826C1000` is the HUD medallion, `807D4000` the font
  page, `817D3000` the SEGA logo, `81966000` the level-select thumbnails -- all
  decode correctly, so the samplers are fine and the sprite alpha is what that
  number measured. The terrain is not gated by it. The register-combiner decode
  was re-verified field for field against xemu's
  `hw/xbox/nv2a/pgraph/glsl/psh.c` (`parse_combiner_inputs`, `parse_input`, the
  final stage `d + mix(c, b, a)`, alpha `final_inputs_1 >> 8`) and matches.
  The open lead is the one batch that does cover the whole frame: a
  2880-triangle mesh sampling `80AD5000`, a 640x480 guest **surface**, whose
  dump is flat grey -- either a screen copy the runtime never fills or an
  off-screen pass whose colour surface is a different VA.
- **2026-10-30 — back end exonerated, and the wedges are a separate fault.**
  The GL vertex shader was clamping window z to [0,1] before rebuilding clip
  space, so geometry past the far plane was pinned to the far plane instead of
  being clipped; the clamp is gone and the whole-frame solid-red captures
  dropped from 0.975 to 0.35 of the frame, with the stage still rendering. The
  remaining coloured wedges (blue/green/red bands with straight edges) are one
  huge triangle carrying R, G, B at its vertices, not the terrain: they want a
  `RECOMP_FRAME_TRACE` armed on a frame where the wedge is on screen, to name
  the batch. The floor is missing on the CPU rasteriser too (`RECOMP_PB_GL=0`,
  maintainer observation), so nothing in `nv2a_pb_gl.c` can be its cause.
- **Two instruments were aimed at the wrong moment (2026-10-30, third pass —
  corrected).** `RECOMP_GL_BATCH=64,1100,1300` printed nothing, but flips
  1100-1300 are the attract movie: the stage begins around flip 3500 in these
  runs. `RECOMP_DROP_AS_PROGRAM=1` (run the loaded vertex program on a batch
  that would otherwise be discarded as "not screen-space", `nv2a_pb_exec.c`)
  likewise produced no floor, but its frames were taken before the stage and
  its negative is not established. Both instruments now take
  `RECOMP_LOG_AFTER=<seconds>` (wall time, not flip number), and the pad
  timeline keeps playing past the stage: the earlier one stopped at 125 s,
  where the title puts up its own Retire prompt and no further input arrives,
  so every "stage" frame after that was a static screen. `RECOMP_TEX_DUMP`
  does decode all 64 stage textures correctly (menu, player, bamboo, gravel,
  HUD, shafts), so the sampler is not the fault.
- **The stage-era census is still missing (2026-10-30):** the two drop
  censuses taken today never reached the stage — every capture with the
  `to-stage` timeline stalled in the attract movie, whose GL mode histogram is
  `0 x92562 1 x30606` at frame 3000 where a stage run reports
  `0 x1273072 1 x726797 4 x161805 6 x270`. So the "0 batches skipped as not
  screen-space" reading describes the movie, and the 2026-10-29 conclusion
  above ("not the dropped batches") still rests on runs whose stage arrival is
  not established. `RECOMP_XF_DROP_AT` / `RECOMP_BATCH_CLASS_AT` now take a
  starting batch id so the census can be aimed at the stage; that measurement
  is the next step, and it needs a capture that actually arrives.
- **2026-10-30 — the near clip is not the cause, and a per-fragment probe now exists.**
  `RECOMP_CLIP_NEAR_Z=0` (clip at `w = 1e-3` only, no near-plane cut) against
  the default on the same ground-facing timeline: the floor is missing in both
  runs (`BOOT_LOG.md`, 2026-10-30, "the near-plane clip is not what hides the
  floor"). `RECOMP_GL_PROBE` spends its record budget in the front end unless
  its fourth field is a starting flip -- `RECOMP_GL_PROBE=320,440,60,1,1500`
  reports the landing cutscene's effect pass, so aim it past the cutscene.
  `RECOMP_PIXEL=x,y` (`nv2a_pb_exec.c`) logs every fragment of one pixel with
  the gate that dropped it (depth / alpha test / painted) plus the combiner,
  texture and w it was drawn with; it runs on the CPU rasteriser path only.
  That probe, armed on a run that reaches the stage, is the next measurement.
- **2026-10-30 — visibility queries answered 0 whenever a back end drew.**
  `ZPASS_PIXEL_CNT` (`GET_REPORT`) was fed only by the CPU rasteriser's own
  counter, so with `RECOMP_PB_GL` on it stayed 0 and every visibility test the
  title took read back "nothing passed" -- an instruction to cull. The GL back
  end now counts `GL_SAMPLES_PASSED` per batch while the title has counting
  enabled, and `GET_REPORT` answers the CPU count plus the back end's.
  `RECOMP_ZPASS_FORCE=<n>` overrides the answer, which is the bisect: the floor
  drawn with a large `n` and not without it means the title culls on the query.
  `docs/BOOT_LOG.md`, 2026-10-30. The stage-walk pad also aimed the camera at
  the canopy (`rdown`), not the floor; it uses `rup` now, which is the input
  that pitches the view down.
- **2026-11-01 — the batch is named, and the two back ends disagree.** The
  floor class is a **960-vertex / 933-triangle** mesh (`cull 0`, depth write
  off, `blend 302/1`, alpha test `GREATER 0`) whose combiner is one stage:
  `rgb = 2*t0*v0` into r0, `a = t0.a*v0.a`, final passing r0 through, with a
  128x128 DXT5 at `810AA000` as stage 0. That page's **alpha is populated**
  (244 levels, 17 % zero) under `RECOMP_TEX_DUMP_ALPHA=1`, so an empty page is
  not the cause. On the GL back end the batch paints **nothing** at the probe
  (`pre` == `post`), and `RECOMP_GL_ALPHA_ONE=1` makes it paint
  (`post FFC3C5FF`) -- the fragment alpha is the gate. On the **CPU
  rasteriser** (`RECOMP_PB_GL=0`, `RECOMP_PIXEL=100,430`) the same timeline
  *paints* that pixel and the dumped cutscene frame shows the water with
  ripples, reflections and lanterns standing in it, so the executor and the
  data can produce the floor. Earlier note in this entry that "the floor is
  missing on the CPU rasteriser too" is superseded. Also withdrawn here: the
  batch this entry used to call the terrain (`n 330` at (320,440),
  `824EA000`) is the **player** -- `824EA000` dumps as a character/armour
  atlas and `82586000` as the same atlas in greyscale. `BOOT_LOG.md`,
  2026-11-01.
- **Where the fix belongs:** likely `nv2a_pb_exec.c` (texture decode or
  surface write path) or upstream in the toolkit's texture upload; not the
  vertex transform, which is verified correct. As of 2026-11-01 the two back
  ends disagree about one pixel, so the GL combiner's evaluation of *this*
  configuration (`ctl 00000001`, `stpar 00000001`, stage 0 alpha
  `t0.a*v0.a`) is where the next measurement points.

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
- **Measured on 2026-10-28, and it narrowed things:** a 210 s
  `RECOMP_PAD_SCRIPT` run gated at 42 s caught the black attract frames (`sd`
  0.0127 at R 0.14 / G 2.97 / B 6.16) but no two-band frame in its 28 dumped
  frames, and the 640x480 YUY2 / `LIN_X8R8G8B8` surfaces above were never
  sampled in that window -- so the movie did not draw there and nothing about
  the strips can be measured until a capture contains one. What *was* ruled out
  is the stale-cache half: the one screen-sized surface that is sampled heavily
  (`0x80AD5000`, 640x480 `A8R8G8B8`, up to 3166 batches a frame) dumps uniform
  `0x80808080` under `RECOMP_TEX_DUMP` **on both back ends**, byte for byte, so
  the title's own content there is a constant and the GL cache is not holding
  an old frame. `BOOT_LOG.md`, 2026-10-28, "the screen-sized surface is
  constant on both back ends".
- **It now blocks stage work (2026-10-30):** the movie is no longer just a
  cosmetic gap. Every `to-stage` capture started today sat in the attract
  movie for its whole run (`terr4` read the movie file 752 times in 210 s),
  because the timeline's A presses stop at 85 s and the title reaches the
  movie later than that on this machine. Since the movie draws black on the
  GL back end, the screen cannot say which state a run is in. Fixing the movie
  is now on the critical path to the floor, not beside it.
- **The X8 alpha change did not fix it (2026-10-30):** returning the X byte of
  an `X8R8G8B8` texel as alpha instead of 1.0 was left in the tree for this
  movie; the movie is still black with it in place, so it was reverted rather
  than kept as an unexplained semantic change. `BOOT_LOG.md`, 2026-10-30.
- **Status:** open. It is the title's only movie, so it can be skipped for
  gameplay work when the timeline actually reaches it; the pre-stage movie is
  skipped the same way. As of 2026-10-30 that skip is unreliable.
- **Where the fix belongs:** the OpenGL back end's texture cache and the
  screen-space batch path (`src/nv2a/nv2a_pb_gl.c`), or the executor's texture
  state if the batch is handed the wrong buffer. A dirty-page hook on guest
  writes to the texture's range is the general fix for (1).

## Closed on 2026-10-28 — the full-frame colour panels

- **Symptom (history):** full-screen frames of one flat colour — green, yellow
  red — reported as a broken transition wipe quad, and seen again on the
  start-game stage as a `#003264` (0,50,100) frame carrying only the HUD.
- **Why it is closed:** the panels are the title's own, and both readings say
  so. `RECOMP_GL_FLAT=1` over the same period gives the `#003264` frame with
  magenta *only* where the HUD is: FLAT paints magenta every fragment that is
  submitted and passes depth, cull and alpha, so a screen-filling quad would be
  magenta — and nothing is drawn over the clear colour in that frame. The CPU
  rasteriser, `RECOMP_PB_GL=0`, produces the same frames itself: `sd` exactly 0
  at (0,50,100) for twenty frames, and a perfectly flat (102,53,0) panel before
  them. That is the path this project treats as the reference.
- **What stays open:** the movie's two green/yellow strips are a different
  symptom on a different surface (above), and are not covered by this.
- **Evidence:** `BOOT_LOG.md`, 2026-10-28, "the full-frame colour panels are
  the title's, on both back ends".

## A depth patch on the title screen

- **Symptom:** a rectangular region over the "PRESS START" area shows a
  polygon pattern that is not part of the artwork, over the falling petals.
- **Evidence:** maintainer report, 2026-10-28, on both the CPU rasteriser and
  the OpenGL back end. `RECOMP_GL_FLAT` draws the title screen as pure
  magenta — every pixel of it is geometry that passes every gate — so the
  patch is depth, not missing or extra geometry.
- **Status:** open, investigated on 2026-10-28, not fixed. `RECOMP_GL_STATE=8000`
  over the title window (flips 300-600, 912 sampled batches) splits into 908
  batches with depth testing off and four with it on: three copies of one
  `n 5766 ... depth 1 func 203 wmask 0 ... box -444..1085 134..645` mesh, and
  the only draws that also *write* depth -- `n 216/72 ... wmask 1`, whose boxes
  are **off screen** (`box 284..338 -219..-176`, `303..343 -143..-96`,
  `320..362 -135..-95`). So nothing visible writes depth over the patch: the
  pair to look at is the mesh above, which depth-tests against a buffer almost
  everything else in that window ignores. Also ruled out: the depth divisor,
  which the clear (`zclear_value()`) and the back end (`z_divisor()`) derive
  identically from `SURFACE_FORMAT` bits 4-7 and both map anything that is not
  Z16 to 2^24.
- **Next:** the `[GLST]` record now ends with the batch's screen box, so
  `RECOMP_GL_STATE=8000` on a title run names the rectangle directly; match it
  against the patch on a frame. The back end has **no scissor at all**
  (nothing in `nv2a_pb_gl.c`), and the executor honours `SET_CLEAR_RECT` only
  on swizzled surfaces -- both are candidates for a hard rectangular boundary
  and neither is exercised by a line of today's logs, so the next run should
  count those methods before either is called the cause.
- **Where the fix belongs:** unknown until the box and the frame agree.
  `RECOMP_GL_NO_DEPTH=1` bisects it, and `RECOMP_GL_STATE=<n>` prints the depth
  state of the batches involved (now with their boxes).

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

## MSVC: the input layer calls POSIX `clock_gettime`

- **Symptom:** the MSVC 2022 build stops with
  `xboxrecomp\src\input\keyboard.c(189,27): error C2065: 'CLOCK_MONOTONIC':
  identifier undeclared` and the same error on line 190; nothing else in the
  Windows build fails to compile.
- **Cause:** the `RECOMP_AUTOPAD` headless hook in `xbox_InputKeyboardState()`
  calls `clock_gettime(CLOCK_MONOTONIC, …)` unguarded. MSVC has `struct
  timespec` but neither `clock_gettime()` nor `CLOCK_MONOTONIC`. It is the only
  unguarded site the MSVC build compiles: the other `clock_gettime` calls are
  behind `_WIN32` arms or in POSIX-only sources (`apu_wav.c`, `host_time.c`, the
  SDL2 back ends), and `src/platform/win32_compat.c` is not compiled on `WIN32`
  at all because `platform` is an INTERFACE target there.
- **Workaround used to verify the rest of the path:** a forced-include shim
  outside the tree (`cl /FI shim.h`) mapping `clock_gettime` onto
  `QueryPerformanceCounter`. With it the build is clean — 0 errors, about 100
  benign warnings — and the title boots.
- **Verified with the workaround (2026-10-31, Build Tools 2022 17.14.44, MSVC
  19.44.35229, Windows SDK 10.0.26100.0):** `[AV] SetDisplayMode
  mode=0x080F0D12 format=0x00000012 pitch=2560 fb=0x00404000`, the promo XMV
  read, the main loop still running after 25 s with no access violation,
  `[OHCI0] raised 00000002 -> ISR claimed it` with `RECOMP_USB=1`, and
  non-silent audio through XAudio2 (`[XA2] XAudio2 initialized (48000 Hz stereo
  16-bit …)`, captured WAV peak 19478/32768, 39.1 % non-zero). That is all five
  `tools/smoke.sh` invariants, and the same state the Linux path reaches. Input,
  save creation and the stage's floor are the same open issues as on Linux.
  The prerequisites the docs used to miss (the C++ workload, CMake, 8 GB free,
  the `SizePreCheckEvaluator` exit code) are in [`RUNNING.md`](RUNNING.md).
- **Where the fix belongs:** the `xboxrecomp` toolkit, `src/input/keyboard.c` —
  not this repository. Guard the hook with `#ifndef _WIN32`, or move it onto
  the toolkit's host-time abstraction (`src/platform/host_time.h`).
- **Status:** open. The docs no longer call the path untested: the build and
  boot are verified, and the guard above is the only source change between
  them and a stock checkout.

## Audio effects are not emulated

- **Symptom:** BGM plays, but the DSP effects are absent.
- **Evidence:** [`../README.md`](../README.md) § Status — "The DSP is a passthrough stub, so effects are not emulated". `BOOT_LOG.md`, 2026-10-20, records that the DSP GP/EP stub was not the cause of the earlier silence.
- **Status:** open, not started.
- **Where the fix belongs:** the toolkit's APU emulation.
