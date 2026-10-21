#!/usr/bin/env bash
# Regenerate the lifted C from your own disc image.
# Usage: ./regen.sh [--disasm]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLKIT="${XBOXRECOMP_DIR:-$ROOT/../xboxrecomp}"
XBE="$ROOT/game/default.xbe"
ANALYSIS="$ROOT/game/otogi1_analysis.json"
GEN_DIR="$ROOT/src/game/recomp/gen"

if [[ ! -f "$XBE" ]]; then
  echo "error: $XBE not found. Unpack your disc image into game/ first." >&2
  exit 1
fi

cd "$TOOLKIT"

echo "==> xbe_parser"
py -3 -m tools.xbe_parser "$XBE" --json "$ANALYSIS"

echo "==> disasm"
py -3 -m tools.disasm "$XBE" --text-only -v

echo "==> func_id"
py -3 -m tools.func_id "$XBE" -v

echo "==> abi_analysis  (never skip: silent wrong signatures otherwise)"
py -3 -m tools.abi_analysis "$XBE" -v

echo "==> recomp  (--split 250: prevents OOM)"
py -3 -m tools.recomp "$XBE" --all --split 250 --gen-dir "$GEN_DIR"

echo "==> done. Now: cmake --build build-xr --config Release"
