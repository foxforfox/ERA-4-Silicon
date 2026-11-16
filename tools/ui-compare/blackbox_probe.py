"""Black-box measurement of what a plugin does, from its output alone (no decompiling).

  make <dir>                      write the probe WAVs (44.1 kHz float, mono) into <dir>
  map <in.wav> <out.wav> [--delay N] [--events t1,t2,...]
                                  print the gain the plugin applied (output / input, STFT)
                                  per band, overall and around each event

Probes (each has 1 s of silence at the end so latency does not cut anything off):
  pops_level   one 60 ms pop (low-passed noise + 45 Hz thump) every 1 s at -36..-6 dBFS, over silence
  pops_voice   the same pops over a steady voice-like tone (150 Hz + harmonics, -18 dBFS)
  pops_band    octave noise bursts with the pop envelope, centred 40..1280 Hz, -12 dBFS
  steady       2 s sine each at 40, 80, 160, 320, 640, 1280 Hz, -12 dBFS
  thump_len    45 Hz thumps lasting 20, 40, 80, 160, 320 ms (are long lows treated as pops?)
  tones <in.wav> <out.wav> <delay> "f1,f2;f1,f2;..."
                                  steady-tone probes (one 1 s segment per ';' group): gain in dB of each
                                  listed tone, read with a 16384-point FFT (best for curves: build a WAV
                                  of 1 s segments that step one variable, e.g. a 40 Hz tone + 300 Hz at
                                  levels -12..+24 dB to map cut vs low/high ratio)
Events (pop start times) are printed by `make` and stored in <dir>/events.txt.
Alignment: --delay is the plugin's reported latency; otherwise cross-correlation.
"""
import sys, os, argparse
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, correlate, stft

RATE = 44100
rng = np.random.default_rng(7)

def env(n, attack=0.003, decay=0.018):
    t = np.arange(n) / RATE
    return (1 - np.exp(-t / attack)) * np.exp(-t / decay)

def pop(level_db, n=int(0.06 * RATE)):
    t = np.arange(n) / RATE
    noise = sosfilt(butter(4, 120, "low", fs=RATE, output="sos"), rng.standard_normal(n))
    p = (noise / np.abs(noise).max() * 0.6 + np.sin(2 * np.pi * 45 * t) * 0.8) * env(n)
    return p / np.abs(p).max() * 10 ** (level_db / 20)

def band_burst(fc, level_db, n=int(0.06 * RATE)):
    sos = butter(2, [fc / 2 ** 0.5, min(fc * 2 ** 0.5, RATE / 2 - 1)], "band", fs=RATE, output="sos")
    b = sosfilt(sos, rng.standard_normal(n)) * env(n)
    return b / np.abs(b).max() * 10 ** (level_db / 20)

def place(total_s, items):
    x = np.zeros(int((total_s + 1) * RATE))
    for t0, sig in items:
        i = int(t0 * RATE); x[i:i + len(sig)] += sig
    return x

def make(d):
    os.makedirs(d, exist_ok=True)
    probes, events = {}, {}
    lv = [-36, -30, -24, -18, -12, -6]
    probes["pops_level"] = place(len(lv) + 0.5, [(0.5 + i, pop(l)) for i, l in enumerate(lv)])
    events["pops_level"] = [0.5 + i for i in range(len(lv))]
    t = np.arange(int((len(lv) + 1.5) * RATE)) / RATE
    voice = sum(np.sin(2 * np.pi * 150 * k * t) / k for k in range(1, 12))
    voice = voice / np.abs(voice).max() * 10 ** (-18 / 20)
    probes["pops_voice"] = voice + place(len(lv) + 0.5, [(0.5 + i, pop(l)) for i, l in enumerate(lv)])[:len(voice)]
    probes["pops_voice"][-RATE:] = 0
    events["pops_voice"] = events["pops_level"]
    fcs = [40, 80, 160, 320, 640, 1280]
    probes["pops_band"] = place(len(fcs) + 0.5, [(0.5 + i, band_burst(f, -12)) for i, f in enumerate(fcs)])
    events["pops_band"] = [0.5 + i for i in range(len(fcs))]
    t2 = np.arange(2 * RATE) / RATE
    probes["steady"] = np.concatenate([np.sin(2 * np.pi * f * t2) * 10 ** (-12 / 20) for f in fcs] + [np.zeros(RATE)])
    events["steady"] = [2.0 * i for i in range(len(fcs))]
    lens = [0.02, 0.04, 0.08, 0.16, 0.32]
    th = []
    for i, L in enumerate(lens):
        n = int(L * RATE); tt = np.arange(n) / RATE
        th.append((0.5 + i, np.sin(2 * np.pi * 45 * tt) * np.hanning(n) * 0.25))
    probes["thump_len"] = place(len(lens) + 0.5, th)
    events["thump_len"] = [0.5 + i for i in range(len(lens))]
    with open(os.path.join(d, "events.txt"), "w") as f:
        for k, x in probes.items():
            wavfile.write(os.path.join(d, k + ".wav"), RATE, x.astype(np.float32))
            f.write(k + " " + ",".join(str(e) for e in events[k]) + "\n")
            print(k, events[k])

def load(p):
    r, x = wavfile.read(p)
    x = x.astype(np.float64)
    if np.abs(x).max() > 2: x /= 32768.0
    return (x.mean(axis=1) if x.ndim > 1 else x), r

BANDS = [(20, 60), (60, 120), (120, 250), (250, 500), (500, 1000), (1000, 4000), (4000, 20000)]

def gain_map(x, y, nper=2048, hop=256):
    f, t, X = stft(x, RATE, nperseg=nper, noverlap=nper - hop)
    _, _, Y = stft(y, RATE, nperseg=nper, noverlap=nper - hop)
    return f, t, np.abs(X), np.abs(Y)

def band_gain(f, X, Y, lo, hi, cols):
    m = (f >= lo) & (f < hi)
    ex, ey = (X[m][:, cols] ** 2).sum(), (Y[m][:, cols] ** 2).sum()
    return 10 * np.log10((ey + 1e-20) / (ex + 1e-20)), 10 * np.log10(ex + 1e-20)

def mapcmd(a):
    x, _ = load(a.inp); y, _ = load(a.out)
    n = min(len(x), len(y)); x, y = x[:n], y[:n]
    d = a.delay if a.delay is not None else int(np.argmax(correlate(y, x, "full", "fft"))) - (n - 1)
    y = np.concatenate([y[d:], np.zeros(d)]) if d >= 0 else np.concatenate([np.zeros(-d), y[:n + d]])
    f, t, X, Y = gain_map(x, y)
    print(f"delay {d}   overall removed {20*np.log10(np.std(x-y)/np.std(x)+1e-12):.1f} dB rel. input")
    hdr = "".join(f"{f'{lo}-{hi}':>11}" for lo, hi in BANDS)
    events = [float(e) for e in a.events.split(",")] if a.events else []
    print(f"{'window':>16}{hdr}   (gain dB; '.' = no input energy there)")
    def row(name, cols):
        s = ""
        for lo, hi in BANDS:
            g, e = band_gain(f, X, Y, lo, hi, cols)
            s += f"{g:11.1f}" if e > -60 else f"{'.':>11}"
        print(f"{name:>16}{s}")
    row("all", slice(None))
    for e in events:
        c = (t >= e - 0.02) & (t < e + 0.12)
        row(f"event {e:.2f}s", c)
    if a.timeline is not None:
        e = a.timeline
        lo, hi = a.tband
        m = (f >= lo) & (f < hi)
        print(f"\ntimeline {lo}-{hi} Hz around {e}s (frame hop {256/RATE*1000:.1f} ms)")
        for i in np.where((t >= e - 0.05) & (t < e + 0.4))[0]:
            ex, ey = (X[m, i] ** 2).sum(), (Y[m, i] ** 2).sum()
            print(f"  {1000*(t[i]-e):7.1f} ms  in {10*np.log10(ex+1e-20):6.1f} dB  gain {10*np.log10((ey+1e-20)/(ex+1e-20)):6.1f} dB")

def tones(inp, out, delay, spec):
    x, _ = load(inp); y, _ = load(out); y = y[int(delay):]
    win = np.hanning(16384)
    for i, group in enumerate(spec.split(";")):
        a = int((i + 0.3) * RATE)
        X = np.abs(np.fft.rfft(x[a:a + 16384] * win)); Y = np.abs(np.fft.rfft(y[a:a + 16384] * win))
        cells = []
        for f in map(float, group.split(",")):
            k = int(round(f * 16384 / RATE)); sl = slice(k - 2, k + 3)
            cells.append(f"{f:g}Hz {20 * np.log10(Y[sl].max() / X[sl].max()):6.1f}")
        print(f"seg{i}: " + "  ".join(cells))

if __name__ == "__main__":
    if sys.argv[1] == "make":
        make(sys.argv[2])
    elif sys.argv[1] == "tones":
        tones(*sys.argv[2:6])
    else:
        ap = argparse.ArgumentParser(); ap.add_argument("cmd"); ap.add_argument("inp"); ap.add_argument("out")
        ap.add_argument("--delay", type=int); ap.add_argument("--events")
        ap.add_argument("--timeline", type=float); ap.add_argument("--tband", type=float, nargs=2, default=[20, 250])
        mapcmd(ap.parse_args())
