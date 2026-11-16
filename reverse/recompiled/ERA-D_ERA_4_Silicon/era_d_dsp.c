/*
 * era_d_dsp.c — ERA-D ERA 4 Silicon reference implementation.
 *
 * Cleaned-up C17 reimplementation of the DSP core recovered from Ghidra
 * decompilation of the ERA 4 ERA-D x86_64 binary.
 *
 * Key decompiled functions mapped:
 *   ERA-D::prepareToPlay  → era_d_prepare  (allocates both sub-engines)
 *   ERA-D::processBlock   → era_d_process  (sequential: noise then reverb)
 *
 * Architecture (from lambda function names in decompiled code):
 *   ERA-D is a dual-engine plugin combining:
 *     - ERA::NoiseReducer   (noise reduction)
 *     - ERA::ReverbReducer  (dereverberation)
 *
 *   Both engines share the same STFT infrastructure (FFT order 11) and
 *   process sequentially: noise reduction first, then dereverberation.
 *   Each has independent processing amount and frequency focus controls.
 *
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Latency: 2048 * 2 = 4096 samples (two sequential STFT engines)
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#include "era_d_dsp.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Public API ────────────────────────────────────────────────────── */

void era_d_init(EraDState *state)
{
    memset(state, 0, sizeof(*state));
    state->output_gain = 1.0f;

    noise_remover_init(&state->noise_engine);
    reverb_remover_init(&state->reverb_engine);
}

void era_d_prepare(EraDState *state, double sample_rate,
                   int max_block_size, int num_channels)
{
    state->sample_rate    = sample_rate;
    state->max_block_size = max_block_size;
    state->num_channels   = num_channels;

    /* Prepare both sub-engines independently.
     * In the decompiled code, ERA-D::prepareToPlay calls
     * NoiseReducer::prepareToPlay and ReverbReducer::prepareToPlay
     * sequentially with the same sample rate and block size.
     */
    noise_remover_prepare(&state->noise_engine, sample_rate,
                          max_block_size, num_channels);
    reverb_remover_prepare(&state->reverb_engine, sample_rate,
                           max_block_size, num_channels);
}

void era_d_free(EraDState *state)
{
    noise_remover_free(&state->noise_engine);
    reverb_remover_free(&state->reverb_engine);
}

void era_d_process(EraDState *state, float **channel_data,
                   int num_channels, int num_samples)
{
    if (num_samples <= 0 || num_channels <= 0)
        return;

    /* Sequential processing: noise reduction first, then dereverberation.
     * This matches the decompiled processBlock which calls:
     *   1. NoiseReducer::processBlock (lambda at ~line 28750)
     *   2. ReverbReducer::processBlock (lambda at ~line 28800)
     *
     * Each engine applies its own output gain internally, but we
     * set them to 1.0 (unity) and apply the master gain at the end.
     */

    /* 1. Noise reduction pass */
    noise_remover_process(&state->noise_engine, channel_data,
                          num_channels, num_samples);

    /* 2. Dereverberation pass */
    reverb_remover_process(&state->reverb_engine, channel_data,
                           num_channels, num_samples);

    /* 3. Master output gain */
    if (state->output_gain != 1.0f) {
        for (int ch = 0; ch < num_channels; ch++)
            for (int i = 0; i < num_samples; i++)
                channel_data[ch][i] *= state->output_gain;
    }
}

/* ── Noise engine controls ────────────────────────────────────────── */

void era_d_set_noise_processing(EraDState *state, float amount)
{
    noise_remover_set_processing(&state->noise_engine, amount);
}

void era_d_set_noise_frequency_focus(EraDState *state, NoiseFrequencyFocus focus)
{
    noise_remover_set_frequency_focus(&state->noise_engine, focus);
}

/* ── Reverb engine controls ───────────────────────────────────────── */

void era_d_set_reverb_processing(EraDState *state, float amount)
{
    reverb_remover_set_processing(&state->reverb_engine, amount);
}

void era_d_set_reverb_frequency_focus(EraDState *state, ReverbFrequencyFocus focus)
{
    reverb_remover_set_frequency_focus(&state->reverb_engine, focus);
}

/* ── Shared controls ──────────────────────────────────────────────── */

void era_d_set_output_gain(EraDState *state, float gain_db)
{
    state->output_gain = powf(10.0f, gain_db / 20.0f);
    /* Set sub-engine gains to unity; master gain applied after both passes */
    noise_remover_set_output_gain(&state->noise_engine, 0.0f);
    reverb_remover_set_output_gain(&state->reverb_engine, 0.0f);
}

/* ── Queries ──────────────────────────────────────────────────────── */

float era_d_get_noise_reduction_db(const EraDState *state)
{
    return noise_remover_get_reduction_db(&state->noise_engine);
}

float era_d_get_reverb_reduction_db(const EraDState *state)
{
    return reverb_remover_get_reduction_db(&state->reverb_engine);
}

int era_d_get_latency(const EraDState *state)
{
    /* Two sequential STFT engines */
    return noise_remover_get_latency(&state->noise_engine)
         + reverb_remover_get_latency(&state->reverb_engine);
}
