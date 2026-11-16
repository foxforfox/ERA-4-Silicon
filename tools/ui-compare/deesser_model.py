"""Python reference of the ERA 4.5 De-Esser Pro (shared/DSP/DeEsserProDSP.cpp), fitted to ERA 4 by
measurement (blackbox_probe.py). Change a constant here and re-run before touching the C++.

    python3 deesser_model.py in.wav era4_out.wav [focus_norm shaping_norm processing]
        prints the null (>1 kHz and overall) of this model against an ERA 4 render
        (era4_out from au-render; its 256-sample latency is removed here). Defaults 0.4286 0.5 0.4.

Measured 2026-11-16: detector = energy above FOCUS vs below (no smoothing); gain = floor + (1-floor) *
min(1, 10^(T/10)*below/above); cut = smooth step above FOCUS centred at focus + (22050-focus)*h(u),
u = processing + 0.8*shaping. Vocal null at defaults -34.2 dB (old build -15.7).
"""
import numpy as np
from scipy.signal import stft, istft, lfilter
R=44100; NY=22050; N=256
PP=np.array([0,.1,.2,.3,.4,.5,.6,.7,.8,.85,.9,.95,1])
MM=np.array([0,2.13,4.82,5.49,6.11,6.71,7.32,7.92,8.53,12.19,15.63,18.84,21.74])
TT=np.array([3.93,3.93,.11,-.58,-1.17,-1.75,-2.31,-2.84,-3.31,-6.34,-8.90,-11.37,-13.74])
U=np.array([0,.2,.4,.6,.8,1,1.2,1.4,1.6,1.8]); HH=np.array([.295,.2617,.2191,.1721,.1202,.07,.0203,-.0347,-.1032,-.2676])
WFRAC=.0241; GATEW=39.4
def mask(f,focus,sh,p):
    f50=focus+(NY-focus)*np.interp(p+.8*sh,U,HH); wd=WFRAC*(NY-focus)
    return 1/(1+np.exp(np.clip(-(f-f50)/wd,-50,50)))/(1+np.exp(np.clip(-(f-focus)/GATEW,-50,50)))
def run(x,focus=4000,sh=.5,p=.4,N=N,hop=None,smooth=None,detw="gate"):
    hop=hop or N//4
    f,_,X=stft(x,R,window="hann",nperseg=N,noverlap=N-hop)
    w=mask(f,focus,sh,p)[:,None]; E=np.abs(X)**2; E[0]=0
    dw=w if detw=="mask" else (f>=focus)[:,None].astype(float)
    hi=(E*dw).sum(0)+1e-20; lo=(E*(1-dw)).sum(0)+1e-20
    if smooth: hi,lo=smooth(hi),smooth(lo)
    gmin=10**(-np.interp(p,PP,MM)/20); q=np.clip(10**(np.interp(p,PP,TT)/10)*lo/hi,0,1)
    g=gmin+(1-gmin)*q
    _,y=istft(X*(1-(1-g[None,:])*w),R,window="hann",nperseg=N,noverlap=N-hop)
    return y[:len(x)]

if __name__ == "__main__":
    import sys
    from scipy.io import wavfile
    from scipy.signal import butter, sosfilt
    def load(p):
        v = wavfile.read(p)[1].astype(float); v = v.mean(1) if v.ndim > 1 else v
        return v / 32768 if np.abs(v).max() > 2 else v
    x, e = load(sys.argv[1]), load(sys.argv[2])[256:]
    fo, sh, p = (map(float, sys.argv[3:6]) if len(sys.argv) > 5 else (.4286, .5, .4))
    n = min(len(x), len(e)); x, e = x[:n], e[:n]
    y = run(x, 1000 + 7000 * fo, sh, p)
    hp = butter(4, 1000, "high", fs=R, output="sos")
    db = lambda a, b: 20 * np.log10(np.std(a) / np.std(b))
    print(f"null >1kHz {db(sosfilt(hp, e - y), sosfilt(hp, e)):.1f} dB   all {db(e - y, e):.1f} dB")
