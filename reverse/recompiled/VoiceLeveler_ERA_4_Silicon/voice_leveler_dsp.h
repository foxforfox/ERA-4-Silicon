/*
 * voice_leveler_dsp.h — VoiceLeveler ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 Voice Leveler x86_64 binary (reverse/VoiceLeveler/ERA4_VoiceLeveler.c).
 *
 * Architecture (from decompiled class names):
 *   ERA::VoiceLevelerEngine — time-domain AGC (automatic gain control)
 *
 * The Voice Leveler is unique among ERA plugins: it does NOT use STFT.
 * Instead it uses:
 *   - Time-domain AGC with lookahead delay buffer
 *   - Dual RMS/peak envelope detection with separate attack/release
 *   - Noise gate to avoid boosting silence
 *   - Emphasis biquad filter (3500 Hz peak) for presence boost
 *   - Normal/Tight modes (tighter compresses more aggressively)
 *   - Breath control parameter
 *   - Target level: -18 dBFS
 *   - Latency: lookahead buffer size (sample_rate * 0.005 rounded up)
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef VOICE_LEVELER_DSP_H
#define VOICE_LEVELER_DSP_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VOICE_LEVELER_NORMAL = 0,
    VOICE_LEVELER_TIGHT  = 1
} VoiceLevelerMode;

/* ── Biquad filter state (emphasis EQ) ────────────────────────────── */

typedef struct {
    double b0, b1, b2;
    double a1, a2;
    double z1, z2;
} BiquadState;

/* ── Voice leveler processor state ────────────────────────────────── */

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* Lookahead delay buffer (circular) */
    float      *delay_buffer;
    int         delay_size;          /* samples of lookahead */
    int         delay_write_pos;

    /* Envelope detection */
    float       rms_env;             /* smoothed RMS envelope      */
    float       peak_env;            /* smoothed peak envelope      */
    float       rms_attack;          /* attack coefficient (fast)   */
    float       rms_release;         /* release coefficient (slow)  */
    float       peak_attack;
    float       peak_release;

    /* Gain smoothing */
    float       current_gain;        /* applied gain (smoothed)     */
    float       gain_attack;         /* gain attack coeff           */
    float       gain_release;        /* gain release coeff          */

    /* Noise gate */
    float       gate_threshold;      /* below this RMS, reduce gain */
    float       gate_env;            /* gate envelope follower      */
    bool        gate_open;

    /* Emphasis biquad (3500 Hz peak filter) */
    BiquadState emphasis_eq;
    bool        emphasis_enabled;

    /* Parameters */
    float       processing_amount;   /* 0.0 .. 1.0                 */
    VoiceLevelerMode mode;
    float       breath_control;      /* 0.0 .. 1.0                 */
    float       output_gain;         /* linear                     */

    /* Constants derived from parameters */
    float       target_level;        /* linear target (-18 dBFS)    */
    float       max_gain;            /* maximum boost allowed       */
    float       min_gain;            /* minimum attenuation         */
} VoiceLevelerState;

/* ── Public API ────────────────────────────────────────────────────── */

void voice_leveler_init(VoiceLevelerState *state);
void voice_leveler_prepare(VoiceLevelerState *state, double sample_rate,
                           int max_block_size, int num_channels);
void voice_leveler_process(VoiceLevelerState *state, float **channel_data,
                           int num_channels, int num_samples);
void voice_leveler_free(VoiceLevelerState *state);

void voice_leveler_set_processing(VoiceLevelerState *state, float amount);
void voice_leveler_set_mode(VoiceLevelerState *state, VoiceLevelerMode mode);
void voice_leveler_set_breath_control(VoiceLevelerState *state, float amount);
void voice_leveler_set_output_gain(VoiceLevelerState *state, float gain_db);

int  voice_leveler_get_latency(const VoiceLevelerState *state);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_LEVELER_DSP_H */
