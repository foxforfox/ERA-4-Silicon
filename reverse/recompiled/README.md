# ERA 4 Silicon — Recompiled Reference Implementations

Cleaned-up, compilable C reference implementations of the DSP cores
from the ERA 4 audio plugins, recovered from Ghidra decompilation of the
original x86_64 binaries.

**These are reference libraries, not JUCE plugins.** The actual product
plugins live in `plugins/`.

## Plugins

| Library                        | FFT Size | Latency | Description                                |
|-------------------------------|----------|---------|--------------------------------------------|
| DeEsser ERA 4 Silicon         | 2048     | 2048    | Sibilance reduction (Broad/Normal/Narrow)  |
| DeEsserPro ERA 4 Silicon      | 256      | 256     | De-essing with focus frequency + shaping   |
| PlosiveRemover ERA 4 Silicon  | 4096     | 4096    | Low-frequency plosive reduction            |
| NoiseRemover ERA 4 Silicon    | 2048     | 2048    | MCRA noise estimation + Wiener gain (6 focus modes) |
| DeClipper ERA 4 Silicon       | 2048     | 2048    | Spectral restoration + Catmull-Rom spline  |
| ReverbRemover ERA 4 Silicon   | 2048     | 2048    | Spectral decay tracking + auto-gain        |
| VoiceLeveler ERA 4 Silicon    | N/A      | ~240    | Time-domain AGC (no STFT), lookahead delay |
| ERA-D ERA 4 Silicon           | 2048     | 4096    | Dual-engine: noise + reverb combined       |

## What was done

Each decompiled binary (3M+ lines from Ghidra) was searched for DSP-relevant
functions using `vDSP`, `powf`, `log10f`, and struct offset patterns. The
relevant functions were:

- **Struct layouts** recovered from pointer arithmetic (`param_1 + 0x138`
  becomes a named struct field)
- **Ghidra artifacts** replaced: `undefined4`/`undefined8` → proper C types,
  `FUN_007xxxxx` → descriptive function names, `DAT_00xxxxxx` → named constants
- **x86 SSE intrinsics** (`minps`, `maxps`, `divpd`) replaced with portable C
  `min`/`max`/division
- **vDSP calls** preserved on Apple (via Accelerate), with portable fallbacks
- **LOCK/UNLOCK** patterns replaced with standard atomic operations or
  single-threaded equivalents
- **DSP algorithm logic** preserved faithfully (STFT, overlap-add, spectral
  gain masking, ring buffers, detector formulas)

## Building

```
cd reverse/recompiled
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Produces eight static libraries in `build/`:
- `libDeEsser_ERA_4_Silicon.a`
- `libDeEsserPro_ERA_4_Silicon.a`
- `libPlosiveRemover_ERA_4_Silicon.a`
- `libNoiseRemover_ERA_4_Silicon.a`
- `libDeClipper_ERA_4_Silicon.a`
- `libReverbRemover_ERA_4_Silicon.a`
- `libVoiceLeveler_ERA_4_Silicon.a`
- `libERA-D_ERA_4_Silicon.a`

**Requirements:** macOS 12+, Apple Silicon (arm64), CMake 3.21+

## File structure

```
recompiled/
  common/
    era_types.h          Shared struct definitions (EraEngine, EraConfig, etc.)
    era_util.h           Shared utilities (ring buffer, matrix ops, vDSP wrappers)
  DeEsser_ERA_4_Silicon/
    deesser_dsp.h        Public API and state struct
    deesser_dsp.c        Implementation
  DeEsserPro_ERA_4_Silicon/
    deesser_pro_dsp.h    Public API and state struct
    deesser_pro_dsp.c    Implementation
  PlosiveRemover_ERA_4_Silicon/
    plosive_dsp.h        Public API and state struct
    plosive_dsp.c        Implementation
  NoiseRemover_ERA_4_Silicon/
    noise_remover_dsp.h  Public API and state struct
    noise_remover_dsp.c  Implementation (MCRA noise estimation + Wiener gain)
  DeClipper_ERA_4_Silicon/
    declipper_dsp.h      Public API and state struct
    declipper_dsp.c      Implementation (spectral restoration + spline interp)
  ReverbRemover_ERA_4_Silicon/
    reverb_remover_dsp.h Public API and state struct
    reverb_remover_dsp.c Implementation (spectral decay tracking + auto-gain)
  VoiceLeveler_ERA_4_Silicon/
    voice_leveler_dsp.h  Public API and state struct
    voice_leveler_dsp.c  Implementation (time-domain AGC, no STFT)
  ERA-D_ERA_4_Silicon/
    era_d_dsp.h          Public API and state struct
    era_d_dsp.c          Implementation (dual-engine: noise + reverb)
  CMakeLists.txt         Build system (arm64, macOS 12+, static libraries)
  README.md              This file
```

## Decompiled source mapping

Each `.c` file documents which decompiled functions it recovers, e.g.:

| Decompiled function | Recovered as           | File               |
|--------------------|-----------------------|--------------------|
| FUN_007b7d30       | `deesser_prepare()`    | deesser_dsp.c      |
| FUN_007b97b0       | `deesser_process()`    | deesser_dsp.c      |
| FUN_007ced10       | STFT frame extraction  | (inlined)          |
| FUN_007cf080       | STFT overlap-add       | (inlined)          |
| FUN_007cf800       | Matrix-vector multiply | (inlined)          |
| FUN_007cebe0       | `era_ring_write()`     | era_util.h         |
| FUN_007cf6b0       | `era_ring_read()`      | era_util.h         |
| FUN_007ce5d0       | `era_ring_init()`      | era_util.h         |
| FUN_007ce450       | `era_matrix_d_resize()`| era_util.h         |
| FUN_007ce700       | `era_matrix_f_resize()`| era_util.h         |
| FUN_007ce3c0       | `era_vector_d_resize()`| era_util.h         |
