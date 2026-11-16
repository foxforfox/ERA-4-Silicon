#!/bin/zsh
# Make Logic Pro load freshly built Audio Units: quit Logic, move its plugin
# validation list and the system AU cache aside (kept as a backup, not deleted),
# restart the AU registrar, then reopen the project given as $1 (optional).
set -u
BACKUP="${TMPDIR:-/tmp}/logic-au-cache-backup/$(date +%Y%m%d-%H%M%S)"

osascript -e 'tell application "Logic Pro" to quit' 2>/dev/null
for i in {1..30}; do pgrep -x "Logic Pro" >/dev/null || break; sleep 1; done
if pgrep -x "Logic Pro" >/dev/null; then echo "Logic Pro is still running (unsaved changes?)" >&2; exit 1; fi

mkdir -p "$BACKUP"
for f in ~/Library/Preferences/com.apple.logic.pro.cs \
         ~/Library/Caches/AudioUnitCache/com.apple.audiounits.cache \
         ~/Library/Caches/AudioUnitCache/com.apple.audiounits.sandboxed.cache; do
  [[ -e "$f" ]] && mv "$f" "$BACKUP/"
done
killall -9 AudioComponentRegistrar 2>/dev/null
sleep 1
echo "Backed up to $BACKUP"
auval -a 2>/dev/null | grep "ERA 4.5"

[[ $# -ge 1 ]] && open "$1"
