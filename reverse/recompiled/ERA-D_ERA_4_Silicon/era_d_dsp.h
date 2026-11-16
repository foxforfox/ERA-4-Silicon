/*
 * era_d_dsp.h — ERA-D ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 ERA-D x86_64 binary (reverse/ERA-D/ERA-D.c).
 *
 * Architecture (from decompiled class names and lambda captures):
 *   ERA-D is a dual-engine plugin combining:
 *     - ERA::NoiseReducer   (noise reduction — same as NoiseRemover)
 *     - ERA::ReverbReducer  (dereverberation — same as ReverbRemover)
 *
 *   Both engines share the same STFT infrastructure (FFT order 11) and
 *   process sequentially: noise reduction first, then dereverberation.
 *   Each has independent processing amount and frequency focus controls.
 *
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Latency: 2048 samples (engines share the STFT frame)
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef ERA_D_DSP_H
#define ERA_D_DSP_H

#include "../NoiseRemover_ERA_4_Silicon/noise_remover_dsp.h"
#include "../ReverbRemover_ERA_4_Silicon/reverb_remover_dsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* Sub-engines */
    NoiseRemoverState   noise_engine;
    ReverbRemoverState  reverb_engine;

    /* Master output gain */
    float       output_gain;         /* linear */
} EraDState;

/* ── Public API ────────────────────────────────────────────────────── */

void era_d_init(EraDState *state);
void era_d_prepare(EraDState *state, double sample_rate,
                   int max_block_size, int num_channels);
void era_d_process(EraDState *state, float **channel_data,
                   int num_channels, int num_samples);
void era_d_free(EraDState *state);

/* Noise engine controls */
void era_d_set_noise_processing(EraDState *state, float amount);
void era_d_set_noise_frequency_focus(EraDState *state, NoiseFrequencyFocus focus);

/* Reverb engine controls */
void era_d_set_reverb_processing(EraDState *state, float amount);
void era_d_set_reverb_frequency_focus(EraDState *state, ReverbFrequencyFocus focus);

/* Shared controls */
void era_d_set_output_gain(EraDState *state, float gain_db);

float era_d_get_noise_reduction_db(const EraDState *state);
float era_d_get_reverb_reduction_db(const EraDState *state);
int   era_d_get_latency(const EraDState *state);

#ifdef __cplusplus
}
#endif

#endif /* ERA_D_DSP_H */
