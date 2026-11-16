# ERA 4 Silicon

Native Apple Silicon rebuilds of the original **accusonus ERA 4** audio restoration plugins.

accusonus was acquired by Meta in 2022 and subsequently shut down. The ERA plugins were released for free before the shutdown. This project preserves the original ERA 4 DSP algorithms by decompiling the x86_64 binaries with Ghidra, cleaning up the output into readable C, and wrapping them as modern AU/VST3 plugins that run natively on Apple Silicon.

## Plugins

| Plugin | What it does |
|--------|-------------|
| **De-Esser** | Sibilance reduction (FFT, bell-shaped per-bin gain) |
| **De-Esser Pro** | Advanced sibilance reduction with focus frequency and shaping |
| **De-Clipper** | Clipped audio restoration (spectral + spline interpolation) |
| **Noise Remover** | Broadband noise reduction (MCRA noise floor, Wiener gain, 6 focus modes) |
| **Plosive Remover** | Plosive/pop reduction (sub-band spectral cut) |
| **Reverb Remover** | Reverb/room tone reduction (spectral decay tracking) |
| **Voice Leveler** | Automatic voice level compression (time-domain AGC) |
| **ERA-D** | Dual-engine: noise removal + reverb removal in one plugin |

## Building

Requires:
- macOS 12+ (Monterey or later)
- CMake 3.22+
- Xcode or Apple command-line tools

```bash
git clone --recursive https://github.com/foxforfox/ERA-4-Silicon.git
cd ERA-4-Silicon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
```

Plugins are automatically installed to `~/Library/Audio/Plug-Ins/` (VST3 + AU + Standalone).

## Project Structure

```
plugins/Silicon/       — JUCE plugin wrappers for each ERA 4 Silicon plugin
reverse/recompiled/    — Clean C17 reference implementations of the original DSP
  common/              — Shared types and utilities (ring buffers, matrices, vDSP)
  *_ERA_4_Silicon/     — One folder per plugin with .h/.c DSP core
reverse/scripts/       — Ghidra decompilation scripts (for reproducing the work)
assets/                — SVG/PNG assets for plugin editors
JUCE/                  — JUCE framework (submodule)
```

## How This Was Made

1. The original ERA 4 x86_64 AU binaries were decompiled using Ghidra headless
2. RTTI and vtable recovery identified class hierarchies and method dispatch
3. The pseudo-C was cleaned up: proper types, named structs, readable function names
4. x86 SSE intrinsics were replaced with portable C and Apple Accelerate (vDSP)
5. Each DSP core was verified to compile cleanly as C17 on arm64
6. JUCE wrappers expose the parameters and build as AU/VST3/Standalone

## License

The original ERA 4 plugins were released as freeware by accusonus before the company was acquired and shut down. This project preserves them for the audio community. The JUCE framework is used under its own license terms.
