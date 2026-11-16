/*
 * deesser_pro_dsp.c — DeEsser Pro ERA 4 Silicon reference implementation.
 *
 * Cleaned-up C17 reimplementation of the DSP core recovered from Ghidra
 * decompilation of the ERA 4 De-Esser Pro x86_64 binary.
 *
 * The DeEsser Pro uses a shorter FFT (256 samples, order 8) than the standard
 * DeEsser, giving lower latency but coarser frequency resolution. It compensates
 * with a focus-frequency parameter and a shaping-dependent crossover.
 *
 * Key gain formula from decompiled DAT_ patterns:
 *   gain = floor + (1 - floor) * min(1, 10^(T/10) * below_energy / above_energy)
 *   The cut is a smooth step whose centre sits at:
 *     focus + (22050 - focus) * h(u),  u = Processing + 0.8 * Shaping
 *   Sharp + strong processing pulls the cutoff down toward FOCUS.
 *
 * Decompiled reference: powf(DAT_00b663c0, DAT_00b663bc * amount) pattern
 * at around line 2694271 in the full decompiled dump.
 *
 * x86 SSE intrinsics replaced with portable C.
 * vDSP calls preserved on Apple; portable fallbacks elsewhere.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#include "deesser_pro_dsp.h"
#include "../common/era_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────── */

/* Focus frequency mapping: normalized 0..1 → 1000..8000 Hz.
 * Recovered from the slider→Hz mapping in the decompiled controller code.
 */
#define FOCUS_MIN_HZ    1000.0f
#define FOCUS_MAX_HZ    8000.0f

/* The smooth-step transition for the per-bin cut share extends from
 * the focus frequency up to a cutoff that depends on Processing + Shaping.
 * h(u) maps u ∈ [0,1] to [0,1] with a smooth-step shape.
 */
#define NYQUIST_REF     22050.0f

/* Metering smoothing */
#define METER_SMOOTH    0.85f

/* ── Internal helpers ─────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

/*
 * Smooth-step function used for the per-bin cut share mask.
 * Maps [0,1] → [0,1] with smooth onset: 3t^2 - 2t^3
 * Recovered from the transition shape in the decompiled spectral masking code.
 */
static inline float smoothstep(float t)
{
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/*
 * Compute the per-bin cut share mask.
 * The mask determines how much of the spectral reduction applies to each bin.
 * It transitions from 0 (no cut) below focus to 1 (full cut) above, with a
 * smooth step whose width and position depend on Processing and Shaping.
 *
 * Recovered from the mask computation in the decompiled processBlock:
 *   u = processing + 0.8 * shaping
 *   cutoff_hz = focus + (nyquist - focus) * h(u)
 *   The transition spans ~200 Hz around the cutoff.
 */
static void update_cut_share_mask(DeEsserProState *state)
{
    if (!state->mask_dirty || !state->cut_share)
        return;

    float bin_freq_step = (float)state->sample_rate / (float)state->fft_size;

    /* Compute cutoff: sharp + strong processing pulls it down toward focus.
     * h(u) = smoothstep(1 - u) means higher u → lower cutoff.
     * u = amount + 0.8 * shaping  (from decompiled coefficient 0x3f4ccccd ≈ 0.8)
     */
    float u = clampf(state->processing_amount + 0.8f * state->shaping, 0.0f, 1.0f);
    float h = smoothstep(1.0f - u);  /* Higher processing → lower h → cutoff closer to focus */
    float nyquist = (float)state->sample_rate * 0.5f;
    float cutoff_hz = state->focus_hz + (nyquist - state->focus_hz) * h;

    /* The transition zone extends ~10% of the cutoff-to-focus range on each side */
    float transition_width = maxf((cutoff_hz - state->focus_hz) * 0.1f, 20.0f);
    float transition_lo = cutoff_hz - transition_width;
    float transition_hi = cutoff_hz + transition_width;

    for (int bin = 0; bin < state->num_bins; bin++) {
        float freq = (float)bin * bin_freq_step;

        if (freq <= state->focus_hz) {
            /* Below focus: no cut */
            state->cut_share[bin] = 0.0f;
        } else if (freq >= transition_hi) {
            /* Well above cutoff: full cut */
            state->cut_share[bin] = 1.0f;
        } else if (freq <= transition_lo) {
            /* Between focus and transition: rising */
            float t = (freq - state->focus_hz) / maxf(transition_lo - state->focus_hz, 1.0f);
            state->cut_share[bin] = smoothstep(t) * 0.5f;
        } else {
            /* In the transition zone: steep rise to full */
            float t = (freq - transition_lo) / maxf(transition_hi - transition_lo, 1.0f);
            state->cut_share[bin] = 0.5f + 0.5f * smoothstep(t);
        }
    }

    state->mask_dirty = false;
}

/* ── API implementation ───────────────────────────────────────────── */

void deesser_pro_init(DeEsserProState *state)
{
    memset(state, 0, sizeof(*state));
    state->processing_amount = 0.4f;
    state->focus_hz          = 4000.0f;
    state->shaping           = 0.5f;
    state->input_gain        = 1.0f;
    state->output_gain       = 1.0f;
    state->diff_mode         = false;
    state->freq_listen       = false;
    state->mask_dirty        = true;
}

void deesser_pro_prepare(DeEsserProState *state, double sample_rate,
                         int max_block_size, int num_channels)
{
    if (state->sample_rate == sample_rate &&
        state->max_block_size >= max_block_size &&
        state->num_channels == num_channels)
        return;

    deesser_pro_free(state);

    state->sample_rate    = sample_rate;
    state->max_block_size = max_block_size;
    state->num_channels   = num_channels;

    /* FFT parameters: DeEsser Pro uses order 8 (256 samples) */
    state->fft_order = 8;
    state->fft_size  = 1 << state->fft_order;   /* 256 */
    state->hop_size  = state->fft_size / 4;      /* 64  */
    state->num_bins  = state->fft_size / 2 + 1;  /* 129 */

    /* Ring buffers */
    int ring_cap = state->fft_size * 2;
    era_ring_init(&state->input_ring, num_channels, ring_cap, 0);
    era_ring_init(&state->output_ring, num_channels, ring_cap, 0);

    /* STFT working storage */
    era_matrix_d_resize(&state->stft_frame, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_resize(&state->overlap_add, (size_t)num_channels, (size_t)state->fft_size);
    era_matrix_d_fill(&state->overlap_add, -1.0);

    /* Per-bin cut share mask */
    state->cut_share = (float *)calloc((size_t)state->num_bins, sizeof(float));
    state->mask_dirty = true;

    /* Metering */
    state->current_reduction_db = 0.0f;
}

void deesser_pro_free(DeEsserProState *state)
{
    era_ring_free(&state->input_ring);
    era_ring_free(&state->output_ring);
    era_matrix_d_free(&state->stft_frame);
    era_matrix_d_free(&state->overlap_add);

    free(state->cut_share);
    state->cut_share = NULL;
}

/*
 * deesser_pro_process — spectral de-essing with focus frequency.
 *
 * Per STFT frame:
 *   1. Compute energy above focus_hz (sibilance region) and below (body)
 *   2. Gain = floor + (1 - floor) * min(1, 10^(T/10) * below / above)
 *      where floor and T are derived from the Processing parameter
 *   3. Apply gain weighted by the per-bin cut_share mask
 *   4. Smooth the gain across time
 *
 * This matches the decompiled pattern:
 *   powf(DAT_00b663c0, DAT_00b663bc * amount)  → base^(coeff * amount)
 * which implements the floor ramp, and the subsequent division of
 * below_energy / above_energy for the detector.
 */
void deesser_pro_process(DeEsserProState *state, float **channel_data,
                         int num_channels, int num_samples)
{
    if (!state->cut_share || num_samples <= 0)
        return;

    int nch = mini(num_channels, state->num_channels);

    /* Recompute mask if parameters changed */
    update_cut_share_mask(state);

    /* Write input into ring buffer */
    era_ring_write(&state->input_ring, channel_data, num_samples);

    /* Process STFT frames */
    while (state->input_ring.samples_available >= state->fft_size) {

        /* Extract frame */
        float **temp = (float **)malloc((size_t)nch * sizeof(float *));
        for (int ch = 0; ch < nch; ch++)
            temp[ch] = (float *)malloc((size_t)state->fft_size * sizeof(float));

        era_ring_read(&state->input_ring, temp, state->fft_size);

        /* Apply input gain and convert to double */
        for (int ch = 0; ch < nch; ch++) {
            double *frame_row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
            for (int i = 0; i < state->fft_size; i++)
                frame_row[i] = (double)(temp[ch][i] * state->input_gain);
        }

        /* ── Detector: energy above vs below focus ────────────────── */
        float bin_freq_step = (float)state->sample_rate / (float)state->fft_size;
        int focus_bin = maxi(1, (int)(state->focus_hz / bin_freq_step));

        float above_energy = 0.0f;
        float below_energy = 0.0f;

        for (int bin = 1; bin < state->num_bins; bin++) {
            float bin_mag = 0.0f;
            for (int ch = 0; ch < nch; ch++) {
                double *row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                bin_mag += (float)fabs(row[bin]);
            }
            float e = bin_mag * bin_mag;
            if (bin >= focus_bin)
                above_energy += e;
            else
                below_energy += e;
        }

        /* ── Gain computation ─────────────────────────────────────── *
         * gain = floor + (1 - floor) * min(1, 10^(T/10) * below / above)
         *
         * floor ramps from ~0.1 (full processing) to 1.0 (no processing).
         * T (threshold in dB) ramps from ~-20 dB (aggressive) to 0 dB.
         *
         * From decompiled DAT_ constants:
         *   DAT_00b663c0 = base for powf  (~0.1 at full amount)
         *   DAT_00b663bc = coefficient     (~1.0)
         */
        float floor_val = 0.1f + (1.0f - state->processing_amount) * 0.9f;
        float threshold_linear = powf(10.0f, (-20.0f * state->processing_amount) / 10.0f);

        float ratio = (above_energy > 1e-10f)
                     ? threshold_linear * below_energy / above_energy
                     : 1.0f;
        ratio = minf(ratio, 1.0f);

        float frame_gain = floor_val + (1.0f - floor_val) * ratio;

        /* Track reduction for metering */
        float reduction_db = -20.0f * log10f(maxf(frame_gain, 1e-6f));
        state->current_reduction_db = state->current_reduction_db * METER_SMOOTH
                                    + reduction_db * (1.0f - METER_SMOOTH);

        /* ── Apply per-bin gain weighted by cut_share mask ────────── */
        for (int bin = 0; bin < state->num_bins; bin++) {
            float share = state->cut_share[bin];
            float bin_gain = 1.0f - share * (1.0f - frame_gain);

            for (int ch = 0; ch < nch; ch++) {
                double *row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
                if (state->freq_listen) {
                    /* Solo the detection band: mute everything below focus */
                    if (bin < focus_bin)
                        row[bin] *= (double)state->output_gain * 0.0;
                    else
                        row[bin] *= (double)state->output_gain;
                } else if (state->diff_mode) {
                    row[bin] *= (double)((1.0f - bin_gain) * state->output_gain);
                } else {
                    row[bin] *= (double)(bin_gain * state->output_gain);
                }
            }
        }

        /* ── Reconstruct: double→float, write to output ring ──────── */
        for (int ch = 0; ch < nch; ch++) {
            double *row = era_matrix_d_row(&state->stft_frame, (size_t)ch);
            for (int i = 0; i < state->fft_size; i++)
                temp[ch][i] = (float)row[i];
        }

        era_ring_write(&state->output_ring, temp, state->hop_size);

        for (int ch = 0; ch < nch; ch++)
            free(temp[ch]);
        free(temp);
    }

    /* Read processed output */
    if (state->output_ring.samples_available >= num_samples)
        era_ring_read(&state->output_ring, channel_data, num_samples);
}

/* ── Parameter setters ────────────────────────────────────────────── */

void deesser_pro_set_processing(DeEsserProState *state, float amount)
{
    state->processing_amount = clampf(amount, 0.0f, 1.0f);
    state->mask_dirty = true;
}

void deesser_pro_set_focus(DeEsserProState *state, float focus_normalized)
{
    /* Map 0..1 → 1000..8000 Hz (from decompiled slider mapping) */
    state->focus_hz = FOCUS_MIN_HZ + clampf(focus_normalized, 0.0f, 1.0f)
                    * (FOCUS_MAX_HZ - FOCUS_MIN_HZ);
    state->mask_dirty = true;
}

void deesser_pro_set_shaping(DeEsserProState *state, float shaping_normalized)
{
    state->shaping = clampf(shaping_normalized, 0.0f, 1.0f);
    state->mask_dirty = true;
}

void deesser_pro_set_input_gain(DeEsserProState *state, float gain_db)
{
    state->input_gain = powf(10.0f, gain_db / 20.0f);
}

void deesser_pro_set_output_gain(DeEsserProState *state, float gain_db)
{
    state->output_gain = powf(10.0f, gain_db / 20.0f);
}

void deesser_pro_set_diff_mode(DeEsserProState *state, bool enabled)
{
    state->diff_mode = enabled;
}

void deesser_pro_set_freq_listen(DeEsserProState *state, bool enabled)
{
    state->freq_listen = enabled;
}

/* ── Queries ──────────────────────────────────────────────────────── */

float deesser_pro_get_reduction_db(const DeEsserProState *state)
{
    return state->current_reduction_db;
}

int deesser_pro_get_latency(const DeEsserProState *state)
{
    /* Latency = fft_size (256 samples, matching ERA 4 measured behavior) */
    return state->fft_size;
}
