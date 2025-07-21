/*
 * Copyright (C) 2025 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef M2D_VIVANTE_GC_H
#define M2D_VIVANTE_GC_H

#include "../m2d_priv.h"

#define GC_STRIDE_ALIGNMENT 16

struct gc_buffer
{
    struct m2d_buffer base;
    uint32_t gpu_format;
};

#ifdef VIVANTE_GC_DUMP_CMD_BUF
void gc_print_cmd_buf(const uint32_t* memory, size_t size);
#else
static inline void gc_print_cmd_buf(const uint32_t* memory, size_t size)
{
    (void)memory;
    (void)size;
}
#endif

/* To be implement by GC drivers. */
int gc_driver_init(void);
void gc_driver_cleanup(void);
struct m2d_buffer* gc_driver_create(size_t width, size_t height,
                             enum m2d_pixel_format format, size_t* size);
struct m2d_buffer* gc_driver_import(const struct m2d_import_desc* desc);

int gc_reserve(size_t size, uint32_t** memory);
int gc_truncate(uint32_t* memory);
size_t gc_get_max_reserved(void);
size_t gc_get_available(void);
int gc_write_buffer_address(uint32_t* word, const struct m2d_buffer* buf, bool is_write);
int gc_flush(bool end);
void gc_free(struct m2d_buffer* buf);
int gc_set_fence(struct m2d_buffer** bufs, size_t count);
int gc_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout);
int gc_sync_for_gpu(struct m2d_buffer* buf);
int gc_wait(const struct m2d_buffer* buf, const struct timespec* timeout);

#endif /* M2D_VIVANTE_GC_H */
