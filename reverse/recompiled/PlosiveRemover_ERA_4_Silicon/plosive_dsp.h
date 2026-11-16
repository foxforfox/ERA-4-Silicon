/*
 * plosive_dsp.h — PlosiveRemover ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 Plosive Remover x86_64 binary.
 *
 * Key decompiled source files:
 *   reverse/PlosiveRemover/processor.c    — prepareToPlay, processBlock stub
 *   reverse/PlosiveRemover/pb_full.c      — full processBlock with all code paths
 *   reverse/PlosiveRemover/engine.c       — STFT, ring buffers, matrix operations
 *   reverse/PlosiveRemover/core.c         — additional engine routines
 *   reverse/PlosiveRemover/classes.txt    — RTTI class hierarchy
 *
 * Class hierarchy (from classes.txt):
 *   PlosiveRemoverProcessor → ERAProcessor (vtable: 69 slots)
 *   ERAStandardController   (vtable: 6 sub-vtables)
 *
 * The PlosiveRemover uses:
 *   - FFT order 12 → 4096-sample FFT, hop = 1024, 4× overlap
 *   - Low-band plosive energy detection (DC up to ~77.5 Hz)
 *   - 3-frame energy history for temporal stability
 *   - Gain formula: floor + (1 - floor) * min(1, (10^(T/10) * high/low)^k)
 *   - Spectral cut: DC up to an edge (126..202 Hz) with linear-gain taper
 *   - Normal/Extreme modes (Extreme: 1.485× wider cut edge)
 *   - Latency: 4096 samples
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef PLOSIVE_DSP_H
#define PLOSIVE_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Processing modes ─────────────────────────────────────────────── */

typedef enum {
    PLOSIVE_MODE_NORMAL  = 0,
    PLOSIVE_MODE_EXTREME = 1
} PlosiveMode;

/* ── Plosive remover processor state ──────────────────────────────── *
 * Struct layout recovered from offset patterns in FUN_007b97b0:
 *   param_1 + 0x110 = num_input_channels
 *   param_1 + 0x1b0 = pointer to EraEngine (the 0x610-byte struct)
 *   Engine offsets recovered:
 *     + 0x138 = band[0] channel_data (full band)
 *     + 0x260 = band[1] channel_data (sub-band 1)
 *     + 0x388 = band[2] channel_data (sub-band 2)
 *     + 0x4b0 = band[3] channel_data (sub-band 3)
 *     + 0x5dc = num_channels
 *     + 0x5c8 = rms_level (double, atomic)
 *     + 0x5d0 = cutoff frequency (double)
 *     + 0x5e0 = low bin index
 *     + 0x5e4 = high bin index
 *     + 0x5e8 = sub-band index
 *     + 0x600 = output gain (float)
 *     + 0x606 = bypass flag
 *     + 0x608 = processing_mode (switch selector for STFT path)
 */

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 4096 for PlosiveRemover                 */
    int         fft_order;       /* 12                                      */
    int         hop_size;        /* fft_size / 4 = 1024                     */
    int         num_bins;        /* fft_size / 2 + 1 = 2049                 */

    /* Ring buffers for overlap-add */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage */
    EraMatrix_d    stft_frame;
    EraMatrix_d    overlap_add;

    /* Sub-band buffers (four bands from the ERA engine) */
    float         **band_buf[4];
    int             band_num_samples[4];
    bool            band_silent[4];

    /* Plosive detector state */
    float           low_hist[3];     /* low-band energy history (3 frames)  */
    float           high_hist[3];    /* high-band energy history (3 frames) */
    int             hist_pos;        /* circular index into history          */

    /* Parameters */
    float           processing_amount;   /* 0.0 .. 1.0                     */
    PlosiveMode     mode;
    float           output_gain;         /* linear gain                     */
    bool            diff_mode;           /* listen to removed signal        */

    /* Metering */
    float           current_reduction_db; /* atomic in original             */
} PlosiveState;

/* ── Public API ────────────────────────────────────────────────────── */

void plosive_init(PlosiveState *state);
void plosive_prepare(PlosiveState *state, double sample_rate,
                     int max_block_size, int num_channels);
void plosive_process(PlosiveState *state, float **channel_data,
                     int num_channels, int num_samples);
void plosive_free(PlosiveState *state);

void plosive_set_processing(PlosiveState *state, float amount);
void plosive_set_mode(PlosiveState *state, PlosiveMode mode);
void plosive_set_output_gain(PlosiveState *state, float gain_db);
void plosive_set_diff_mode(PlosiveState *state, bool enabled);

float plosive_get_reduction_db(const PlosiveState *state);
int   plosive_get_latency(const PlosiveState *state);

#ifdef __cplusplus
}
#endif

#endif /* PLOSIVE_DSP_H */
