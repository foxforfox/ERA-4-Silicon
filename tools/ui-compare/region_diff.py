#!/usr/bin/env python3
"""Compare two captures of a plugin editor region by region.

    region_diff.py reference.png ours.png diff_out.png [name=x0,y0,x1,y1 ...]

Both PNGs come from tools/au-snapshot at the same --scale. Regions are in
editor points. For each region it prints the mean pixel difference now, and
the shift of OURS (in points, to a quarter point at --scale 4) that would
match the reference best: a non-zero shift means "move that element by this
much"; a difference that no shift removes means its size or colour is wrong.
Also prints the ten 10 pt blocks that differ most, and writes a diff image
(differences in red over a dimmed reference). Needs numpy and Pillow.
"""
import sys
import numpy as np
from PIL import Image

ref = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(np.int32)
ours = np.asarray(Image.open(sys.argv[2]).convert("RGB")).astype(np.int32)
height = min(ref.shape[0], ours.shape[0])
width = min(ref.shape[1], ours.shape[1])
ref, ours = ref[:height, :width], ours[:height, :width]

# The De-Esser Pro window is 800 x 640 points, the others 300 x 420.
points_wide = 800 if (width % 800 == 0 and height % 640 == 0) else 300
scale = width // points_wide

diff = np.abs(ref - ours).max(axis=2)
print("total: mean abs diff %.3f, pixels differing by more than 24: %d (%.2f%%)"
      % (diff.mean(), (diff > 24).sum(), 100 * (diff > 24).mean()))

picture = np.zeros((height, width, 3), np.uint8)
picture[..., 0] = np.clip(diff * 3, 0, 255)
picture[..., 1] = picture[..., 2] = ref.mean(axis=2) // 4
Image.fromarray(picture).save(sys.argv[3])

reach = 4 * scale   # search +/- 4 points
for spec in sys.argv[4:]:
    name, box = spec.split("=")
    x0, y0, x1, y1 = [int(float(v) * scale) for v in box.split(",")]
    target = ref[y0:y1, x0:x1]
    now = np.abs(target - ours[y0:y1, x0:x1]).mean()
    best = (1e9, 0, 0)
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            if y0 - dy < 0 or x0 - dx < 0 or y1 - dy > height or x1 - dx > width:
                continue
            error = np.abs(target - ours[y0 - dy:y1 - dy, x0 - dx:x1 - dx]).mean()
            if error < best[0] - 1e-9:
                best = (error, dx, dy)
    print("%-14s now %.2f  best %.2f at shift dx=%+.2f dy=%+.2f pt"
          % (name, now, best[0], best[1] / scale, best[2] / scale))

block = 10 * scale
worst = sorted(((diff[y:y + block, x:x + block].mean(), x // scale, y // scale)
                for y in range(0, height, block) for x in range(0, width, block)), reverse=True)
print("worst 10 pt blocks:", " ".join("(%d,%d):%.1f" % (x, y, mean) for mean, x, y in worst[:10]))
