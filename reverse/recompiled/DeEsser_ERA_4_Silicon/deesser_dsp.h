/*
 * deesser_dsp.h — DeEsser ERA 4 Silicon reference implementation.
 *
 * Cleaned-up types and function prototypes recovered from Ghidra decompilation
 * of the ERA 4 De-Esser x86_64 binary (reverse/DeEsser/ERA4_DeEsser.c).
 *
 * Key functions recovered:
 *   FUN_007b7d30 — prepareToPlay (sample rate change, engine allocation)
 *   FUN_007b97b0 — processBlock (main audio callback)
 *   FUN_007ced10 — STFT frame extraction (float-to-double conversion)
 *   FUN_007cf800 — Matrix-vector multiply (spectral smoothing kernel)
 *   FUN_007cf080 — STFT overlap-add reconstruction
 *
 * The DeEsser uses a 2048-sample FFT (order 11), hop = 512, with spectral
 * gain masking in the sibilance band (4000-12000 Hz depending on mode).
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef DEESSER_DSP_H
#define DEESSER_DSP_H

#include "../common/era_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Processing modes (from ButtonGroupSelector in decompiled code) ── */

typedef enum {
    DEESSER_MODE_NARROW = 0,   /* Tight sibilance band */
    DEESSER_MODE_NORMAL = 1,   /* Default band         */
    DEESSER_MODE_BROAD  = 2    /* Wide band            */
} DeEsserMode;

/* ── De-Esser processor state ──────────────────────────────────────── *
 * Struct layout recovered from offset patterns in FUN_007b97b0:
 *   param_1 + 0x110  = num_input_channels
 *   param_1 + 0x114  = num_output_channels
 *   param_1 + 0x1b0  = pointer to EraEngine
 *   param_1 + 0x1b8  = pointer to EraConfig
 *   param_1 + 0xbe0  = last sample rate (double)
 *   param_1 + 0xbe8  = last block size (double)
 *   param_1 + 0xbf0  = output selector (0=normal, nonzero=diff)
 *   param_1 + 0xbf4  = needs-update flag (atomic)
 *   param_1 + 0xac8  = plosive output buffer pointers
 *   param_1 + 0xabc  = plosive output num samples
 */

typedef struct {
    double      sample_rate;
    int         max_block_size;
    int         num_channels;

    /* STFT engine */
    int         fft_size;        /* 2048 for DeEsser (from prepareToPlay) */
    int         fft_order;       /* 11                                    */
    int         hop_size;        /* fft_size / 4 = 512                    */
    int         num_bins;        /* fft_size / 2 + 1 = 1025              */

    /* Ring buffers for overlap-add */
    EraRingBuffer  input_ring;
    EraRingBuffer  output_ring;

    /* STFT working storage (double-precision internally) */
    EraMatrix_d    stft_frame;      /* [num_channels x fft_size] double  */
    EraMatrix_d    stft_spectrum;   /* spectral data for processing      */
    EraMatrix_d    overlap_add;     /* OLA accumulator                   */

    /* Sub-band buffers (four bands: full, low, mid, high) */
    float         **band_buf[4];    /* [4][num_channels][num_bins]       */
    int             band_num_samples[4];
    bool            band_silent[4];

    /* Per-bin gain mask and smoothing */
    float          *gain_mask;      /* [num_bins] current gain per bin    */
    float          *smoothed_gain;  /* [num_bins] temporally smoothed     */

    /* Parameters */
    float           processing_amount;   /* 0.0 .. 1.0                   */
    DeEsserMode     mode;
    float           output_gain;         /* linear gain                   */
    bool            diff_mode;           /* listen to removed signal      */
    int             output_selector;     /* 0=normal, 1=band1, 2=band2   */

    /* Metering */
    float           current_reduction_db; /* atomic in original           */
    float           rms_level;

    /* Sibilance detector state */
    float           sibilance_probability;
    float           sibilance_attack;    /* ~0.3 from decompiled init    */
    float           sibilance_release;   /* ~0.05                        */
    float           gain_smooth_coeff;   /* ~0.3                         */
} DeEsserState;

/* ── Public API ────────────────────────────────────────────────────── */

void deesser_init(DeEsserState *state);
void deesser_prepare(DeEsserState *state, double sample_rate, int max_block_size, int num_channels);
void deesser_process(DeEsserState *state, float **channel_data, int num_channels, int num_samples);
void deesser_free(DeEsserState *state);

void deesser_set_processing(DeEsserState *state, float amount);
void deesser_set_mode(DeEsserState *state, DeEsserMode mode);
void deesser_set_output_gain(DeEsserState *state, float gain_db);
void deesser_set_diff_mode(DeEsserState *state, bool enabled);

float deesser_get_reduction_db(const DeEsserState *state);
int   deesser_get_latency(const DeEsserState *state);

#ifdef __cplusplus
}
#endif

#endif /* DEESSER_DSP_H */
