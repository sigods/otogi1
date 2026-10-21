#!/usr/bin/env bash
# Release smoke test for the v0.1 technical preview.
#
# Builds nothing by default: point it at an existing build directory. It runs
# the title twice (plain, and with the USB model enabled) and asserts the five
# invariants the release notes claim:
#
#   1. the title boots and reaches SetDisplayMode
#   2. it keeps running until the timeout kills it (no early exit, no crash)
#   3. no segmentation fault
#   4. the APU produces non-silent audio (peak > 0 in the captured WAV)
#   5. the OHCI controllers come up only when RECOMP_USB=1
#
# Usage:
#   tools/smoke.sh [build-dir]        (default: build-linux)
#   SMOKE_SECS=20 tools/smoke.sh      (default: 15)
#
# Requires: game/ extracted next to the project, python3 for the WAV check.
set -uo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." >/dev/null && pwd)
BUILD=${1:-${BUILD_DIR:-$ROOT/build-linux}}
BIN=$BUILD/bin/otogi1
SECS=${SMOKE_SECS:-15}

fails=0
ok()   { printf '  ok    %s\n' "$1"; }
bad()  { printf '  FAIL  %s\n' "$1"; fails=$((fails + 1)); }
skip() { printf '  skip  %s\n' "$1"; }

[ -d "$ROOT/game" ] || {
    echo "game/ is missing: extract the disc first (see docs/RUNNING.md)" >&2
    exit 2
}
[ -x "$BIN" ] || {
    echo "$BIN not found: configure and build first (see docs/RUNNING.md)" >&2
    exit 2
}

LOG=$(mktemp)
WAV=$(mktemp --suffix=.wav)
trap 'rm -f "$LOG" "$WAV"' EXIT

echo "smoke: $BIN (${SECS}s per run)"

# ── Run 1: plain boot, audio capture on ────────────────────────────────
(
    cd "$ROOT" || exit 2
    RECOMP_AC97_READY=1 RECOMP_AUDIO_WAV="$WAV" \
        timeout "$SECS" "$BIN" >"$LOG" 2>&1
)
rc=$?

if [ "$rc" -eq 124 ]; then
    ok "ran for ${SECS}s and was stopped by the timeout"
else
    bad "exited early (status $rc)"
fi

if grep -q "SetDisplayMode" "$LOG"; then
    ok "reached [AV] SetDisplayMode"
else
    bad "never reached [AV] SetDisplayMode"
fi

if grep -qiE "segmentation|sigsegv" "$LOG"; then
    bad "log contains a segmentation fault"
else
    ok "no segmentation fault"
fi

if command -v python3 >/dev/null 2>&1; then
    if python3 - "$WAV" <<'PY'
import array, sys, wave

w = wave.open(sys.argv[1])
n = w.getnframes()
a = array.array("h")
a.frombytes(w.readframes(n))
if not a:
    print("      no frames captured")
    sys.exit(1)
peak = max(abs(x) for x in a)
rms = (sum(x * x for x in a) / len(a)) ** 0.5
print(f"      {n} frames, peak {peak}, rms {rms:.0f}")
sys.exit(0 if peak > 0 else 1)
PY
    then
        ok "audio is non-silent"
    else
        bad "audio is silent (peak 0)"
    fi
else
    skip "audio check (python3 not found)"
fi

# ── Run 2: the USB model must be opt-in ────────────────────────────────
if grep -q "OHCI: two controllers" "$LOG"; then
    bad "OHCI came up without RECOMP_USB"
else
    ok "OHCI stays off without RECOMP_USB"
fi

: >"$LOG"
(
    cd "$ROOT" || exit 2
    RECOMP_USB=1 timeout "$SECS" "$BIN" >"$LOG" 2>&1
)
rc=$?

if grep -q "OHCI: two controllers" "$LOG"; then
    ok "OHCI comes up with RECOMP_USB=1"
else
    bad "OHCI did not come up with RECOMP_USB=1 (status $rc)"
fi

if grep -qiE "segmentation|sigsegv" "$LOG"; then
    bad "segmentation fault in the RECOMP_USB run"
else
    ok "no segmentation fault with RECOMP_USB=1"
fi

echo
if [ "$fails" -eq 0 ]; then
    echo "smoke: PASS"
    exit 0
fi
echo "smoke: FAIL ($fails)"
exit 1
