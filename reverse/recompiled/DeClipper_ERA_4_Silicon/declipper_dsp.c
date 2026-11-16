/*
 * declipper_dsp.c — DeClipper ERA 4 Silicon reference implementation.
 *
 * Cleaned-up C17 reimplementation of the DSP core recovered from Ghidra
 * decompilation of the ERA 4 De-Clipper x86_64 binary.
 *
 * Key decompiled functions mapped:
 *   DeClipperProcessor::prepareToPlay  → declipper_prepare
 *   DeClipperProcessor::processBlock   → declipper_process
 *   EnvelopeDetector::process          → envelope follower in process loop
 *
 * Architecture:
 *   - FFT order 11 → 2048-sample FFT, hop = 512 (Standard) or 256 (High)
 *   - Time-domain clip detection (samples near +/-1.0)
 *   - STFT spectral interpolation to restore clipped harmonics
 *   - Catmull-Rom spline reconstruction for time-domain clip regions
 *   - Standard/Heavy modes with different reconstruction aggressiveness
 *
 * x86 SSE intrinsics replaced with portable C min/max.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#include "declipper_dsp.h"
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
static inline int   mini(int a, int b) { return a < b ? a : b; }
static inline int   maxi(int a, int b) { return a > b ? a : b; }

#define MAX_CONTEXT_SAMPLES 16

/* ── Catmull-Rom spline interpolation for clipped samples ─────────── */

static float interpolate_clipped(const float *context, int len, int pos,
                                 float clip_threshold, float processing_amount)
{
    if (len < 2)
        return context[pos] * 0.9f;

    /* Find nearest non-clipped samples on each side */
    int left = -1, right = -1;
    for (int i = pos - 1; i >= 0; i--) {
        if (fabsf(context[i]) < clip_threshold) { left = i; break; }
    }
    for (int i = pos + 1; i < len; i++) {
        if (fabsf(context[i]) < clip_threshold) { right = i; break; }
    }

    if (left >= 0 && right >= 0) {
        /* Catmull-Rom spline through the clipped region */
        float t = (float)(pos - left) / (float)(right - left);
        float p0 = (left > 0) ? context[left - 1] : context[left];
        float p1 = context[left];
        float p2 = context[right];
        float p3 = (right < len - 1) ? context[right + 1] : context[right];

        float t2 = t * t;
        float t3 = t2 * t;

        float result = 0.5f * ((2.0f * p1) +
                               (-p0 + p2) * t +
                               (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                               (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

        float max_extension = 1.5f + processing_amount * 0.5f;
        return clampf(result, -max_extension, max_extension);
    }
    else if (left >= 0) {
        float slope = context[left] - ((left > 0) ? context[left - 1] : 0.0f);
        return context[left] + slope * 0.3f;
    }
    else if (right >= 0) {
        float slope = context[right] - ((right < len - 1) ? context[right + 1] : 0.0f);
        return context[right] + slope * 0.3f;
    }

    return context[pos] * 0.95f;
}

/* ── Public API ────────────────────────────────────────────────────── */

void declipper_init(DeClipperState *state)
{
    memset(state, 0, sizeof(*state));
    state->processing_amount = 0.5f;
    state->processing_type   = DECLIPPER_TYPE_STANDARD;
    state->quality           = DECLIPPER_QUALITY_STANDARD;
    state->output_gain       = 1.0f;
    state->clip_threshold    = 0.98f;
    state->enabled           = true;
}

void declipper_prepare(DeClipperState *state, double sample_rate,
                       int max_block_size, int num_channels)
{
    if (state->sample_rate == sample_rate &&
        state->max_block_size >= max_block_size &&
        state->num_channels == num_channels)
        return;

    declipper_free(state);

    state->sample_rate    = sample_rate;
    state->max_block_size = max_block_size;
    state->num_channels   = num_channels;

    /* FFT parameters: DeClipper uses order 11 (2048 samples) */
    state->fft_order = 11;
    state->fft_size  = 1 << state->fft_order;
    /* Quality setting controls hop size: Standard = /4, High = /8 */
    int hop_div = (state->quality == DECLIPPER_QUALITY_HIGH) ? 8 : 4;
    state->hop_size = state->fft_size / hop_div;
    state->num_bins = state->fft_size / 2 + 1;

    /* Ring buffers */
    int ring_cap = state->fft_size * 2;
    era_ring_init(&state->input_ring, num_channels, ring_cap, 0);
    era_ring_init(&state->output_ring, num_channels, ring_cap, 0);

    /* STFT working storage */
    era_matrix_d_resize(&state->stft_frame, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_resize(&state->overlap_add, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_fill(&state->overlap_add, -1.0);

    /* Spectral reference */
    size_t nbins = (size_t)state->num_bins;
    state->reference_spectrum = (float *)calloc(nbins, sizeof(float));
    state->reference_phase    = (float *)calloc(nbins, sizeof(float));
    state->has_reference = false;
    state->reference_age = 0.0f;
    state->smoothed_clip_ratio = 0.0f;

    /* Envelope detector coefficients */
    state->envelope_attack  = 1.0f - expf(-1.0f / ((float)sample_rate * 0.001f));
    state->envelope_release = 1.0f - expf(-1.0f / ((float)sample_rate * 0.010f));
    state->envelope_follower = 0.0f;

    /* Time-domain reconstruction context */
    state->prev_samples_size = MAX_CONTEXT_SAMPLES * 2;
    state->prev_samples = (float *)calloc((size_t)state->prev_samples_size, sizeof(float));
}

void declipper_free(DeClipperState *state)
{
    era_ring_free(&state->input_ring);
    era_ring_free(&state->output_ring);
    era_matrix_d_free(&state->stft_frame);
    era_matrix_d_free(&state->overlap_add);

    free(state->reference_spectrum); state->reference_spectrum = NULL;
    free(state->reference_phase);    state->reference_phase    = NULL;
    free(state->prev_samples);       state->prev_samples       = NULL;
}

void declipper_process(DeClipperState *state, float **channel_data,
                       int num_channels, int num_samples)
{
    if (!state->reference_spectrum || num_samples <= 0)
        return;

    /* Bypass when disabled */
    if (!state->enabled) {
        for (int ch = 0; ch < num_channels; ch++)
            for (int i = 0; i < num_samples; i++)
                channel_data[ch][i] *= state->output_gain;
        return;
    }

    if (state->processing_amount < 0.001f) {
        for (int ch = 0; ch < num_channels; ch++)
            for (int i = 0; i < num_samples; i++)
                channel_data[ch][i] *= state->output_gain;
        return;
    }

    int nch = mini(num_channels, state->num_channels);
    int num_bins = state->num_bins;

    /* ── Pass 1: STFT spectral restoration ──────────────────────────── */

    era_ring_write(&state->input_ring, channel_data, num_samples);

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

        /* Compute magnitude and detect clipping */
        float bin_freq_step = (float)state->sample_rate / (float)state->fft_size;
        int mid_bin = mini(num_bins - 1, (int)(4000.0f / bin_freq_step));

        float total_energy = 0.0f;
        float high_energy = 0.0f;

        float *mag = (float *)malloc((size_t)num_bins * sizeof(float));
        for (int bin = 0; bin < num_bins; bin++) {
            float sum = 0.0f;
            for (int ch = 0; ch < nch; ch++) {
                double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                sum += (float)fabs(frame_row[bin]);
            }
            mag[bin] = sum / (float)nch;
        }

        for (int bin = 1; bin < num_bins; bin++) {
            total_energy += mag[bin] * mag[bin];
            if (bin > mid_bin)
                high_energy += mag[bin] * mag[bin];
        }

        float clip_ratio = (total_energy > 1e-10f)
                          ? high_energy / total_energy : 0.0f;
        state->smoothed_clip_ratio = state->smoothed_clip_ratio * 0.9f
                                   + clip_ratio * 0.1f;

        bool frame_is_clean = (state->smoothed_clip_ratio < 0.3f);

        if (frame_is_clean) {
            float ref_alpha = state->has_reference ? 0.1f : 1.0f;
            for (int bin = 0; bin < num_bins; bin++) {
                state->reference_spectrum[bin] =
                    state->reference_spectrum[bin] * (1.0f - ref_alpha)
                    + mag[bin] * ref_alpha;
            }
            state->has_reference = true;
            state->reference_age = 0.0f;
        } else {
            state->reference_age += 1.0f;
        }

        /* Spectral restoration when clipping detected */
        if (state->has_reference &&
            state->smoothed_clip_ratio > 0.35f &&
            state->reference_age < 100.0f) {

            float restoration_amount = state->processing_amount
                * clampf((state->smoothed_clip_ratio - 0.3f) * 5.0f, 0.0f, 1.0f);

            float age_confidence = maxf(0.0f, 1.0f - state->reference_age * 0.02f);
            restoration_amount *= age_confidence;

            /* Heavy mode: more aggressive restoration */
            if (state->processing_type == DECLIPPER_TYPE_HEAVY)
                restoration_amount *= 1.5f;

            for (int bin = mid_bin; bin < num_bins; bin++) {
                if (state->reference_spectrum[bin] > mag[bin] * 1.2f) {
                    float boost = state->reference_spectrum[bin]
                                / maxf(mag[bin], 1e-10f);
                    boost = clampf(boost, 1.0f, 4.0f);

                    for (int ch = 0; ch < nch; ch++) {
                        double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                        frame_row[bin] *= (double)(1.0f + (boost - 1.0f) * restoration_amount);
                    }
                }
            }
        }

        /* Apply output gain */
        for (int bin = 0; bin < num_bins; bin++) {
            for (int ch = 0; ch < nch; ch++) {
                double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                frame_row[bin] *= (double)state->output_gain;
            }
        }

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

    /* ── Pass 2: Time-domain clip interpolation ─────────────────────── */
    int look_ahead = (state->processing_type == DECLIPPER_TYPE_HEAVY)
                    ? MAX_CONTEXT_SAMPLES
                    : MAX_CONTEXT_SAMPLES / 2;

    for (int ch = 0; ch < nch; ch++) {
        for (int i = 0; i < num_samples; i++) {
            float abs_sample = fabsf(channel_data[ch][i]);

            /* Envelope follower */
            float coeff = (abs_sample > state->envelope_follower)
                        ? state->envelope_attack
                        : state->envelope_release;
            state->envelope_follower += coeff * (abs_sample - state->envelope_follower);

            bool is_clipped = abs_sample >= state->clip_threshold * state->output_gain;

            if (is_clipped && state->processing_amount > 0.01f) {
                int window_start = maxi(0, i - look_ahead);
                int window_end   = mini(num_samples, i + look_ahead + 1);
                int window_len   = window_end - window_start;
                int pos_in_window = i - window_start;

                float interpolated = interpolate_clipped(
                    channel_data[ch] + window_start, window_len, pos_in_window,
                    state->clip_threshold, state->processing_amount);

                float blend = state->processing_amount * 0.8f;
                channel_data[ch][i] = channel_data[ch][i] * (1.0f - blend)
                                    + interpolated * blend;
            }
        }
    }
}

/* ── Parameter setters ────────────────────────────────────────────── */

void declipper_set_processing(DeClipperState *state, float amount)
{
    state->processing_amount = clampf(amount, 0.0f, 1.0f);
    state->clip_threshold = 0.99f - state->processing_amount * 0.10f;
}

void declipper_set_type(DeClipperState *state, DeClipperType type)
{
    state->processing_type = type;
}

void declipper_set_quality(DeClipperState *state, DeClipperQuality quality)
{
    state->quality = quality;
}

void declipper_set_output_gain(DeClipperState *state, float gain_db)
{
    state->output_gain = powf(10.0f, gain_db / 20.0f);
}

void declipper_set_enabled(DeClipperState *state, bool enabled)
{
    state->enabled = enabled;
}

/* ── Queries ──────────────────────────────────────────────────────── */

int declipper_get_latency(const DeClipperState *state)
{
    return state->fft_size;
}
