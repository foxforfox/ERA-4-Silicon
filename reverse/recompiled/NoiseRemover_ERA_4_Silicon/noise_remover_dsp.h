/*
 * noise_remover_dsp.h — NoiseRemover ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 Noise Remover x86_64 binary (reverse/NoiseRemover/ERA4_NoiseRemover.c).
 *
 * Architecture (from decompiled class names and vtable structure):
 *   ERA::DenoiseEngine  — Wiener-like spectral gain computation
 *   ERA::NoiseEstimator — MCRA-style minimum-statistics noise floor estimation
 *
 * The Noise Remover uses:
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Continuous minimum-statistics noise floor estimation
 *   - Per-bin Wiener-like spectral gain: G = max(floor, 1 - noise/signal)
 *   - Temporal gain smoothing (asymmetric: fast attack, slow release)
 *   - 6 frequency focus modes (All, Hi, Low, HiLow, Mid, HumBuzz)
 *   - Latency: 2048 samples
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef NOISE_REMOVER_DSP_H
#define NOISE_REMOVER_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Frequency focus modes (from ERA 4 ButtonGroupSelector) ──────── */

typedef enum {
    NOISE_FOCUS_ALL     = 0,
    NOISE_FOCUS_HI      = 1,
    NOISE_FOCUS_LOW     = 2,
    NOISE_FOCUS_HILOW   = 3,
    NOISE_FOCUS_MID     = 4,
    NOISE_FOCUS_HUMBUZZ = 5
} NoiseFrequencyFocus;

/* ── Noise remover processor state ────────────────────────────────── */

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 2048 */
    int         fft_order;       /* 11   */
    int         hop_size;        /* 512  */
    int         num_bins;        /* 1025 */

    /* Ring buffers for overlap-add */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage */
    EraMatrix_d    stft_frame;
    EraMatrix_d    overlap_add;

    /* Noise estimator state (ERA::NoiseEstimator) */
    float          *noise_power;        /* [num_bins] estimated noise power     */
    float          *smoothed_power;     /* [num_bins] smoothed signal power     */
    float          *min_tracker;        /* [num_bins] minimum tracker           */
    float          *min_tracker_tmp;    /* [num_bins] temporary minimum         */
    int             min_window_length;  /* sub-window length in frames          */
    int             min_sub_window_count;
    int             frame_count;
    bool            noise_initialized;
    float           noise_alpha;        /* smoothing for noise power update     */

    /* Gain computation state (ERA::DenoiseEngine) */
    float          *gain_mask;          /* [num_bins] current gain per bin      */
    float          *prev_gain_mask;     /* [num_bins] previous frame gain       */

    /* Parameters */
    float           processing_amount;  /* 0.0 .. 1.0                          */
    NoiseFrequencyFocus frequency_focus;
    float           output_gain;        /* linear                              */

    /* Metering */
    float           current_reduction_db;
} NoiseRemoverState;

/* ── Public API ────────────────────────────────────────────────────── */

void noise_remover_init(NoiseRemoverState *state);
void noise_remover_prepare(NoiseRemoverState *state, double sample_rate,
                           int max_block_size, int num_channels);
void noise_remover_process(NoiseRemoverState *state, float **channel_data,
                           int num_channels, int num_samples);
void noise_remover_free(NoiseRemoverState *state);

void noise_remover_set_processing(NoiseRemoverState *state, float amount);
void noise_remover_set_frequency_focus(NoiseRemoverState *state, NoiseFrequencyFocus focus);
void noise_remover_set_output_gain(NoiseRemoverState *state, float gain_db);

float noise_remover_get_reduction_db(const NoiseRemoverState *state);
int   noise_remover_get_latency(const NoiseRemoverState *state);

#ifdef __cplusplus
}
#endif

#endif /* NOISE_REMOVER_DSP_H */
