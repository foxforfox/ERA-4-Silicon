#!/bin/zsh
# compare.sh <Name> "<ERA 4.5 component name>" [extra au-snapshot arguments for ours]
#   e.g.  compare.sh DeEsser "ERA 4.5 De-Esser"
#         compare.sh ReverbRemover "ERA 4.5 Reverb Remover" --set 1=0.564286 --set 3=1
#         compare.sh DeEsserPro "ERA 4.5 De-Esser Pro" --set 0=0.428571
# Captures the installed ERA 4.5 Audio Unit at 4x and diffs it against the saved
# capture of the original in reference/orig_<Name>.png. It never launches an
# original ERA 4 plugin. AU_SNAPSHOT must point at the built tools/au-snapshot binary.
here=${0:a:h}
: ${AU_SNAPSHOT:?set AU_SNAPSHOT to the au-snapshot binary (see tools/au-snapshot/CMakeLists.txt)}
name=$1; component=$2; shift 2
regions=regions_compact.txt; [[ $name == DeEsserPro ]] && regions=regions_pro.txt

"$AU_SNAPSHOT" "$HOME/Library/Audio/Plug-Ins/Components/$component.component" \
    "$here/captures/ours_$name.png" --wait 1200 --scale 4 "$@" | grep -E "FAILED|no Audio|could not"
python3 "$here/region_diff.py" "$here/reference/orig_$name.png" "$here/captures/ours_$name.png" \
    "$here/captures/diff_$name.png" $(cat "$here/$regions")
