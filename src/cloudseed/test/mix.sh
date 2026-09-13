#!/bin/bash
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
"${CXX:-g++}" -O2 -std=c++14 -Wall -Wextra -Werror -ffp-contract=off \
  -I "$HERE/.." -I "$ROOT/lib/cloudseed-daisy/src" -I "$ROOT/lib/libDaisy/src" \
  "$HERE/mix.cpp" "$ROOT/lib/libDaisy/src/hid/ctrl.cpp" \
  "$ROOT/lib/cloudseed-daisy/src/cloudseed/fdlibm_trig.cpp" -o "$WORK/mix"
"$WORK/mix"
