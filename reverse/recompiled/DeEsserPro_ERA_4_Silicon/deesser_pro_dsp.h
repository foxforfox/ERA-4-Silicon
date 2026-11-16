/*
 * deesser_pro_dsp.h — DeEsser Pro ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 De-Esser Pro x86_64 binary (reverse/DeEsserPro/ERA4_DeEsserPro.c).
 *
 * The DeEsser Pro differs from the standard DeEsser:
 *   - FFT order 8 → 256-sample FFT, hop = 64, 4× overlap
 *   - Additional Focus parameter (frequency selector, 1000–8000 Hz)
 *   - Shaping parameter (soft/sharp crossover rolloff)
 *   - Detector: energy above FOCUS vs below
 *   - Per-bin cut share mask (smooth step above focus frequency)
 *   - Latency: 256 samples (matching ERA 4 measured behavior)
 *
 * Key gain formula recovered from decompiled DAT_ patterns:
 *   gain = floor + (1 - floor) * min(1, 10^(T/10) * below / above)
 *   where floor and T come from the Processing parameter.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef DEESSER_PRO_DSP_H
#define DEESSER_PRO_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── De-Esser Pro processor state ─────────────────────────────────── *
 * Struct layout recovered from offset patterns in the decompiled code.
 * Similar to the standard DeEsser but with additional focus/shaping
 * parameters and a per-bin cut-share mask.
 */

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 256 for DeEsser Pro (from prepareToPlay) */
    int         fft_order;       /* 8                                        */
    int         hop_size;        /* fft_size / 4 = 64                        */
    int         num_bins;        /* fft_size / 2 + 1 = 129                   */

    /* Ring buffers for overlap-add */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage (double-precision internally) */
    EraMatrix_d    stft_frame;      /* [num_channels x fft_size] double */
    EraMatrix_d    overlap_add;     /* OLA accumulator                  */

    /* Per-bin cut share mask: how much of the reduction applies per bin.
     * This is the smooth-step transition above the focus frequency.
     * Recovered from the cutShare computation in decompiled processBlock.
     */
    float          *cut_share;      /* [num_bins]                       */
    bool            mask_dirty;     /* true when focus/shaping changed  */

    /* Parameters */
    float           processing_amount;   /* 0.0 .. 1.0                 */
    float           focus_hz;            /* 1000 .. 8000 Hz            */
    float           shaping;             /* 0.0 .. 1.0 (soft .. sharp) */
    float           input_gain;          /* linear                     */
    float           output_gain;         /* linear                     */
    bool            diff_mode;           /* listen to removed signal   */
    bool            freq_listen;         /* solo the detection band    */

    /* Metering */
    float           current_reduction_db; /* atomic in original        */
} DeEsserProState;

/* ── Public API ────────────────────────────────────────────────────── */

void deesser_pro_init(DeEsserProState *state);
void deesser_pro_prepare(DeEsserProState *state, double sample_rate,
                         int max_block_size, int num_channels);
void deesser_pro_process(DeEsserProState *state, float **channel_data,
                         int num_channels, int num_samples);
void deesser_pro_free(DeEsserProState *state);

void deesser_pro_set_processing(DeEsserProState *state, float amount);
void deesser_pro_set_focus(DeEsserProState *state, float focus_normalized);
void deesser_pro_set_shaping(DeEsserProState *state, float shaping_normalized);
void deesser_pro_set_input_gain(DeEsserProState *state, float gain_db);
void deesser_pro_set_output_gain(DeEsserProState *state, float gain_db);
void deesser_pro_set_diff_mode(DeEsserProState *state, bool enabled);
void deesser_pro_set_freq_listen(DeEsserProState *state, bool enabled);

float deesser_pro_get_reduction_db(const DeEsserProState *state);
int   deesser_pro_get_latency(const DeEsserProState *state);

#ifdef __cplusplus
}
#endif

#endif /* DEESSER_PRO_DSP_H */
