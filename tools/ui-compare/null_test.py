"""Null test: how close is an ERA 4.5 render to the ERA 4 render of the same input?

Aligns the two outputs by cross-correlation (plugins report latency differently),
then prints the residual (original minus rebuild) relative to the original, overall
and below/above 300 Hz, plus how much each removed from the input.
-60 dB or lower is a practical match; -20 dB is clearly different.
Usage: python3 null_test.py input.wav era4_out.wav era45_out.wav
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, correlate

def load(path):
    rate, x = wavfile.read(path)
    x = x.astype(np.float64)
    if x.dtype != np.float32 and np.abs(x).max() > 2:
        x /= {2: 32768.0, 4: 2147483648.0}.get(wavfile.read(path)[1].dtype.itemsize, 32768.0)
    return (x.mean(axis=1) if x.ndim > 1 else x), rate

def lag(ref, x, rate):
    seg = slice(0, min(len(ref), len(x), rate * 10))
    c = correlate(x[seg], ref[seg], mode="full", method="fft")
    return int(np.argmax(c)) - (len(ref[seg]) - 1)

def shift(x, d):
    return np.concatenate([x[d:], np.zeros(d)]) if d > 0 else np.concatenate([np.zeros(-d), x[:len(x) + d]])

def db(a, b): return 20 * np.log10((np.sqrt(np.mean(a ** 2)) + 1e-12) / (np.sqrt(np.mean(b ** 2)) + 1e-12))

inp, rate = load(sys.argv[1]); old, _ = load(sys.argv[2]); new, _ = load(sys.argv[3])
n = min(len(inp), len(old), len(new))
inp, old, new = inp[:n], old[:n], new[:n]
old = shift(old, lag(inp, old, rate)); new = shift(new, lag(inp, new, rate))
lo = butter(4, 300, "low", fs=rate, output="sos"); hi = butter(4, 300, "high", fs=rate, output="sos")
res = old - new
print(f"residual  all {db(res, old):6.1f} dB   <300Hz {db(sosfilt(lo, res), sosfilt(lo, old)):6.1f} dB   >300Hz {db(sosfilt(hi, res), sosfilt(hi, old)):6.1f} dB")
print(f"removed   ERA 4 {db(inp - old, inp):6.1f} dB   ERA 4.5 {db(inp - new, inp):6.1f} dB   (removed signal relative to input)")
