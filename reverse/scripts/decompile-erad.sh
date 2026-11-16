#!/bin/zsh
# Decompile ERA-D plugin (different naming convention from ERA4_* plugins).
set -eu
P="ERA-D"; HERE="${0:A:h}"; OUT="$HERE/../$P"; WORK=~/Library/Caches/era-ghidra
export JAVA_HOME="$(brew --prefix openjdk@25)/libexec/openjdk.jdk/Contents/Home"
mkdir -p "$OUT" "$WORK/bin" "$WORK/projects"
SRC=~/Library/Audio/Plug-Ins/Components/ERA-D.component/Contents/MacOS/ERA-D
if lipo -info "$SRC" | grep -q "^Architectures in the fat file"; then lipo "$SRC" -thin x86_64 -output "$WORK/bin/ERA-D"
else cp "$SRC" "$WORK/bin/ERA-D"; fi
"$(brew --prefix ghidra)/libexec/support/analyzeHeadless" "$WORK/projects" "ERA-D" \
  -import "$WORK/bin/ERA-D" -overwrite \
  -scriptPath "$HERE" -postScript ExportDecompiled.java "$OUT" \
  -analysisTimeoutPerFile 7200 > "$WORK/ERA-D.log" 2>&1
ls -la "$OUT"
