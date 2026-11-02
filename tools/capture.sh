#!/usr/bin/env bash
# Capture a named scenario from the recompiled build.
#
#   tools/capture.sh <scenario> [seconds] [outdir]
#
#   tools/capture.sh to-stage 200          # front end -> the stage, standing
#   tools/capture.sh stage-walk 260        # ... and then walk, camera down
#   STRIDE=1 tools/capture.sh to-stage 120 # every flip (about 1 MB each)
#   EXTRA_ENV="RECOMP_GL_NO_COMB=1" tools/capture.sh stage-walk 200
#
# Output goes to ${TMPDIR:-/tmp}/otogi-runs/<scenario> unless a third argument
# names a directory.
#
# The scenario names a pad timeline in tools/pads/<scenario>.txt, in
# RECOMP_PAD_SCRIPT syntax: `<ms>:<button>[+...][:<hold ms>]` from the title's
# first pad read. Two names are composed: `stage-walk` is `to-stage` followed by
# `stage-walk`, because the second only makes sense after the first.
#
# What the run sets, and why each one is not optional:
#
#   RECOMP_PB_EXEC=1     without the push-buffer executor nothing is drawn.
#   RECOMP_USB=1         the pad timeline is delivered through USB OHCI.
#   RECOMP_AC97_READY=1  the front end waits on the APU. Without it the title
#                        stops presenting inside mainmenu_us.med and never
#                        returns -- no error, no crash, a frozen picture while
#                        the kernel counters keep climbing. Two capture runs
#                        were lost to this before it was spotted.
#   RECOMP_FB_DUMP       one BMP per RECOMP_FB_DUMP_STRIDE-th flip, gated on the
#                        `go` file in the output directory, so a capture can be
#                        cut short by deleting it.
#
# Frame dumps are what a run is for: they are the only record of what was on
# screen at a moment. Play them back with
#   ffmpeg -framerate 30 -i runs/<scenario>/f%05d.bmp -pix_fmt yuv420p out.mp4
# and drop the run once it has been read -- at stride 1 a minute is ~1.5 GB of
# tmpfs.
#
# The run also prints the titles' own numbers at the end: flips, frames dumped,
# exit status. A run that exits by signal, or one whose frame count stops while
# its pad time does not, is a finding, not a capture.
set -uo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." >/dev/null && pwd)
BUILD=${BUILD_DIR:-$ROOT/build-linux}
BIN=$BUILD/bin/otogi1
PADDIR=$ROOT/tools/pads

SCEN=${1:-to-stage}
SECS=${2:-200}
# Captures are large (a stride-1 minute is ~1.5 GB), so they land in the
# temporary directory by default: pass an outdir to keep one.
OUT=${3:-${TMPDIR:-/tmp}/otogi-runs/$SCEN}
STRIDE=${STRIDE:-3}

if [ ! -x "$BIN" ]; then
    printf 'capture: no binary at %s (build it, or set BUILD_DIR)\n' "$BIN" >&2
    exit 1
fi

case $SCEN in
    stage-walk) PADFILES="$PADDIR/to-stage.txt $PADDIR/stage-walk.txt" ;;
    *)          PADFILES="$PADDIR/$SCEN.txt" ;;
esac
for f in $PADFILES; do
    if [ ! -f "$f" ]; then
        printf 'capture: no pad timeline %s\n' "$f" >&2
        printf 'capture: scenarios are %s\n' "$(ls "$PADDIR" | sed 's/\.txt//' | tr '\n' ' ')" >&2
        exit 1
    fi
done

# One file for the composed scenarios, since the toolkit reads one script. The
# timelines are documented with `#` lines, and the toolkit's parser stops at the
# first entry it cannot read, so comments and blanks are stripped here rather
# than left to trip it: `script entry without <ms>: at "# Title screen..."`.
PAD=$(mktemp "${TMPDIR:-/tmp}/otogi-pad-XXXXXX.txt")
cat $PADFILES | grep -v '^[[:space:]]*#' | grep -v '^[[:space:]]*$' > "$PAD"
trap 'rm -f "$PAD"' EXIT

rm -rf "$OUT"
mkdir -p "$OUT"
: > "$OUT/go"

export RECOMP_PB_EXEC=1
export RECOMP_USB=1
export RECOMP_AC97_READY=1
# Captures are headless: the emulated APU still runs, but the mixed output
# must not go through a real ALSA device, which stalls a run on a busy box.
# Always the dummy device: a capture is read, not listened to, and a real
# ALSA device stalls the run on a busy machine.
export SDL_AUDIODRIVER=dummy
export RECOMP_PAD_SCRIPT="@$PAD"
export RECOMP_FB_DUMP="$OUT/f"
export RECOMP_FB_DUMP_FLIPS="$OUT/go"
export RECOMP_FB_DUMP_STRIDE="$STRIDE"
export RECOMP_XF_THREADS=${XF_THREADS:-$(( $(nproc) > 8 ? 6 : 3 ))}
# EXTRA_ENV is for a one-off bisect (RECOMP_GL_NO_DEPTH=1 and friends). It is
# split deliberately: it is a list of assignments, not a shell command.
if [ -n "${EXTRA_ENV:-}" ]; then
    for kv in $EXTRA_ENV; do export "$kv"; done
fi

printf 'capture: %s -> %s for %ss (stride %s)\n' "$SCEN" "$OUT" "$SECS" "$STRIDE"
printf 'capture: pads %s\n' "$PADFILES"
timeout "$SECS" "$BIN" > "$OUT/log.txt" 2>&1
rc=$?

frames=$(ls "$OUT"/f*.bmp 2>/dev/null | wc -l)
flips=$(grep -c '\[FB\] frame' "$OUT"/log.txt 2>/dev/null)
lastpad=$(grep -o 'PAD: t=[0-9]* ms' "$OUT"/log.txt 2>/dev/null | tail -1)
printf 'capture: exit %s, %s frames dumped, %s flips, %s\n' \
    "$rc" "$frames" "$flips" "${lastpad:-no pad step}"
if [ "$rc" -ge 128 ]; then
    printf 'capture: stopped by signal %s -- read %s, this is a finding\n' \
        "$((rc - 128))" "$OUT/log.txt" >&2
fi
