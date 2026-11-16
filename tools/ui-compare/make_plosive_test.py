"""Mix synthetic mic pops (plosive air blasts) into a vocal, for testing plosive removers.

Each pop is low-passed noise plus a ~45 Hz half-cycle thump, 60 ms long with a fast
attack, at roughly the vocal's peak level, placed every `--every` seconds.
Usage: python3 make_plosive_test.py in.wav out.wav [--every 1.1] [--start 2.5] [--level 0.5]
Prints the pop times so displays can be checked at those moments.
"""
import sys, wave, argparse
import numpy as np
from scipy.signal import butter, sosfilt

ap = argparse.ArgumentParser()
ap.add_argument("inp"); ap.add_argument("out")
ap.add_argument("--every", type=float, default=1.1)
ap.add_argument("--start", type=float, default=2.5)
ap.add_argument("--level", type=float, default=0.5)
a = ap.parse_args()

with wave.open(a.inp) as w:
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
if width == 3:
    b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
    x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608.0
elif width == 2:
    x = np.frombuffer(raw, np.int16) / 32768.0
else:
    x = np.frombuffer(raw, np.int32) / 2147483648.0
x = x.reshape(-1, ch).astype(np.float64)

rng = np.random.default_rng(1)
length = int(0.06 * rate)
t = np.arange(length) / rate
env = (1 - np.exp(-t / 0.003)) * np.exp(-t / 0.018)
noise = sosfilt(butter(4, 120, "low", fs=rate, output="sos"), rng.standard_normal(length))
pop = noise / np.abs(noise).max() * 0.6 + np.sin(2 * np.pi * 45 * t) * 0.8
pop = pop * env / np.abs(pop * env).max() * a.level

times = []
pos = a.start
while pos * rate + length < len(x):
    i = int(pos * rate)
    x[i:i + length] += pop[:, None]
    times.append(round(pos, 2))
    pos += a.every
x = np.clip(x, -1, 1)
with wave.open(a.out, "wb") as w:
    w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes((x * 32767).astype(np.int16).tobytes())
print("pops at", times)
