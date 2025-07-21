/*
 * Copyright (C) 2025 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d_drm.h"
#include "vivante_gc/vivante_gc.h"

#include <errno.h>
#include <inttypes.h>
#include <libdrm/etnaviv_drmif.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ETNAVIV_DEV_FILENAME "etnaviv"

#define ETNAVIV_CMD_STREAM_SIZE 0x1000

struct etnaviv_context
{
    struct m2d_device* m2d_dev;
    struct etna_device* dev;
    struct etna_gpu* gpu;
    struct etna_pipe* pipe;
    struct etna_cmd_stream* stream;
};

struct etnaviv_buffer
{
    struct gc_buffer base;
    struct etna_bo* bo;
    uint32_t timestamp;
};

static struct etnaviv_context ctx;

static inline struct etnaviv_buffer* to_etnaviv_buffer(const struct m2d_buffer* buf)
{
    return buf ? container_of(buf, struct etnaviv_buffer, base.base) : NULL;
}

int gc_reserve(size_t size, uint32_t** memory)
{
    struct etna_cmd_stream* stream = ctx.stream;

    etna_cmd_stream_reserve(stream, size);
    *memory = &stream->buffer[stream->offset];
    stream->offset += size;
    return 0;
}

int gc_truncate(uint32_t* memory)
{
    struct etna_cmd_stream* stream = ctx.stream;
    uint32_t* begin = stream->buffer;
    uint32_t* end = stream->buffer + stream->size;

    if (memory < begin || memory > end)
    {
        LIBM2D_ERROR("could not truncate the command stream\n");
        return -1;
    }

    stream->offset = memory - begin;
    return 0;
}

size_t gc_get_max_reserved()
{
    struct etna_cmd_stream* stream = ctx.stream;
    uint32_t saved_offset = stream->offset;
    size_t max_reserved;

    stream->offset = 0;
    max_reserved = etna_cmd_stream_avail(stream);
    stream->offset = saved_offset;

    return max_reserved;
}

size_t gc_get_available()
{
    return etna_cmd_stream_avail(ctx.stream);
}

int gc_write_buffer_address(uint32_t* word, const struct m2d_buffer* buf, bool is_write)
{
    struct etna_cmd_stream* stream = ctx.stream;
    struct etnaviv_buffer* priv_buf = to_etnaviv_buffer(buf);
    uint32_t* begin = stream->buffer;
    uint32_t* end = stream->buffer + stream->offset;
    uint32_t saved_offset = stream->offset;

    if (word < begin || word >= end)
    {
        LIBM2D_ERROR("could not write buffer %u address into cmd stream\n", buf->id);
        return -1;
    }

    stream->offset = word - begin;
    etna_cmd_stream_reloc(stream, &(struct etna_reloc){
            .bo = priv_buf->bo,
            .flags = (is_write ? ETNA_RELOC_WRITE : ETNA_RELOC_READ),
            .offset = 0,
        });
    stream->offset = saved_offset;
    return 0;
}

int gc_flush(bool end)
{
    (void)end;

    etna_cmd_stream_flush(ctx.stream);
    return 0;
}

struct m2d_buffer* gc_driver_create(size_t width, size_t height,
                                    enum m2d_pixel_format format, size_t* stride)
{
    struct etnaviv_buffer* priv_buf;
    struct m2d_buffer* buf;

    (void)width;
    (void)format;

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for buffer: %s\n", strerror(errno));
        goto out;
    }
    buf = &priv_buf->base.base;

    *stride = ALIGN(*stride, GC_STRIDE_ALIGNMENT);
    priv_buf->bo = etna_bo_new(ctx.dev, height * *stride,
                               DRM_ETNA_GEM_CACHE_CACHED);
    if (!priv_buf->bo)
    {
        LIBM2D_ERROR("could not create etnaviv buffer object\n");
        goto out_free;
    }

    buf->cpu_addr = etna_bo_map(priv_buf->bo);
    if (!buf->cpu_addr)
    {
        LIBM2D_ERROR("could not map etnaviv buffer object\n");
        goto out_bo_del;
    }

    return buf;

out_bo_del:
    etna_bo_del(priv_buf->bo);

out_free:
    free(priv_buf);

out:
    return NULL;
}

struct m2d_buffer* gc_driver_import(const struct m2d_import_desc* desc)
{
    struct etnaviv_buffer* priv_buf;
    struct m2d_buffer* buf;

    if (!IS_ALIGNED(desc->stride, GC_STRIDE_ALIGNMENT))
    {
        LIBM2D_ERROR("could not import dumb buffer which stride is not %u-byte aligned\n",
                     GC_STRIDE_ALIGNMENT);
        return NULL;
    }

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for imported dumb buffer: %s\n", strerror(errno));
        return NULL;
    }
    buf = &priv_buf->base.base;

    priv_buf->bo = etna_bo_from_dmabuf(ctx.dev, desc->fd);
    if (!priv_buf->bo)
    {
        LIBM2D_ERROR("could not create etnaviv buffer object from a DRM PRIME file descriptor\n");
        free(priv_buf);
        return NULL;
    }

    return buf;
}

void gc_free(struct m2d_buffer* buf)
{
    struct etnaviv_buffer* priv_buf = to_etnaviv_buffer(buf);

    etna_bo_del(priv_buf->bo);
    free(priv_buf);
}

int gc_set_fence(struct m2d_buffer** bufs, size_t count)
{
    struct etna_cmd_stream* stream = ctx.stream;
    uint32_t timestamp = etna_cmd_stream_timestamp(stream);
    size_t i;

    LIBM2D_TRACE("command stream timestamp: %u\n", timestamp);
    for (i = 0; i < count; i++)
        to_etnaviv_buffer(bufs[i])->timestamp = timestamp;

    return 0;
}

int gc_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout)
{
    struct etnaviv_buffer* priv_buf = to_etnaviv_buffer(buf);

    (void)timeout;

    if (etna_bo_cpu_prep(priv_buf->bo,
                         DRM_ETNA_PREP_READ | DRM_ETNA_PREP_WRITE))
    {
        LIBM2D_ERROR("failed to synchronize buffer %u for CPU: %s\n", buf->id, strerror(errno));
        return -1;
    }

    return 0;
}

int gc_sync_for_gpu(struct m2d_buffer* buf)
{
    struct etnaviv_buffer* priv_buf = to_etnaviv_buffer(buf);

    etna_bo_cpu_fini(priv_buf->bo);
    return 0;
}

int gc_wait(const struct m2d_buffer* buf, const struct timespec* timeout)
{
    const struct etnaviv_buffer* priv_buf = to_etnaviv_buffer(buf);
    uint64_t ns = 0;

    if (timeout)
    {
        struct timespec now, delta;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (!m2d_timespec_diff(timeout, &now, &delta))
        {
            LIBM2D_ERROR("failed to wait for buffer %u: timeout in the past\n", buf->id);
            return -1;
        }

        ns = delta.tv_sec * 1000000000 + delta.tv_nsec;
    }

    if (etna_pipe_wait_ns(ctx.pipe, priv_buf->timestamp, ns))
    {
        LIBM2D_ERROR("failed to wait for buffer %u: %s\n", buf->id, strerror(errno));
        return -1;
    }

    LIBM2D_TRACE("wait for command stream timestamp: %u\n", priv_buf->timestamp);

    return 0;
}

int gc_driver_init()
{
    uint64_t model, revision;
    int core = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.m2d_dev = m2d_get_device();
    ctx.m2d_dev->name = ETNAVIV_DEV_FILENAME;

    if (m2d_drm_open(ctx.m2d_dev))
        goto out;

    ctx.dev = etna_device_new(ctx.m2d_dev->fd);
    if (!ctx.dev)
    {
        LIBM2D_ERROR("could not create etnaviv device\n");
        goto out_drm;
    }

    do
    {
        uint64_t feat;

        ctx.gpu = etna_gpu_new(ctx.dev, core);
        if (!ctx.gpu)
        {
            LIBM2D_ERROR("could not find a 2D capable etnaviv gpu\n", core);
            goto out_device;
        }

        if (etna_gpu_get_param(ctx.gpu, ETNA_GPU_FEATURES_0, &feat))
        {
            LIBM2D_ERROR("could not get features for etnaviv gpu (core=%d)\n", core);
            goto out_gpu;
        }

        if ((feat & (1 << 9)) == 0) {
            /* GPU is not 2D capable. */
            etna_gpu_del(ctx.gpu);
            ctx.gpu = NULL;
        }

        core++;
    } while (!ctx.gpu);

    if (etna_gpu_get_param(ctx.gpu, ETNA_GPU_MODEL, &model) ||
        etna_gpu_get_param(ctx.gpu, ETNA_GPU_REVISION, &revision))
    {
        LIBM2D_ERROR("could not get model or revision from etnaviv gpu\n");
        goto out_gpu;
    }
    LIBM2D_INFO("Vivante GC%" PRIx64 " rev %" PRIu64 "\n", model, revision);

    ctx.pipe = etna_pipe_new(ctx.gpu, ETNA_PIPE_2D);
    if (!ctx.pipe)
    {
        LIBM2D_ERROR("could not create etnaviv pipe\n");
        goto out_gpu;
    }

    ctx.stream = etna_cmd_stream_new(ctx.pipe, ETNAVIV_CMD_STREAM_SIZE,
                                     NULL, NULL);
    if (!ctx.stream)
    {
        LIBM2D_ERROR("could not create etnaviv command stream\n");
        goto out_pipe;
    }

    return 0;

//out_stream:
//    etna_cmd_stream_del(ctx.stream);

out_pipe:
    etna_pipe_del(ctx.pipe);

out_gpu:
    etna_gpu_del(ctx.gpu);

out_device:
    etna_device_del(ctx.dev);

out_drm:
    m2d_drm_close(ctx.m2d_dev);

out:
    return -1;
}

void gc_driver_cleanup()
{
    etna_cmd_stream_del(ctx.stream);
    etna_pipe_del(ctx.pipe);
    etna_gpu_del(ctx.gpu);
    etna_device_del(ctx.dev);
    m2d_drm_close(ctx.m2d_dev);
}

#ifdef VIVANTE_GC_DUMP_CMD_BUF
void __real_etna_cmd_stream_flush(struct etna_cmd_stream* stream);
void __wrap_etna_cmd_stream_flush(struct etna_cmd_stream* stream);

void __wrap_etna_cmd_stream_flush(struct etna_cmd_stream* stream)
{
    gc_print_cmd_stream(stream->buffer, stream->offset);
    __real_etna_cmd_stream_flush(stream);
}
#endif
