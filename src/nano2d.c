/*
 * Copyright (C) 2025 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "vivante_gc/vivante_gc.h"

#include <nano2D.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define NANO2D_DEV_FILENAME "nano2d"

struct nano2d_buffer
{
    struct gc_buffer base;
    n2d_buffer_t buffer;
};

static inline struct nano2d_buffer* to_nano2d_buffer(const struct m2d_buffer* buf)
{
    return buf ? container_of(buf, struct nano2d_buffer, base.base) : NULL;
}

#ifdef VIVANTE_GC_DUMP_CMD_BUF
static uint32_t* latest_memory;
static size_t latest_size;

#define nano2d_print_latest() do { if (latest_memory && latest_size) gc_print_cmd_buf(latest_memory, latest_size); } while (0)
#define nano2d_set_buffer(_memory, _size) do { latest_memory = (_memory); latest_size = (_size); } while (0)
#define nano2d_set_end(_memory) do { if (latest_memory) latest_size = (_memory) - latest_memory; } while (0)
#else
#define nano2d_print_latest()
#define nano2d_set_buffer(_memory, _size)
#define nano2d_set_end(_memory)
#endif

int gc_reserve(size_t size, uint32_t** memory)
{
    nano2d_print_latest();

    if (n2d_reserve(size, memory) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not reserve %zu words in the command buffer\n", size);
        nano2d_set_buffer(NULL, 0);
        return -1;
    }

    nano2d_set_buffer(*memory, size);

    return 0;
}

int gc_truncate(uint32_t* memory)
{
    if (n2d_truncate(memory) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not truncate the command buffer\n");
        return -1;
    }

    nano2d_set_end(memory);

    return 0;
}

size_t gc_get_max_reserved()
{
    size_t size = 0;

    if (n2d_get_max_reserved(&size) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not get maximum number of words to be reserved in the command buffer\n");
        return 0;
    }

    return size;
}

size_t gc_get_available()
{
    size_t size = 0;

    if (n2d_get_available(&size) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not get the number of words left in the command buffer\n");
        return 0;
    }

    return size;
}

int gc_write_buffer_address(uint32_t* word, const struct m2d_buffer* buf, bool is_write)
{
    struct nano2d_buffer* priv_buf = to_nano2d_buffer(buf);

    (void)is_write;

    *word = (uint32_t)priv_buf->buffer.gpu;
    return 0;
}

int gc_flush(bool end)
{
    nano2d_print_latest();
    nano2d_set_buffer(NULL, 0);

    if (n2d_flush(end) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not flush the command buffer\n");
        return -1;
    }

    return 0;
}

static n2d_buffer_format_t to_n2d_buffer_format(enum m2d_pixel_format format)
{
    switch (format)
    {
    case M2D_PF_ARGB8888:
        return N2D_ARGB8888;

    case M2D_PF_RGB565:
        return N2D_RGB565;

    case M2D_PF_A8:
        return N2D_A8;
    }

    return N2D_ARGB8888;
}

struct m2d_buffer* gc_driver_create(size_t width, size_t height,
                                    enum m2d_pixel_format format, size_t* stride)
{
    struct nano2d_buffer* priv_buf;
    struct m2d_buffer* buf;
    n2d_buffer_t* buffer;

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for buffer: %s\n", strerror(errno));
        goto out;
    }
    buf = &priv_buf->base.base;
    buffer = &priv_buf->buffer;

    buffer->width = width;
    buffer->height = height;
    buffer->format = to_n2d_buffer_format(format);
    buffer->orientation = N2D_0;
    buffer->tiling = N2D_LINEAR;
    buffer->tile_status_config = N2D_TSC_DISABLE;
    buffer->cacheMode = N2D_CACHE_128;
    if (n2d_allocate(buffer) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not allocate nano2d buffer\n");
        goto out_free;
    }

    *stride = buffer->stride;
    buf->cpu_addr = buffer->memory;

    return buf;

out_free:
    free(priv_buf);

out:
    return NULL;
}

struct m2d_buffer* gc_driver_import(const struct m2d_import_desc* desc)
{
    n2d_user_memory_desc_t mem_desc;
    struct nano2d_buffer* priv_buf;
    struct m2d_buffer* buf;
    n2d_buffer_t* buffer;
    n2d_uintptr_t handle;

    if (!IS_ALIGNED(desc->stride, GC_STRIDE_ALIGNMENT))
    {
        LIBM2D_ERROR("could not import dmabuf which stride is not %u-byte aligned\n",
                     GC_STRIDE_ALIGNMENT);
        goto out;
    }

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for imported dmabuf: %s\n", strerror(errno));
        goto out;
    }
    buf = &priv_buf->base.base;
    buffer = &priv_buf->buffer;

    memset(&mem_desc, 0, sizeof(mem_desc));
    mem_desc.flag = N2D_WRAP_FROM_DMABUF;
    mem_desc.handle = (n2d_int32_t)desc->fd;
    mem_desc.logical = (n2d_uintptr_t)desc->cpu_addr;
    if (n2d_wrap(&mem_desc, &handle) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("failed to wrap imported dmabuf\n");
        goto out_free;
    }

    buffer->width = desc->width;
    buffer->height = desc->height;
    buffer->stride = desc->stride;
    buffer->format = to_n2d_buffer_format(desc->format);
    buffer->orientation = N2D_0;
    buffer->tiling = N2D_LINEAR;
    buffer->handle = handle;
    if (n2d_map(buffer) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("failed to map imported dmabuf into GPU\n");
        goto out_free_n2d_buffer;
    }

    return buf;

out_free_n2d_buffer:
    n2d_free(buffer);

out_free:
    free(priv_buf);

out:
    return NULL;
}

void gc_free(struct m2d_buffer* buf)
{
    struct nano2d_buffer* priv_buf = to_nano2d_buffer(buf);

    n2d_free(&priv_buf->buffer);
    free(priv_buf);
}

int gc_set_fence(struct m2d_buffer** bufs, size_t count)
{
    (void)bufs;
    (void)count;
    return 0;
}

int gc_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout)
{
    return gc_wait(buf, timeout);
}

int gc_sync_for_gpu(struct m2d_buffer* buf)
{
    (void)buf;
    return 0;
}

int gc_wait(const struct m2d_buffer* buf, const struct timespec* timeout)
{
    n2d_uint32_t wait = N2D_INFINITE;

    if (timeout)
    {
        struct timespec now, delta;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (!m2d_timespec_diff(timeout, &now, &delta))
        {
            LIBM2D_ERROR("failed to wait for buffer %u: timeout in the past\n", buf->id);
            return -1;
        }

        wait = delta.tv_sec * 1000 + delta.tv_nsec / 1000000;
    }

    return (n2d_wait(wait) != N2D_SUCCESS) ? -1 : 0;
}

int gc_driver_init()
{
    struct m2d_device* dev = m2d_get_device();

    /* Open the context. */
    if (n2d_open() != N2D_SUCCESS)
    {
        LIBM2D_ERROR("could not open nano2d device\n");
        goto out;
    }

    /* Switch to default device and core. */
    if (n2d_switch_device(N2D_DEVICE_0) != N2D_SUCCESS ||
        n2d_switch_core(N2D_CORE_0) != N2D_SUCCESS)
    {
        LIBM2D_ERROR("failed to switch to nano2d device 0, core 0\n");
        goto out_close;
    }

    dev->name = NANO2D_DEV_FILENAME;
    dev->fd = 0x7fffffff;

    return 0;

out_close:
    (void)n2d_close();

out:
    return -1;
}

void gc_driver_cleanup()
{
    (void)n2d_close();
}
