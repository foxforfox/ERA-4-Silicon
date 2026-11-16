/*
 * era_util.h — Utility functions recovered from Ghidra decompilation.
 *
 * These are helper routines used across all three ERA 4 plugins:
 *   - Ring buffer management (FUN_007ce5d0, FUN_007cebe0, FUN_007cf6b0)
 *   - 2-D matrix allocation (FUN_007ce450, FUN_007ce510, FUN_007ce700)
 *   - 1-D vector allocation (FUN_007ce3c0)
 *
 * Compiles with: clang -std=c17 -target arm64-apple-macos12
 */

#ifndef ERA_UTIL_H
#define ERA_UTIL_H

#include "era_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

/* ── Next power of two ─────────────────────────────────────────────── */

static inline int era_next_pow2(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

/* ── Vector allocation (1-D double array) ──────────────────────────── *
 * Recovered from FUN_007ce3c0: allocates or resizes a double vector.
 */

static inline void era_vector_d_resize(EraVector_d *v, size_t count)
{
    if (v->count != count) {
        free(v->data);
        v->data = (count > 0) ? (double *)calloc(count, sizeof(double)) : NULL;
    }
    v->count = count;
}

static inline void era_vector_d_free(EraVector_d *v)
{
    free(v->data);
    v->data = NULL;
    v->count = 0;
}

/* ── Matrix allocation (2-D, contiguous) ───────────────────────────── *
 * Recovered from FUN_007ce450 (double), FUN_007ce700 (float).
 */

static inline void era_matrix_d_resize(EraMatrix_d *m, size_t rows, size_t cols)
{
    size_t total = rows * cols;
    if (m->rows * m->cols != total) {
        free(m->data);
        m->data = (total > 0) ? (double *)calloc(total, sizeof(double)) : NULL;
    }
    m->rows = rows;
    m->cols = cols;
}

static inline void era_matrix_d_free(EraMatrix_d *m)
{
    free(m->data);
    m->data = NULL;
    m->rows = m->cols = 0;
}

static inline void era_matrix_d_zero(EraMatrix_d *m)
{
    if (m->data && m->rows > 0 && m->cols > 0)
        memset(m->data, 0, m->rows * m->cols * sizeof(double));
}

static inline void era_matrix_d_fill(EraMatrix_d *m, double value)
{
    size_t n = m->rows * m->cols;
    for (size_t i = 0; i < n; i++)
        m->data[i] = value;
}

static inline double *era_matrix_d_row(const EraMatrix_d *m, size_t row)
{
    return m->data + row * m->cols;
}

static inline void era_matrix_f_resize(EraMatrix_f *m, size_t rows, size_t cols)
{
    size_t total = rows * cols;
    if (m->rows * m->cols != total) {
        free(m->data);
        m->data = (total > 0) ? (float *)calloc(total, sizeof(float)) : NULL;
    }
    m->rows = rows;
    m->cols = cols;
}

static inline void era_matrix_f_free(EraMatrix_f *m)
{
    free(m->data);
    m->data = NULL;
    m->rows = m->cols = 0;
}

static inline void era_matrix_f_zero(EraMatrix_f *m)
{
    if (m->data && m->rows > 0 && m->cols > 0)
        memset(m->data, 0, m->rows * m->cols * sizeof(float));
}

static inline float *era_matrix_f_row(const EraMatrix_f *m, size_t row)
{
    return m->data + row * m->cols;
}

/* ── Ring buffer ───────────────────────────────────────────────────── *
 * Recovered from FUN_007ce5d0 (init), FUN_007cebe0 (write), FUN_007cf6b0 (read).
 */

static inline void era_ring_init(EraRingBuffer *rb, int num_channels,
                                  int buffer_size, int pre_delay)
{
    /* Free any existing data */
    if (rb->channel_data) {
        for (int ch = 0; ch < rb->num_channels; ch++)
            free(rb->channel_data[ch]);
        free(rb->channel_data);
    }

    rb->num_channels    = num_channels;
    rb->buffer_size     = buffer_size;
    rb->channel_data    = (float **)malloc((size_t)num_channels * sizeof(float *));
    for (int ch = 0; ch < num_channels; ch++)
        rb->channel_data[ch] = (float *)calloc((size_t)buffer_size, sizeof(float));

    rb->write_offset     = 0;
    rb->read_offset      = 0;
    rb->samples_written  = 0;
    rb->samples_available = 0;
}

static inline void era_ring_free(EraRingBuffer *rb)
{
    if (rb->channel_data) {
        for (int ch = 0; ch < rb->num_channels; ch++)
            free(rb->channel_data[ch]);
        free(rb->channel_data);
        rb->channel_data = NULL;
    }
    rb->num_channels = 0;
}

/*
 * Write samples into the ring buffer.
 * Recovered from FUN_007cebe0: handles wrap-around copy.
 */
static inline void era_ring_write(EraRingBuffer *rb, float **src, int num_samples)
{
    if (!src || num_samples <= 0) return;

    int space = rb->buffer_size - rb->write_offset;

    if (space < num_samples) {
        /* Wrap: write first part, then second part from start */
        int first_part  = space;
        int second_part = num_samples - first_part;

        for (int ch = 0; ch < rb->num_channels; ch++) {
            if (rb->channel_data[ch] && src[ch]) {
                memcpy(rb->channel_data[ch] + rb->write_offset,
                       src[ch], (size_t)first_part * sizeof(float));
                memcpy(rb->channel_data[ch],
                       src[ch] + first_part, (size_t)second_part * sizeof(float));
            }
        }
        rb->write_offset = second_part;
    } else {
        for (int ch = 0; ch < rb->num_channels; ch++) {
            if (rb->channel_data[ch] && src[ch]) {
                memcpy(rb->channel_data[ch] + rb->write_offset,
                       src[ch], (size_t)num_samples * sizeof(float));
            }
        }
        rb->write_offset = (rb->write_offset + num_samples) % rb->buffer_size;
    }

    rb->samples_written   += num_samples;
    rb->samples_available += num_samples;
}

/*
 * Read samples from the ring buffer.
 * Recovered from FUN_007cf6b0: handles wrap-around copy.
 */
static inline void era_ring_read(EraRingBuffer *rb, float **dst, int num_samples)
{
    if (!rb->channel_data || num_samples <= 0) return;

    int space = rb->buffer_size - rb->read_offset;

    if (space < num_samples) {
        int first_part  = space;
        int second_part = num_samples - first_part;

        for (int ch = 0; ch < rb->num_channels; ch++) {
            if (rb->channel_data[ch] && dst[ch]) {
                memcpy(dst[ch],
                       rb->channel_data[ch] + rb->read_offset,
                       (size_t)first_part * sizeof(float));
                memcpy(dst[ch] + first_part,
                       rb->channel_data[ch],
                       (size_t)second_part * sizeof(float));
            }
        }
        rb->read_offset = second_part;
    } else {
        for (int ch = 0; ch < rb->num_channels; ch++) {
            if (rb->channel_data[ch] && dst[ch]) {
                memcpy(dst[ch],
                       rb->channel_data[ch] + rb->read_offset,
                       (size_t)num_samples * sizeof(float));
            }
        }
        rb->read_offset = (rb->read_offset + num_samples) % rb->buffer_size;
    }

    rb->samples_available -= num_samples;
    if (rb->samples_available < 0)
        rb->samples_available = 0;
}

/* ── vDSP wrappers (portable fallback for non-Apple platforms) ────── */

static inline void era_vclr(float *dst, int stride, int count)
{
#ifdef __APPLE__
    vDSP_vclr(dst, (vDSP_Stride)stride, (vDSP_Length)count);
#else
    for (int i = 0; i < count; i++)
        dst[i * stride] = 0.0f;
#endif
}

static inline void era_vsmul(const float *src, int src_stride,
                              const float *scalar,
                              float *dst, int dst_stride, int count)
{
#ifdef __APPLE__
    vDSP_vsmul(src, (vDSP_Stride)src_stride, scalar,
               dst, (vDSP_Stride)dst_stride, (vDSP_Length)count);
#else
    float s = *scalar;
    for (int i = 0; i < count; i++)
        dst[i * dst_stride] = src[i * src_stride] * s;
#endif
}

static inline float era_rms_f(const float *data, int count)
{
    if (count <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < count; i++)
        sum += data[i] * data[i];
    return sqrtf(sum / (float)count);
}

#endif /* ERA_UTIL_H */
