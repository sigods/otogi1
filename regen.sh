#!/usr/bin/env bash
# Regenerate the lifted C from your own disc image.
# Usage: ./regen.sh [--disasm]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLKIT="${XBOXRECOMP_DIR:-$ROOT/xboxrecomp}"
XBE="$ROOT/game/default.xbe"
ANALYSIS="$ROOT/game/otogi1_analysis.json"
GEN_DIR="$ROOT/src/game/recomp/gen"

if [[ ! -f "$XBE" ]]; then
  echo "error: $XBE not found. Unpack your disc image into game/ first." >&2
  exit 1
fi

cd "$TOOLKIT"

echo "==> xbe_parser"
python3 -m tools.xbe_parser "$XBE" --json "$ANALYSIS"

echo "==> disasm"
python3 -m tools.disasm "$XBE" --text-only -v \
  --seed-functions "$ROOT/config/seed_functions.json" \
  --extra-sections D3D,D3DX,DSOUND,XGRPH,XMV,XPP

echo "==> func_id"
python3 -m tools.func_id "$XBE" -v

echo "==> abi_analysis  (never skip: silent wrong signatures otherwise)"
python3 -m tools.abi_analysis "$XBE" -v

echo "==> recomp  (--split 250: prevents OOM)"
python3 -m tools.recomp "$XBE" --all --split 250 --gen-dir "$GEN_DIR" \
  --exclude-manual "$ROOT/src/game/recomp_manual.c"

echo "==> done. Now: cmake --build build-linux"
