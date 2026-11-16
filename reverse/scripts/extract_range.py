"""Pull the decompiled functions whose entry address lies in [start, end) out of a
Ghidra export (decompile.sh output) into a smaller file, with each one's line count.
Usage: python3 -I extract_range.py <export.c> <start hex> <end hex> <out.c>"""
import sys, re
src, start, end, out = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16), sys.argv[4]
keep, sizes, cur = [], [], None
for line in open(src, errors="replace"):
    m = re.match(r"// ==== ([0-9a-f]+)\s+(\S+)", line)
    if m:
        cur = int(m.group(1), 16)
        if start <= cur < end: sizes.append([m.group(1), m.group(2), 0])
    if cur is not None and start <= cur < end:
        keep.append(line); sizes[-1][2] += 1
open(out, "w").writelines(keep)
for a, n, c in sizes: print(a, n, c)
