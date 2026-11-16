#!/bin/zsh
# Plays the same vocal through an ERA 4 original and its ERA 4.5 rebuild in real time
# and saves each editor as a PNG, so their activity displays can be compared.
#   display-compare.sh <ERA4 name> "<ERA 4.5 name>" <vocal.wav> [start s] [wait ms] [out dir]
#   display-compare.sh DeEsserPro "ERA 4.5 De-Esser Pro" vocal.wav 2 9000
set -u
ROOT="${0:A:h}/../.."
SNAP="${AU_SNAPSHOT:-$ROOT/build-tools/au-snapshot/au-snapshot_artefacts/Release/au-snapshot.app/Contents/MacOS/au-snapshot}"
C=~/Library/Audio/Plug-Ins/Components
OLD="$1"; NEW="$2"; VOCAL="$3"; START="${4:-2}"; WAIT="${5:-9000}"; OUT="${6:-${TMPDIR:-/tmp}/era-display-compare}"
mkdir -p "$OUT"
arch -x86_64 "$SNAP" "$C/ERA4_$OLD.component" "$OUT/${OLD}_40.png" --feed "$VOCAL" start "$START" --wait "$WAIT" --scale 2 &
"$SNAP" "$C/$NEW.component" "$OUT/${OLD}_45.png" --feed "$VOCAL" start "$START" --wait "$WAIT" --scale 2 &
wait
