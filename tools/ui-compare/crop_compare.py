#!/usr/bin/env python3
"""Stack the same region of two captures, reference on top, for a close look.

    crop_compare.py reference.png ours.png out.png x0 y0 x1 y1 [zoom]

Coordinates are editor points; zoom enlarges the result without smoothing.
"""
import sys
from PIL import Image

ref = Image.open(sys.argv[1]).convert("RGB")
ours = Image.open(sys.argv[2]).convert("RGB")
points_wide = 800 if (ref.width % 800 == 0 and ref.height % 640 == 0) else 300
scale = ref.width / points_wide
box = tuple(int(float(v) * scale) for v in sys.argv[4:8])
zoom = int(sys.argv[8]) if len(sys.argv) > 8 else 1

top, bottom = ref.crop(box), ours.crop(box)
out = Image.new("RGB", (top.width, top.height * 2 + 6), (255, 0, 255))
out.paste(top, (0, 0))
out.paste(bottom, (0, top.height + 6))
out.resize((out.width * zoom, out.height * zoom), Image.NEAREST).save(sys.argv[3])
