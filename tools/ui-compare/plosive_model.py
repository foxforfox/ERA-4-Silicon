"""Python reference of the ERA 4.5 Plosive Remover (shared/DSP/PlosiveReducerDSP.cpp), used to fit it
to ERA 4 by measurement. Change a constant here, re-run, and only port to C++ once the null improves.

    python3 plosive_model.py in.wav era4_out.wav [processing 0..1] [extreme 0|1]
        prints the low-band (<300 Hz) and overall null of this model against an ERA 4 render
        (era4_out from au-render; its 4096-sample latency is removed here)

Measured (2026-11-16, blackbox_probe.py): detector = energy < 77.5 Hz vs above, summed over the
last 3 frames; gain = floor + (1-floor)*min(1, (10^(T/10)*high/low)^k); cut band DC..edge with a
linear-gain taper; Extreme only moves the edge x1.485. Vocal null at defaults: -30.5 dB.
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import stft, istft, butter, sosfilt

R, N, H = 44100, 4096, 1024
P = np.array([0, .2, .4, .6, .8, 1.])
MAXCUT = np.array([0, 10.2, 18.36, 24.39, 31.38, 33.42])
THRESH = np.array([1.08, 1.08, -3.04, -5.86, -10.24, -14.47])
EXPO = np.array([1.16, 1.16, 1.31, 1.41, 1.78, 1.98])
EDGE = np.array([125.9, 125.9, 132.7, 155.6, 177.1, 202.3])
TX = np.array([0, .2, .34, .47, .61, .68, .75, .81, .88, .95, 1.0])
TW = np.array([1, .9993, .9971, .9946, .990, .982, .9725, .945, .873, .255, 0])

def run(x, p=.4, extreme=False):
    f, _, X = stft(x, R, window="hann", nperseg=N, noverlap=N - H)
    E = np.abs(X) ** 2
    wl = np.clip((77.5 + 5 - f) / 10, 0, 1)[:, None]; wl[0] = 0
    box = lambda v: np.convolve(v, np.ones(3))[:len(v)] + 1e-20
    low, high = box((E * wl).sum(0)), box((E * (1 - wl)).sum(0))
    q = np.clip((10 ** (np.interp(p, P, THRESH) / 10) * high / low) ** np.interp(p, P, EXPO), 0, 1)
    floor = 10 ** (-np.interp(p, P, MAXCUT) / 20)
    g = floor + (1 - floor) * q
    w = np.interp(f / (np.interp(p, P, EDGE) * (1.485 if extreme else 1)), TX, TW)[:, None]
    _, y = istft(X * (1 - (1 - g[None, :]) * w), R, window="hann", nperseg=N, noverlap=N - H)
    return y[:len(x)]

if __name__ == "__main__":
    def load(path):
        x = wavfile.read(path)[1].astype(float)
        x = x.mean(1) if x.ndim > 1 else x
        return x / 32768 if np.abs(x).max() > 2 else x
    x, e = load(sys.argv[1]), load(sys.argv[2])[4096:]
    p = float(sys.argv[3]) if len(sys.argv) > 3 else .4
    n = min(len(x), len(e)); x, e = x[:n], e[:n]
    y = run(x, p, len(sys.argv) > 4 and sys.argv[4] == "1")
    lo = butter(4, 300, "low", fs=R, output="sos")
    db = lambda a, b: 20 * np.log10(np.std(a) / np.std(b))
    print(f"null <300Hz {db(sosfilt(lo, e - y), sosfilt(lo, e)):.1f} dB   all {db(e - y, e):.1f} dB")
