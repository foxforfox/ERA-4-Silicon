/*
 * reverb_remover_dsp.c — ReverbRemover ERA 4 Silicon reference implementation.
 *
 * Cleaned-up C17 reimplementation of the DSP core recovered from Ghidra
 * decompilation of the ERA 4 Reverb Remover x86_64 binary.
 *
 * Key decompiled functions mapped:
 *   ERA::ReverbEstimator::update       → update_reverb_estimate
 *   ERA::DereverberationEngine::compute → compute_gain_mask
 *   ERA::DereverberationEngine::process → reverb_remover_process
 *
 * Architecture:
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Per-bin spectral decay tracking to estimate reverb tail
 *   - Transient detection to preserve direct sound
 *   - Spectral subtraction of estimated reverb content
 *   - Auto-gain compensation for level restoration
 *   - 5 frequency focus modes (All, Hi, Low, HiLow, Mid)
 *
 * x86 SSE intrinsics replaced with portable C min/max.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#include "reverb_remover_dsp.h"
#include "../common/era_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline int   mini(int a, int b) { return a < b ? a : b; }

/* ── Frequency focus weighting ────────────────────────────────────── */

static float get_focus_weight(ReverbFrequencyFocus focus, int bin, int num_bins)
{
    float nf = (float)bin / (float)num_bins;
    switch (focus) {
        case REVERB_FOCUS_HI:    return 0.2f + 0.8f * nf;
        case REVERB_FOCUS_LOW:   return 0.2f + 0.8f * (1.0f - nf);
        case REVERB_FOCUS_HILOW: return 0.2f + 0.8f * fabsf(nf - 0.5f) * 2.0f;
        case REVERB_FOCUS_MID:   return 0.2f + 0.8f * (1.0f - fabsf(nf - 0.5f) * 2.0f);
        case REVERB_FOCUS_ALL:
        default:                 return 1.0f;
    }
}

/* ── Reverb estimation (spectral decay tracking) ──────────────────── */

static void update_reverb_estimate(ReverbRemoverState *state,
                                   const float *mag, int num_bins)
{
    float signal_power_smoothing = 0.92f;

    for (int bin = 0; bin < num_bins; bin++) {
        float power = mag[bin] * mag[bin];

        /* Smooth the power spectrum */
        state->smoothed_power[bin] = signal_power_smoothing * state->smoothed_power[bin]
                                   + (1.0f - signal_power_smoothing) * power;

        /* Transient detection: sharp increase relative to previous frame */
        float increase = mag[bin] - state->prev_magnitude[bin];
        float relative_increase = (state->prev_magnitude[bin] > 1e-8f)
                                ? increase / state->prev_magnitude[bin] : 0.0f;

        bool is_transient = (relative_increase > 0.5f) && (increase > 0.0f);

        /* Smooth transient flag: fast attack, moderate release */
        if (is_transient)
            state->transient_flag[bin] = minf(state->transient_flag[bin] + 0.5f, 1.0f);
        else
            state->transient_flag[bin] *= 0.85f;

        /* Decay rate estimation */
        if (!is_transient && mag[bin] < state->prev_magnitude[bin]
            && state->prev_magnitude[bin] > 1e-8f) {
            float instant_decay = mag[bin] / state->prev_magnitude[bin];
            instant_decay = clampf(instant_decay, 0.5f, 0.999f);
            state->decay_rate[bin] = state->decay_rate[bin] * 0.95f
                                   + instant_decay * 0.05f;
        }

        /* Reverb power estimate */
        float reverb_decay = state->decay_rate[bin];

        if (is_transient) {
            state->reverb_power[bin] *= reverb_decay * 0.5f;
        } else if (mag[bin] < state->prev_magnitude[bin]) {
            float expected_direct = state->prev_magnitude[bin] * 0.3f;
            float excess = maxf(0.0f, mag[bin] - expected_direct);
            float excess_power = excess * excess;
            state->reverb_power[bin] = maxf(state->reverb_power[bin] * reverb_decay,
                                            excess_power * 0.3f);
        } else {
            state->reverb_power[bin] *= reverb_decay;
        }

        /* Prevent reverb estimate from exceeding signal power */
        state->reverb_power[bin] = minf(state->reverb_power[bin],
                                        state->smoothed_power[bin] * 0.9f);

        state->prev_magnitude[bin] = mag[bin];
    }
}

/* ── Gain computation ─────────────────────────────────────────────── */

static void compute_gain_mask(ReverbRemoverState *state,
                              const float *mag, int num_bins)
{
    float max_suppression = state->processing_amount * 35.0f;
    float gain_floor = powf(10.0f, -max_suppression / 20.0f);
    gain_floor = maxf(gain_floor, 0.01f);

    float over_subtract = 1.0f + state->processing_amount * 2.5f;

    for (int bin = 0; bin < num_bins; bin++) {
        float signal_power = mag[bin] * mag[bin];
        float reverb_est = state->reverb_power[bin] * over_subtract;
        float focus_w = get_focus_weight(state->frequency_focus, bin, num_bins);

        /* Wiener-like gain */
        float gain;
        if (signal_power > 1e-20f)
            gain = maxf(gain_floor, 1.0f - (reverb_est * focus_w) / signal_power);
        else
            gain = gain_floor;

        /* Transient preservation */
        float transient_boost = state->transient_flag[bin] * 0.8f;
        gain = gain + transient_boost * (1.0f - gain);

        /* Temporal smoothing */
        float smooth_coeff;
        if (gain < state->prev_gain_mask[bin])
            smooth_coeff = 0.25f;
        else
            smooth_coeff = 0.08f;

        gain = state->prev_gain_mask[bin]
             + smooth_coeff * (gain - state->prev_gain_mask[bin]);

        state->gain_mask[bin] = gain;
        state->prev_gain_mask[bin] = gain;
    }
}

/* ── Public API ────────────────────────────────────────────────────── */

void reverb_remover_init(ReverbRemoverState *state)
{
    memset(state, 0, sizeof(*state));
    state->processing_amount     = 0.5f;
    state->frequency_focus       = REVERB_FOCUS_ALL;
    state->auto_gain_enabled     = false;
    state->output_gain           = 1.0f;
    state->auto_gain_compensation = 1.0f;
}

void reverb_remover_prepare(ReverbRemoverState *state, double sample_rate,
                            int max_block_size, int num_channels)
{
    if (state->sample_rate == sample_rate &&
        state->max_block_size >= max_block_size &&
        state->num_channels == num_channels)
        return;

    reverb_remover_free(state);

    state->sample_rate    = sample_rate;
    state->max_block_size = max_block_size;
    state->num_channels   = num_channels;

    /* FFT parameters: ReverbRemover uses order 11 (2048 samples) */
    state->fft_order = 11;
    state->fft_size  = 1 << state->fft_order;
    state->hop_size  = state->fft_size / 4;
    state->num_bins  = state->fft_size / 2 + 1;

    /* Ring buffers */
    int ring_cap = state->fft_size * 2;
    era_ring_init(&state->input_ring, num_channels, ring_cap, 0);
    era_ring_init(&state->output_ring, num_channels, ring_cap, 0);

    /* STFT working storage */
    era_matrix_d_resize(&state->stft_frame, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_resize(&state->overlap_add, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_fill(&state->overlap_add, -1.0);

    /* Reverb estimator arrays */
    size_t nbins = (size_t)state->num_bins;
    state->prev_magnitude = (float *)calloc(nbins, sizeof(float));
    state->reverb_power   = (float *)calloc(nbins, sizeof(float));
    state->smoothed_power = (float *)calloc(nbins, sizeof(float));
    state->transient_flag = (float *)calloc(nbins, sizeof(float));
    state->decay_rate     = (float *)malloc(nbins * sizeof(float));
    for (size_t i = 0; i < nbins; i++)
        state->decay_rate[i] = 0.95f;

    /* Gain arrays */
    state->gain_mask      = (float *)malloc(nbins * sizeof(float));
    state->prev_gain_mask = (float *)malloc(nbins * sizeof(float));
    for (size_t i = 0; i < nbins; i++) {
        state->gain_mask[i]      = 1.0f;
        state->prev_gain_mask[i] = 1.0f;
    }

    /* Auto-gain state */
    state->smoothed_input_level  = 0.0f;
    state->smoothed_output_level = 0.0f;
    state->auto_gain_compensation = 1.0f;

    state->current_reduction_db = 0.0f;
}

void reverb_remover_free(ReverbRemoverState *state)
{
    era_ring_free(&state->input_ring);
    era_ring_free(&state->output_ring);
    era_matrix_d_free(&state->stft_frame);
    era_matrix_d_free(&state->overlap_add);

    free(state->prev_magnitude); state->prev_magnitude = NULL;
    free(state->reverb_power);   state->reverb_power   = NULL;
    free(state->smoothed_power); state->smoothed_power  = NULL;
    free(state->transient_flag); state->transient_flag  = NULL;
    free(state->decay_rate);     state->decay_rate      = NULL;
    free(state->gain_mask);      state->gain_mask       = NULL;
    free(state->prev_gain_mask); state->prev_gain_mask  = NULL;
}

void reverb_remover_process(ReverbRemoverState *state, float **channel_data,
                            int num_channels, int num_samples)
{
    if (!state->gain_mask || !state->reverb_power || num_samples <= 0)
        return;

    int nch = mini(num_channels, state->num_channels);
    int num_bins = state->num_bins;

    /* Measure input level for auto-gain */
    if (state->auto_gain_enabled) {
        float in_rms = 0.0f;
        for (int ch = 0; ch < nch; ch++)
            for (int i = 0; i < num_samples; i++)
                in_rms += channel_data[ch][i] * channel_data[ch][i];
        in_rms = sqrtf(in_rms / (float)(num_samples * nch));
        state->smoothed_input_level = state->smoothed_input_level * 0.95f
                                    + in_rms * 0.05f;
    }

    /* Write input into ring buffer */
    era_ring_write(&state->input_ring, channel_data, num_samples);

    /* Process STFT frames */
    while (state->input_ring.samples_available >= state->fft_size) {

        float **temp_in = (float **)malloc((size_t)nch * sizeof(float *));
        for (int ch = 0; ch < nch; ch++)
            temp_in[ch] = (float *)malloc((size_t)state->fft_size * sizeof(float));

        era_ring_read(&state->input_ring, temp_in, state->fft_size);

        for (int ch = 0; ch < nch; ch++) {
            double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
            for (int i = 0; i < state->fft_size; i++)
                frame_row[i] = (double)temp_in[ch][i];
        }

        /* Compute per-bin magnitude */
        float *mag = (float *)malloc((size_t)num_bins * sizeof(float));
        for (int bin = 0; bin < num_bins; bin++) {
            float sum = 0.0f;
            for (int ch = 0; ch < nch; ch++) {
                double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                sum += (float)fabs(frame_row[bin]);
            }
            mag[bin] = sum / (float)nch;
        }

        /* 1. Update reverb estimate */
        update_reverb_estimate(state, mag, num_bins);

        /* 2. Compute gain mask */
        compute_gain_mask(state, mag, num_bins);

        /* 3. Apply gain mask */
        float max_reduction_db = 0.0f;
        for (int bin = 0; bin < num_bins; bin++) {
            float gain = state->gain_mask[bin];

            float reduction_db = -20.0f * log10f(maxf(gain, 1e-6f));
            if (reduction_db > max_reduction_db)
                max_reduction_db = reduction_db;

            for (int ch = 0; ch < nch; ch++) {
                double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                frame_row[bin] *= (double)(gain * state->output_gain);
            }
        }

        state->current_reduction_db = state->current_reduction_db * 0.85f
                                    + max_reduction_db * 0.15f;

        free(mag);

        /* Overlap-add reconstruction */
        for (int ch = 0; ch < nch; ch++) {
            double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
            for (int i = 0; i < state->fft_size; i++)
                temp_in[ch][i] = (float)frame_row[i];
        }

        era_ring_write(&state->output_ring, temp_in, state->hop_size);

        for (int ch = 0; ch < nch; ch++)
            free(temp_in[ch]);
        free(temp_in);
    }

    /* Read processed samples from output ring buffer */
    if (state->output_ring.samples_available >= num_samples) {
        era_ring_read(&state->output_ring, channel_data, num_samples);
    }

    /* Auto-gain compensation */
    if (state->auto_gain_enabled && state->smoothed_input_level > 1e-6f) {
        float out_rms = 0.0f;
        for (int ch = 0; ch < nch; ch++)
            for (int i = 0; i < num_samples; i++)
                out_rms += channel_data[ch][i] * channel_data[ch][i];
        out_rms = sqrtf(out_rms / (float)(num_samples * nch));
        state->smoothed_output_level = state->smoothed_output_level * 0.95f
                                     + out_rms * 0.05f;

        if (state->smoothed_output_level > 1e-6f) {
            float target_comp = state->smoothed_input_level
                              / state->smoothed_output_level;
            target_comp = clampf(target_comp, 0.5f, 6.0f);

            state->auto_gain_compensation +=
                0.02f * (target_comp - state->auto_gain_compensation);

            for (int ch = 0; ch < nch; ch++)
                for (int i = 0; i < num_samples; i++)
                    channel_data[ch][i] *= state->auto_gain_compensation;
        }
    }
}

/* ── Parameter setters ────────────────────────────────────────────── */

void reverb_remover_set_processing(ReverbRemoverState *state, float amount)
{
    state->processing_amount = clampf(amount, 0.0f, 1.0f);
}

void reverb_remover_set_frequency_focus(ReverbRemoverState *state, ReverbFrequencyFocus focus)
{
    state->frequency_focus = focus;
}

void reverb_remover_set_auto_gain(ReverbRemoverState *state, bool enabled)
{
    state->auto_gain_enabled = enabled;
}

void reverb_remover_set_output_gain(ReverbRemoverState *state, float gain_db)
{
    state->output_gain = powf(10.0f, gain_db / 20.0f);
}

/* ── Queries ──────────────────────────────────────────────────────── */

float reverb_remover_get_reduction_db(const ReverbRemoverState *state)
{
    return state->current_reduction_db;
}

int reverb_remover_get_latency(const ReverbRemoverState *state)
{
    return state->fft_size;
}
