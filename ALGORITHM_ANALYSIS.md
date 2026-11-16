# ERA 4 Algorithm Analysis

Binary analysis of ERA 4 VST plugins (Accusonus) to reverse-engineer DSP algorithms for ERA 4.5 reimplementation.

## Common Architecture

All ERA 4 plugins are x86_64 + i386 fat Mach-O JUCE-based VSTs. Internal libraries:
- **a3dsplib** (`/Users/accusonus/AccusonusProjects/a3dsplib/source/DSP_Window.cpp`) -- STFT framework
- **Eigen 3.3.2** (`/Users/accusonus/SDKs/Eigen/3.3.2/`) -- matrix/vector operations
- **Apple Accelerate** -- `vDSP_vclr`, `vDSP_vclrD` (buffer clearing), `vDSP_vsmul` (scalar multiply)
- **STFT constraint** found in assertion: `(hopSize > 0)&&(hopSize <= windowSize/2)`

### Common STFT Parameters (a3dsplib DSP_Window)
- **Window type**: Hann (standard for overlap-add COLA)
- **FFT size**: 2048 samples (at 44.1/48 kHz, ~43-46ms)
- **Hop size**: fftSize / 4 = 512 samples (75% overlap)
- **Overlap-add**: Weighted overlap-add (WOLA) with Hann analysis + synthesis windows
- Numeric constants found in binaries: 1024, 2048, 3072, 4096, 6144, 8192

### Binary Sizes (complexity proxy)
| Plugin | Size | STFT | Eigen | Splines | EnvelopeDetector | DelayedIO |
|--------|------|------|-------|---------|-----------------|-----------|
| DeEsserPro | 13.7 MB | Yes (7 Eigen refs) | Yes | No | No | No |
| VoiceLeveler | 14.6 MB | No | float | No | Yes | float |
| DeEsser | 23.6 MB | Yes (14 Eigen refs) | Yes | No | No | No |
| PlosiveRemover | 23.6 MB | Yes (14 Eigen refs) | Yes | No | No | No |
| ReverbRemover | 28.7 MB | Yes | No | No | No | No |
| NoiseRemover | 29.0 MB | Yes | double | No | Yes | double |
| DeClipper | 29.2 MB | No | double | BesselSpline | Yes | double |

---

## Per-Plugin Analysis

### 1. Noise Remover

**ERA namespace classes**: `ERA::Estimator` (base), `ERA::NoiseEstimator`, `ERA::DenoiseEngine`, `ERA::RestorationEngine`
**Other classes**: `ERAProcessor`, `EnvelopeDetector`, `DelayedIO<Eigen::Matrix<double>>`, `ButtonGroupSelector`

**Focus modes** (ButtonGroupSelector): All-freq, Hi-freq, Low-freq, Hi/low-freq, Mid-freq

**Algorithm**: Multi-stage STFT spectral noise reduction
1. **ERA::NoiseEstimator** -- Noise PSD estimation using minimum statistics (MCRA-inspired). Tracks running minimum of smoothed power spectrum in a sliding sub-window (~0.5s). Bias compensation factor (1.5-2.0x) corrects for minimum underestimation. Speech presence probability controls noise estimate adaptation rate.
2. **ERA::DenoiseEngine** -- Wiener-like spectral gain computation. Per-bin gain: `G = max(floor, 1 - overSubtract * noisePower / signalPower)`. Oversubtraction factor scales from 1.0 to 4.0 with processing amount. Asymmetric temporal gain smoothing (fast attack, slow release) to reduce musical noise.
3. **ERA::RestorationEngine** -- Signal restoration / musical noise suppression.
4. **EnvelopeDetector** -- Voice activity detection.
5. **DelayedIO<Eigen::Matrix<double>>** -- Lookahead buffer (double precision).

**Processing Amount mapping**:
- 0% -> 0 dB suppression, oversubtraction = 1.0
- 100% -> ~45 dB suppression, oversubtraction = 4.0
- Gain floor = 10^(-amount * 45 / 20) dB

**Frequency Focus implementation**: Linear weighting in normalized frequency space. All=1.0, Hi=0.2+0.8*nf, Low=0.2+0.8*(1-nf), Hi/Low=0.2+0.8*|nf-0.5|*2, Mid=0.2+0.8*(1-|nf-0.5|*2)

**Float constants found**: 0.5, 0.25, 0.9, 0.95, 1.5, 2.0, 3.0

---

### 2. De-Esser (Standard)

**Classes**: `DeEsserProcessor`, `ButtonGroupSelector`
**Modes**: Broad / Narrow

**Algorithm**: STFT-based frequency-selective sibilance suppression

**Band definitions**:
- **Broad**: 4000-12000 Hz
- **Narrow**: 5000-9000 Hz

**Sibilance detection**: Band energy ratio (sibilance energy / total energy). Dynamic threshold: `0.10 + (1 - amount) * 0.30`. When ratio exceeds threshold, sibilance probability increases with fast attack (0.3), slow release (0.05).

**Gain computation**: Per-bin gain shaped by squared-sine bell within sibilance band. `gain = 1 - reductionDepth * bellShape`. Gain floor = 0.05 + (1-amount)*0.15. Temporal gain smoothing coefficient = 0.3.

---

### 3. De-Esser Pro

**Classes**: `ERAProProcessor`, `DeEsserProProcessor`, `FocusControl`
**Source paths**: a3dsplib DSP_Window.cpp, Eigen 3.3.2 Block.h

**Algorithm**: Enhanced STFT de-essing with parametric focus control

**FOCUS slider** (1 kHz - 8 kHz):
- Logarithmic mapping: `focusHz = 1000 * 8^(normalized)` for perceptual linearity
- 0% = 1000 Hz, 50% = 2828 Hz, 100% = 8000 Hz

**SHAPING slider** (Soft - Sharp):
- Maps to bandwidth in octaves: `width = 2.5 - normalized * 2.1`
- Soft (0%) = 2.5 octaves wide, Sharp (100%) = 0.4 octaves narrow
- Gaussian weighting in log-frequency domain: `weight = exp(-distOctaves^2 / (2 * width^2))`
- Hard cutoff at weight < 0.01

**Sibilance detection**: Combined score from:
- Band energy ratio (70% weight): weighted energy in focus band / total energy
- Spectral flux (30% weight): half-wave rectified magnitude increase in focus band
- Dynamic threshold: `0.08 + (1 - amount) * 0.25`
- Temporal smoothing: attack=0.3, release=0.05

**Gain computation**: `gain = 1 - reductionDepth * focusBandWeight`. Gain floor = 0.03 + (1-amount)*0.12.

**DIFF mode**: Outputs removed content only: `mag *= (1 - gain) * outputGain`

**Spectrum display**: FFTProcessor stores latest pre-processing magnitude spectrum (lock-free single-writer). GUI reads at 30 Hz with exponential smoothing (alpha=0.3).

---

### 4. De-Clipper

**Classes**: `DeClipperProcessor`, `DeClipperController`, `EnvelopeDetector`, `DeClipperKnob`
**External libraries**: Splines (BesselSpline, CubicSplineBase)
**Debug strings**: "BesselSpline::build(): npts = ", "In spline: "
**NO STFT** -- primarily time-domain processing

**Algorithm**: Dual-stage waveform reconstruction

**Stage 1**: STFT spectral harmonic restoration
- Maintains reference spectrum from clean (non-clipped) frames
- Clipping destroys odd harmonics; reference spectrum captures expected harmonic shape
- Detects clipped frames via high-frequency energy ratio
- Boosts back high-frequency harmonics weaker than reference (up to 4x)
- Age-based confidence decay when no clean frames available

**Stage 2**: Time-domain cubic spline interpolation
- ERA 4 used Bessel splines (BesselSpline library) for C2-continuous reconstruction
- Detects clipped samples (|sample| >= threshold)
- Finds nearest non-clipped samples on each side
- Spline interpolation through clip region
- ERA 4.5 uses Catmull-Rom (C1-continuous, simpler)

**Processing Type** (Standard / Heavy):
- Standard: shorter context window, basic reconstruction
- Heavy: longer context window, more aggressive reconstruction

**Clip threshold**: `0.99 - processingAmount * 0.10` (lower = detect more clips)

---

### 5. Reverb Remover

**ERA namespace classes**: `ERA::Estimator`, `ERA::ReverbEstimator`, `ERA::DereverberationEngine`, `ERA::RestorationEngine`
**Focus modes**: Same 5 as NoiseRemover + "Automatic make-up gain."

**Algorithm**: STFT-based spectral dereverberation (mirrors NoiseRemover architecture)

1. **Per-bin spectral decay tracking**: Estimates decay rate per frequency bin. Reverb bins decay slowly; direct sound decays quickly.
2. **Transient detection**: Sharp increase in magnitude relative to previous frame (>50% relative increase). Fast attack (0.5), moderate release (0.85). During transients, reverb estimate drops rather than grows.
3. **Reverb power estimation**: Tracks slowly-decaying envelope. Rises during reverb tail (signal decaying but still present). Limited to 90% of smoothed signal power.
4. **Wiener-like gain**: `gain = max(floor, 1 - overSubtract * reverbPower * focusWeight / signalPower)`. Transient preservation boosts gain during detected transients.
5. **Auto-gain compensation**: RMS-based input/output level matching. Smoothed compensation factor (0.5x-6.0x range).

**Processing Amount mapping**: Max suppression = amount * 35 dB. Oversubtraction = 1.0 + amount * 2.5.

---

### 6. Voice Leveler

**Classes**: `LouderProcessor` (NOT ERAProcessor), `EnvelopeDetector`, `DelayedIO<Eigen::Matrix<float>>`
**NO STFT** -- pure time-domain processing

**Algorithm**: Lookahead AGC (automatic gain control)

1. **Lookahead delay** (~5ms): `DelayedIO<Eigen::Matrix<float>>` enables transparent gain changes
2. **Dual envelope detection**:
   - RMS envelope: sliding window (~30ms), separate attack/release IIR
   - Peak envelope: fast attack (~1ms), moderate release
3. **Noise gate with hysteresis**: Open threshold = gateThreshDb + 6 dB, close at gateThreshDb
4. **Soft-knee gain computer**: Centered on target RMS. Knee width = 6 dB. Below target: boost. Above target: reduce.
5. **Peak limiter**: If gain would cause clipping, reduces gain to stay below 0.98.
6. **Gain smoothing**: Separate attack (10-40ms) and release (80-200ms) coefficients.

**Processing Amount mapping**:
- Target RMS: -24 to -12 dBFS
- Max boost: +6 to +24 dB
- Max cut: -3 to -12 dB
- Gate threshold: -50 to -60 dB

---

### 7. Plosive Remover

**Classes**: `ERAProcessor`, `WaveformViewComponent`, `LineViewer`
**Uses STFT** (a3dsplib + Eigen)

**Algorithm**: STFT-based low-frequency transient detection and attenuation

1. **Band energy measurement**: Low (<300 Hz), Mid (300-3000 Hz), Hi (>3000 Hz)
2. **Spectral flux in low band**: Half-wave rectified magnitude increase, exponential smoothing
3. **Plosive detection criteria** (all must be true):
   - Spectral flux onset detected
   - Low-frequency dominant (low/mid ratio + low/hi ratio above thresholds)
   - Rapid rise or above baseline
4. **Plosive probability state machine**: Fast attack, hold phase (~20-40ms), slow decay
5. **Frequency-weighted gain reduction**: Strongest at lowest bins, tapering to 300 Hz

---

### 8. ERA-D (Dual: Denoise + Dereverb)

**Classes**: `AudioEnhancementProcessor`, `PsdGraph`
Combines NoiseRemover + ReverbRemover algorithms with:
- Independent DENOISE and DEREVERB amount controls
- Signal path routing (Cascade, Denoise First, Dereverb First, Parallel)
- Parallel blend control
- Output monitoring (Output, Input, Difference)

---

## Implementation Notes for ERA 4.5

### FFTProcessor
- Order 11 (2048 samples), hop = 512, Hann window
- WOLA normalization computed numerically for correctness
- Both mag/phase and raw-complex callback APIs
- Thread-safe spectrum readout for GUI display

### Key Differences from ERA 4
- ERA 4 used double-precision Eigen matrices for some plugins (NoiseRemover, DeClipper); ERA 4.5 uses float for performance on modern hardware
- ERA 4's Bessel spline library replaced with Catmull-Rom (4-point cubic) which provides similar C1-continuous interpolation
- ERA 4's minimum statistics noise estimator used full MCRA; ERA 4.5 uses simplified sliding-window minimum tracking with bias compensation
- ERA 4's reverb estimator likely used more sophisticated decay modeling; ERA 4.5 uses per-bin decay rate tracking with direct-sound vs. reverb separation
