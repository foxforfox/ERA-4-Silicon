/*
 * era_types.h — Common types recovered from Ghidra decompilation of ERA 4 plugins.
 *
 * These struct layouts are reconstructed from pointer arithmetic patterns in the
 * decompiled code (e.g., *(float *)(param_1 + 0x138) becomes struct member access).
 * Field names are inferred from usage context; the originals are unknown.
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef ERA_TYPES_H
#define ERA_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Ring buffer (circular buffer for STFT overlap) ────────────────── */

typedef struct {
    int   num_channels;       /* number of audio channels                  */
    int   buffer_size;        /* total capacity per channel (samples)      */
    int   write_offset;       /* next write position in ring               */
    int   read_offset;        /* next read position in ring                */
    int   samples_written;    /* total samples pushed since last reset     */
    int   samples_available;  /* samples available to read                 */
    float **channel_data;     /* [num_channels] pointers to float arrays   */
} EraRingBuffer;

/* ── 2-D array wrappers (rows x cols, contiguous storage) ─────────── */

typedef struct {
    double *data;
    size_t  rows;
    size_t  cols;
} EraMatrix_d;   /* element size 8 (double) */

typedef struct {
    float *data;
    size_t rows;
    size_t cols;
} EraMatrix_f;   /* element size 4 (float) */

typedef struct {
    double *data;
    size_t  count;
} EraVector_d;

typedef struct {
    float *data;
    size_t count;
} EraVector_f;

/* ── Multiband buffer set (four subbands used by the ERA engine) ──── */

typedef struct {
    float **channel_data;     /* [num_channels] */
    int     num_channels;
    int     num_samples;
    bool    silent;           /* true = all zeros (skip processing) */
    /* padding to 0x128 stride in original — sub-bands separated by 0x128 */
} EraBandBuffer;

/* ── DSP engine inner state (the 0x610-byte allocation in prepareToPlay) */

typedef struct {
    void           *config;          /* offset 0x00: pointer to config struct  */
    void           *inner_state;     /* offset 0x08: pointer to inner buffers  */
    int             hop_size;        /* offset 0x10: hop = fft_size / overlap  */
    int             fft_size;        /* offset 0x14: FFT size (next pow2)      */
    int             overlap;         /* offset 0x18: overlap factor             */
    double          freq_per_bin;    /* offset 0x20: sampleRate / fft_size     */
    double          time_per_hop;    /* offset 0x28: hop_size / sampleRate     */

    /* STFT frame storage (double-precision internally) */
    EraMatrix_d     stft_in;         /* offset 0x28+: input STFT frames        */
    EraMatrix_d     stft_work;       /* working STFT data                      */
    EraMatrix_d     stft_out;        /* output STFT data                       */
    EraMatrix_d     stft_overlap;    /* overlap-add accumulator                */
    EraMatrix_f     magnitude;       /* per-bin magnitudes (float)             */
    EraMatrix_f     gain_mask;       /* per-bin gain mask (float)              */

    /* Per-channel smoothing/history buffers */
    EraVector_d     smooth_buf[8];   /* up to 8 smoothing buffers              */

    /* sub-band extraction buffers (four bands) */
    EraBandBuffer   band[4];         /* band[0] = full, [1..3] = sub-bands    */

    int             num_channels;    /* at offset 0x5dc in decompiled code     */
    double          rms_level;       /* at offset 0x5c8: detected RMS          */
    float           output_gain;     /* at offset 0x600                        */
    bool            bypass;          /* at offset 0x606                        */

    /* Engine processing mode selector */
    int             processing_mode; /* at offset 0x608                        */
} EraEngine;

/* ── Configuration block (the 0x74-byte allocation) ───────────────── */

typedef struct {
    int   version;
    int   fft_order_minus2;      /* fft_size minus 2 */
    int   field_08;              /* always 1 in init   */
    int   field_0c;              /* always 0 in init   */
    int   fft_order_minus1;      /* fft_size minus 1 */
    float field_14;              /* -100.0f            */
    int   field_18;              /* 3                  */
    float field_1c;              /* 0.0f               */
    float field_20;              /* 60.0f              */
    int   field_24;              /* 0                  */
    int   field_28;              /* 50                 */
    int   field_2c;              /* 10                 */

    /* Pairs of floats: threshold coefficients per processing mode */
    float coeffs[8];             /* offsets 0x30..0x4f */

    /* Mode flags */
    uint8_t  flag_60;
    uint8_t  flag_61;
    uint8_t  flag_62;
    uint8_t  pad_63;
    uint16_t mode_flags;         /* offset 0x64        */
    uint16_t pad_66;
    int      field_68;           /* 0                  */
    int      field_6c;           /* 3                  */
    int      field_70;           /* 3                  */
} EraConfig;

#endif /* ERA_TYPES_H */
