/*
 * declipper_dsp.h — DeClipper ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 De-Clipper x86_64 binary (reverse/DeClipper/ERA4_DeClipper.c).
 *
 * Architecture (from decompiled class names):
 *   DeClipperProcessor + EnvelopeDetector
 *
 * The De-Clipper uses:
 *   - FFT order 11 → 2048-sample FFT, hop = 512 (Standard) or 256 (High quality)
 *   - Time-domain clip detection (samples near +/-1.0)
 *   - STFT spectral interpolation to restore clipped harmonics
 *   - Catmull-Rom spline reconstruction for time-domain clip regions
 *   - Standard/Heavy modes with different reconstruction aggressiveness
 *   - Standard/High quality settings (hop divisor 4 vs 8)
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef DECLIPPER_DSP_H
#define DECLIPPER_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DECLIPPER_TYPE_STANDARD = 0,
    DECLIPPER_TYPE_HEAVY    = 1
} DeClipperType;

typedef enum {
    DECLIPPER_QUALITY_STANDARD = 0,
    DECLIPPER_QUALITY_HIGH     = 1
} DeClipperQuality;

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 2048 */
    int         fft_order;       /* 11   */
    int         hop_size;        /* 512 (Standard) or 256 (High) */
    int         num_bins;        /* 1025 */

    /* Ring buffers */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage */
    EraMatrix_d    stft_frame;
    EraMatrix_d    overlap_add;

    /* Spectral reference (clean frame for harmonic restoration) */
    float          *reference_spectrum;   /* [num_bins] */
    float          *reference_phase;      /* [num_bins] */
    float           reference_age;
    bool            has_reference;

    /* Clip detection state */
    float           smoothed_clip_ratio;
    float           clip_threshold;       /* 0.89 .. 0.99 */
    float           envelope_follower;
    float           envelope_attack;
    float           envelope_release;

    /* Parameters */
    float           processing_amount;
    DeClipperType   processing_type;
    DeClipperQuality quality;
    float           output_gain;
    bool            enabled;

    /* Time-domain reconstruction context */
    float          *prev_samples;         /* context for spline interpolation */
    int             prev_samples_size;
} DeClipperState;

/* ── Public API ────────────────────────────────────────────────────── */

void declipper_init(DeClipperState *state);
void declipper_prepare(DeClipperState *state, double sample_rate,
                       int max_block_size, int num_channels);
void declipper_process(DeClipperState *state, float **channel_data,
                       int num_channels, int num_samples);
void declipper_free(DeClipperState *state);

void declipper_set_processing(DeClipperState *state, float amount);
void declipper_set_type(DeClipperState *state, DeClipperType type);
void declipper_set_quality(DeClipperState *state, DeClipperQuality quality);
void declipper_set_output_gain(DeClipperState *state, float gain_db);
void declipper_set_enabled(DeClipperState *state, bool enabled);

int  declipper_get_latency(const DeClipperState *state);

#ifdef __cplusplus
}
#endif

#endif /* DECLIPPER_DSP_H */
