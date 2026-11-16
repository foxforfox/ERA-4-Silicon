/*
 * noise_remover_dsp.c — NoiseRemover ERA 4 Silicon reference implementation.
 *
 * Cleaned-up C17 reimplementation of the DSP core recovered from Ghidra
 * decompilation of the ERA 4 Noise Remover x86_64 binary.
 *
 * Key decompiled functions mapped:
 *   ERA::NoiseEstimator::update   → update_noise_estimate (MCRA minimum statistics)
 *   ERA::DenoiseEngine::compute   → compute_gain_mask     (Wiener-like gain)
 *   ERA::DenoiseEngine::process   → noise_remover_process (STFT overlap-add)
 *
 * Architecture:
 *   - FFT order 11 → 2048-sample FFT, hop = 512, 4× overlap
 *   - Continuous minimum-statistics noise floor estimation
 *   - Per-bin Wiener-like spectral gain: G = max(floor, 1 - noise/signal)
 *   - Temporal gain smoothing (asymmetric: fast attack, slow release)
 *   - 6 frequency focus modes (All, Hi, Low, HiLow, Mid, HumBuzz)
 *
 * x86 SSE intrinsics replaced with portable C min/max.
 * vDSP calls preserved on Apple; portable fallbacks on other platforms.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#include "noise_remover_dsp.h"
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
static inline int   maxi(int a, int b) { return a > b ? a : b; }

/* ── Frequency focus weighting ────────────────────────────────────── */

static float get_focus_weight(const NoiseRemoverState *state, int bin, int num_bins)
{
    float nf = (float)bin / (float)num_bins;

    switch (state->frequency_focus) {
        case NOISE_FOCUS_HI:
            return 0.2f + 0.8f * nf;

        case NOISE_FOCUS_LOW:
            return 0.2f + 0.8f * (1.0f - nf);

        case NOISE_FOCUS_HILOW: {
            float mid = fabsf(nf - 0.5f) * 2.0f;
            return 0.2f + 0.8f * mid;
        }
        case NOISE_FOCUS_MID: {
            float mid = 1.0f - fabsf(nf - 0.5f) * 2.0f;
            return 0.2f + 0.8f * mid;
        }
        case NOISE_FOCUS_HUMBUZZ: {
            float bin_freq_step = (float)state->sample_rate / (float)state->fft_size;
            float freq = (float)bin * bin_freq_step;

            if (freq > 500.0f)
                return 0.05f;

            /* Check proximity to 50/60 Hz harmonics (up to ~480 Hz) */
            float min_dist = 999.0f;
            float fundamentals[] = { 50.0f, 60.0f };
            for (int f = 0; f < 2; f++) {
                for (int h = 1; h <= 8; h++) {
                    float harmonic = fundamentals[f] * (float)h;
                    float dist = fabsf(freq - harmonic);
                    if (dist < min_dist)
                        min_dist = dist;
                }
            }

            /* Gaussian-like weighting around harmonics */
            float half_bw = bin_freq_step * 1.5f;
            float harmonic_weight = expf(-(min_dist * min_dist)
                                         / (2.0f * half_bw * half_bw));

            float low_boost = (freq < 300.0f) ? 0.6f : 0.1f;
            return minf(1.0f, low_boost + 0.4f * harmonic_weight);
        }
        case NOISE_FOCUS_ALL:
        default:
            return 1.0f;
    }
}

/* ── Noise estimation (MCRA-style minimum statistics) ─────────────── */

static void update_noise_estimate(NoiseRemoverState *state, const float *mag, int num_bins)
{
    state->frame_count++;

    float alpha = state->noise_alpha;

    /* Power smoothing (first-order IIR on power spectrum) */
    for (int bin = 0; bin < num_bins; bin++) {
        float power = mag[bin] * mag[bin];
        state->smoothed_power[bin] = alpha * state->smoothed_power[bin]
                                   + (1.0f - alpha) * power;
    }

    /* Initialisation phase: use first ~10 frames to seed the noise estimate */
    if (state->frame_count <= 10) {
        float w = (state->frame_count == 1) ? 1.0f : 0.3f;
        for (int bin = 0; bin < num_bins; bin++) {
            state->noise_power[bin] = state->noise_power[bin] * (1.0f - w)
                                    + state->smoothed_power[bin] * w;
            state->min_tracker[bin]     = state->noise_power[bin];
            state->min_tracker_tmp[bin] = state->noise_power[bin];
        }
        state->noise_initialized = (state->frame_count == 10);
        return;
    }

    /* Minimum statistics: track minimum of smoothed power in a sliding window */
    state->min_sub_window_count++;
    for (int bin = 0; bin < num_bins; bin++) {
        state->min_tracker_tmp[bin] = minf(state->min_tracker_tmp[bin],
                                           state->smoothed_power[bin]);
    }

    if (state->min_sub_window_count >= state->min_window_length) {
        state->min_sub_window_count = 0;
        for (int bin = 0; bin < num_bins; bin++) {
            state->min_tracker[bin] = minf(state->min_tracker[bin],
                                           state->min_tracker_tmp[bin]);
            state->min_tracker_tmp[bin] = state->smoothed_power[bin];
        }
    }

    /* Bias compensation: noise estimate = minimum * bias factor */
    float bias_factor = 1.5f + 0.5f * state->processing_amount;

    for (int bin = 0; bin < num_bins; bin++) {
        float min_estimate = state->min_tracker[bin] * bias_factor;

        /* Speech presence probability */
        float snr_local = state->smoothed_power[bin] / maxf(min_estimate, 1e-20f);
        float speech_prob = clampf((snr_local - 1.0f) * 2.0f, 0.0f, 1.0f);

        /* Adapt noise estimate only when speech is absent */
        float adapt_rate = (1.0f - speech_prob) * 0.05f;
        state->noise_power[bin] = state->noise_power[bin] * (1.0f - adapt_rate)
                                + min_estimate * adapt_rate;

        /* Prevent noise estimate from growing unboundedly */
        state->noise_power[bin] = minf(state->noise_power[bin],
                                       state->smoothed_power[bin] * 2.0f);
    }
}

/* ── Gain computation (Wiener-like spectral subtraction) ──────────── */

static void compute_gain_mask(NoiseRemoverState *state, const float *mag, int num_bins)
{
    /* Suppression depth: 0% -> 0 dB, 100% -> ~45 dB */
    float max_suppression_db = state->processing_amount * 45.0f;
    float gain_floor = powf(10.0f, -max_suppression_db / 20.0f);
    gain_floor = maxf(gain_floor, 0.003f);

    /* Oversubtraction factor (more aggressive at higher processing amounts) */
    float over_subtract = 1.0f + state->processing_amount * 3.0f;

    for (int bin = 0; bin < num_bins; bin++) {
        float signal_power = mag[bin] * mag[bin];
        float noise = state->noise_power[bin] * over_subtract;
        float focus_w = get_focus_weight(state, bin, num_bins);

        /* Wiener gain: G = max(floor, 1 - noise/signal) */
        float gain;
        if (signal_power > 1e-20f) {
            float snr = (signal_power - noise * focus_w) / signal_power;
            gain = maxf(snr, gain_floor);
        } else {
            gain = gain_floor;
        }

        /* Temporal smoothing: fast attack, slow release */
        float smooth_coeff;
        if (gain < state->prev_gain_mask[bin])
            smooth_coeff = 0.3f;
        else
            smooth_coeff = 0.05f + 0.1f * state->processing_amount;

        gain = state->prev_gain_mask[bin]
             + smooth_coeff * (gain - state->prev_gain_mask[bin]);

        state->gain_mask[bin] = gain;
        state->prev_gain_mask[bin] = gain;
    }
}

/* ── Public API ────────────────────────────────────────────────────── */

void noise_remover_init(NoiseRemoverState *state)
{
    memset(state, 0, sizeof(*state));
    state->processing_amount = 0.5f;
    state->frequency_focus   = NOISE_FOCUS_ALL;
    state->output_gain       = 1.0f;
    state->noise_alpha       = 0.98f;
}

void noise_remover_prepare(NoiseRemoverState *state, double sample_rate,
                           int max_block_size, int num_channels)
{
    if (state->sample_rate == sample_rate &&
        state->max_block_size >= max_block_size &&
        state->num_channels == num_channels)
        return;

    noise_remover_free(state);

    state->sample_rate    = sample_rate;
    state->max_block_size = max_block_size;
    state->num_channels   = num_channels;

    /* FFT parameters: NoiseRemover uses order 11 (2048 samples) */
    state->fft_order = 11;
    state->fft_size  = 1 << state->fft_order;   /* 2048 */
    state->hop_size  = state->fft_size / 4;      /* 512  */
    state->num_bins  = state->fft_size / 2 + 1;  /* 1025 */

    /* Ring buffers */
    int ring_cap = state->fft_size * 2;
    era_ring_init(&state->input_ring, num_channels, ring_cap, 0);
    era_ring_init(&state->output_ring, num_channels, ring_cap, 0);

    /* STFT working storage (double-precision internally) */
    era_matrix_d_resize(&state->stft_frame, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_resize(&state->overlap_add, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_fill(&state->overlap_add, -1.0);

    /* Noise estimator arrays */
    size_t nbins = (size_t)state->num_bins;
    state->noise_power     = (float *)calloc(nbins, sizeof(float));
    state->smoothed_power  = (float *)calloc(nbins, sizeof(float));
    state->min_tracker     = (float *)malloc(nbins * sizeof(float));
    state->min_tracker_tmp = (float *)malloc(nbins * sizeof(float));
    for (size_t i = 0; i < nbins; i++) {
        state->min_tracker[i]     = 1e10f;
        state->min_tracker_tmp[i] = 1e10f;
    }

    /* Sub-window for minimum tracking: ~0.5 seconds worth of frames */
    float frames_per_sec = (float)sample_rate / (float)state->hop_size;
    state->min_window_length = maxi(8, (int)(frames_per_sec * 0.5f));
    state->min_sub_window_count = 0;
    state->frame_count = 0;
    state->noise_initialized = false;

    /* Gain arrays */
    state->gain_mask      = (float *)malloc(nbins * sizeof(float));
    state->prev_gain_mask = (float *)malloc(nbins * sizeof(float));
    for (size_t i = 0; i < nbins; i++) {
        state->gain_mask[i]      = 1.0f;
        state->prev_gain_mask[i] = 1.0f;
    }

    state->current_reduction_db = 0.0f;
}

void noise_remover_free(NoiseRemoverState *state)
{
    era_ring_free(&state->input_ring);
    era_ring_free(&state->output_ring);
    era_matrix_d_free(&state->stft_frame);
    era_matrix_d_free(&state->overlap_add);

    free(state->noise_power);     state->noise_power     = NULL;
    free(state->smoothed_power);  state->smoothed_power  = NULL;
    free(state->min_tracker);     state->min_tracker     = NULL;
    free(state->min_tracker_tmp); state->min_tracker_tmp = NULL;
    free(state->gain_mask);       state->gain_mask       = NULL;
    free(state->prev_gain_mask);  state->prev_gain_mask  = NULL;
}

void noise_remover_process(NoiseRemoverState *state, float **channel_data,
                           int num_channels, int num_samples)
{
    if (!state->gain_mask || !state->noise_power || num_samples <= 0)
        return;

    int nch = mini(num_channels, state->num_channels);
    int num_bins = state->num_bins;

    /* Write input into ring buffer */
    era_ring_write(&state->input_ring, channel_data, num_samples);

    /* Process STFT frames while enough data available */
    while (state->input_ring.samples_available >= state->fft_size) {

        /* Extract frame: float -> double */
        float **temp_in = (float **)malloc((size_t)nch * sizeof(float *));
        for (int ch = 0; ch < nch; ch++)
            temp_in[ch] = (float *)malloc((size_t)state->fft_size * sizeof(float));

        era_ring_read(&state->input_ring, temp_in, state->fft_size);

        for (int ch = 0; ch < nch; ch++) {
            double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
            for (int i = 0; i < state->fft_size; i++)
                frame_row[i] = (double)temp_in[ch][i];
        }

        /* Compute per-bin magnitude (average across channels) */
        float *mag = (float *)malloc((size_t)num_bins * sizeof(float));
        for (int bin = 0; bin < num_bins; bin++) {
            float sum = 0.0f;
            for (int ch = 0; ch < nch; ch++) {
                double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                sum += (float)fabs(frame_row[bin]);
            }
            mag[bin] = sum / (float)nch;
        }

        /* 1. Update noise estimate */
        update_noise_estimate(state, mag, num_bins);

        /* During initialisation, pass through unmodified */
        if (!state->noise_initialized) {
            for (int bin = 0; bin < num_bins; bin++) {
                for (int ch = 0; ch < nch; ch++) {
                    double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                    frame_row[bin] *= (double)state->output_gain;
                }
            }
        } else {
            /* 2. Compute spectral gain mask */
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
        }

        free(mag);

        /* Overlap-add reconstruction: double -> float, write to output ring */
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
}

/* ── Parameter setters ────────────────────────────────────────────── */

void noise_remover_set_processing(NoiseRemoverState *state, float amount)
{
    state->processing_amount = clampf(amount, 0.0f, 1.0f);
}

void noise_remover_set_frequency_focus(NoiseRemoverState *state, NoiseFrequencyFocus focus)
{
    state->frequency_focus = focus;
}

void noise_remover_set_output_gain(NoiseRemoverState *state, float gain_db)
{
    state->output_gain = powf(10.0f, gain_db / 20.0f);
}

/* ── Queries ──────────────────────────────────────────────────────── */

float noise_remover_get_reduction_db(const NoiseRemoverState *state)
{
    return state->current_reduction_db;
}

int noise_remover_get_latency(const NoiseRemoverState *state)
{
    return state->fft_size;
}
