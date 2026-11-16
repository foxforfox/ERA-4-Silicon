/*
 * reverb_remover_dsp.h — ReverbRemover ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 Reverb Remover x86_64 binary (reverse/ReverbRemover/ERA4_ReverbRemover.c).
 *
 * Architecture (from decompiled class names):
 *   ERA::DereverberationEngine — spectral subtraction of reverb content
 *   ERA::ReverbEstimator       — per-bin spectral decay tracking
 *
 * The Reverb Remover uses:
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Per-bin spectral decay tracking to estimate reverb tail
 *   - Transient detection to preserve direct sound
 *   - Spectral subtraction of estimated reverb content
 *   - Auto-gain compensation for level restoration
 *   - 5 frequency focus modes (All, Hi, Low, HiLow, Mid)
 *   - Latency: 2048 samples
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef REVERB_REMOVER_DSP_H
#define REVERB_REMOVER_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REVERB_FOCUS_ALL    = 0,
    REVERB_FOCUS_HI     = 1,
    REVERB_FOCUS_LOW    = 2,
    REVERB_FOCUS_HILOW  = 3,
    REVERB_FOCUS_MID    = 4
} ReverbFrequencyFocus;

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 2048 */
    int         fft_order;       /* 11   */
    int         hop_size;        /* 512  */
    int         num_bins;        /* 1025 */

    /* Ring buffers */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage */
    EraMatrix_d    stft_frame;
    EraMatrix_d    overlap_add;

    /* Reverb estimator state */
    float          *prev_magnitude;      /* [num_bins] previous frame magnitude  */
    float          *reverb_power;        /* [num_bins] running reverb estimate   */
    float          *smoothed_power;      /* [num_bins] smoothed signal power     */
    float          *transient_flag;      /* [num_bins] transient probability     */
    float          *decay_rate;          /* [num_bins] estimated decay rate      */

    /* Gain computation */
    float          *gain_mask;           /* [num_bins] */
    float          *prev_gain_mask;      /* [num_bins] */

    /* Parameters */
    float           processing_amount;
    ReverbFrequencyFocus frequency_focus;
    bool            auto_gain_enabled;
    float           output_gain;

    /* Auto-gain state */
    float           smoothed_input_level;
    float           smoothed_output_level;
    float           auto_gain_compensation;

    /* Metering */
    float           current_reduction_db;
} ReverbRemoverState;

/* ── Public API ────────────────────────────────────────────────────── */

void reverb_remover_init(ReverbRemoverState *state);
void reverb_remover_prepare(ReverbRemoverState *state, double sample_rate,
                            int max_block_size, int num_channels);
void reverb_remover_process(ReverbRemoverState *state, float **channel_data,
                            int num_channels, int num_samples);
void reverb_remover_free(ReverbRemoverState *state);

void reverb_remover_set_processing(ReverbRemoverState *state, float amount);
void reverb_remover_set_frequency_focus(ReverbRemoverState *state, ReverbFrequencyFocus focus);
void reverb_remover_set_auto_gain(ReverbRemoverState *state, bool enabled);
void reverb_remover_set_output_gain(ReverbRemoverState *state, float gain_db);

float reverb_remover_get_reduction_db(const ReverbRemoverState *state);
int   reverb_remover_get_latency(const ReverbRemoverState *state);

#ifdef __cplusplus
}
#endif

#endif /* REVERB_REMOVER_DSP_H */
