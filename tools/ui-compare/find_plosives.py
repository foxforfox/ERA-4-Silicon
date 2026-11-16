"""Rank WAV files by how many plosive-like pops they contain.

A pop is a short burst where the energy below 150 Hz jumps well above both its own
recent level and the file's typical low-frequency level. Prints, per file, the
number of pops and the times of the strongest ones.
Usage: python3 -I find_plosives.py <folder or wav> [...]
"""
import sys, pathlib, wave
import numpy as np
from scipy.signal import butter, sosfilt

def read_wav(path):
    with wave.open(str(path)) as w:
        n, ch, width, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    if width == 2:   data = np.frombuffer(raw, np.int16).astype(np.float32) / 32768
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        data = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608.0
    elif width == 4: data = np.frombuffer(raw, np.int32).astype(np.float32) / 2147483648
    else: raise ValueError(width)
    return data.reshape(-1, ch).mean(axis=1), rate

def pops(path):
    x, rate = read_wav(path)
    low = sosfilt(butter(4, 150, "low", fs=rate, output="sos"), x)
    hop = rate // 100                                   # 10 ms frames
    frames = len(low) // hop
    e = np.sqrt((low[: frames * hop].reshape(frames, hop) ** 2).mean(axis=1)) + 1e-9
    typical = np.median(e[e > 1e-4]) if np.any(e > 1e-4) else 1e-4
    found = []
    for i in range(5, frames):
        if e[i] > 6 * typical and e[i] > 4 * e[i - 5:i].mean() and e[i] > 0.01:
            if not found or i - found[-1][0] > 15:
                found.append((i, e[i]))
    return [(i / 100, 20 * np.log10(v)) for i, v in sorted(found, key=lambda f: -f[1])]

files = []
for arg in sys.argv[1:]:
    p = pathlib.Path(arg)
    files += sorted(p.glob("*.wav")) if p.is_dir() else [p]
results = []
for f in files:
    try: results.append((pops(f), f))
    except Exception as err: print(f"skip {f.name}: {err}", file=sys.stderr)
for found, f in sorted(results, key=lambda r: -len(r[0]))[:12]:
    print(f"{len(found):3d} pops  {f.name}  strongest at " + ", ".join(f"{t:.2f}s ({db:.0f} dB)" for t, db in found[:5]))
