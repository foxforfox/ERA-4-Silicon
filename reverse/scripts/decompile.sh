#!/bin/zsh
# Decompile one ERA 4 plugin's x86_64 slice with Ghidra (headless) into reverse/<Plugin>/.
#   reverse/scripts/decompile.sh PlosiveRemover
# The Ghidra project (large) stays local in ~/Library/Caches/era-ghidra; only the
# decompiled .c and vtable list land in ~/Flex.
set -eu
P="$1"; HERE="${0:A:h}"; OUT="$HERE/../$P"; WORK=~/Library/Caches/era-ghidra
export JAVA_HOME="$(brew --prefix openjdk@25)/libexec/openjdk.jdk/Contents/Home"
mkdir -p "$OUT" "$WORK/bin" "$WORK/projects"
SRC=~/Library/Audio/Plug-Ins/Components/ERA4_$P.component/Contents/MacOS/ERA4_$P
if lipo -info "$SRC" | grep -q "^Architectures in the fat file"; then lipo "$SRC" -thin x86_64 -output "$WORK/bin/ERA4_$P"
else cp "$SRC" "$WORK/bin/ERA4_$P"; fi   # already a single x86_64 slice
"$(brew --prefix ghidra)/libexec/support/analyzeHeadless" "$WORK/projects" "ERA4_$P" \
  -import "$WORK/bin/ERA4_$P" -overwrite \
  -scriptPath "$HERE" -postScript ExportDecompiled.java "$OUT" \
  -analysisTimeoutPerFile 7200 > "$WORK/$P.log" 2>&1
ls -la "$OUT"
