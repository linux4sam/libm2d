/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d/m2d.h"
#include "m2d_priv.h"

#include <drm/microchip_drm.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#define GFX2D_TIMEOUT_SECS 1

#define GFX2D_DEV_FILENAME "microchip-gfx2d"

#define GFX2D_DIM_MASK  0x1fffu

struct gfx2d_buffer
{
    struct m2d_buffer base;
    bool imported;
    enum drm_mchp_gfx2d_direction direction;
    uint32_t handle;
    uint32_t tmp_handle;
};

static inline struct gfx2d_buffer* to_gfx2d_buffer(const struct m2d_buffer* buf)
{
    return buf ? container_of(buf, struct gfx2d_buffer, base) : NULL;
}

struct gfx2d_source
{
    struct gfx2d_buffer* buf;
    dim_t x;
    dim_t y;
    bool enabled;
};

struct gfx2d_state
{
    struct gfx2d_buffer* target;

    uint32_t source_color;
    struct gfx2d_source sources[M2D_MAX_SOURCES];

    bool blend_enabled;
    uint32_t blend_color;
    enum drm_mchp_gfx2d_blend_function function;
    enum drm_mchp_gfx2d_blend_factor safactor;
    enum drm_mchp_gfx2d_blend_factor dafactor;
    enum drm_mchp_gfx2d_blend_factor scfactor;
    enum drm_mchp_gfx2d_blend_factor dcfactor;
};

struct gfx2d_device
{
    struct m2d_device base;
    struct gfx2d_state state;
};

static const struct m2d_capabilities gfx2d_caps =
{
    .stride_alignment = 1,
    .max_sources = 1,
    .dst_is_source = true,
    .draw_lines = false,
    .stretched_blit = false,
};

static int gfx2d_init(void);
static void gfx2d_cleanup(void);
static struct m2d_buffer* gfx2d_create(size_t width, size_t height,
                                       enum m2d_pixel_format format,
                                       size_t* stride);
static struct m2d_buffer* gfx2d_import(const struct m2d_import_desc* desc);
static void gfx2d_free(struct m2d_buffer* buf);
static int gfx2d_sync_for_cpu(struct m2d_buffer* buf,
                              const struct timespec* timeout);
static int gfx2d_sync_for_gpu(struct m2d_buffer* buf);
static int gfx2d_wait(const struct m2d_buffer* buf,
                      const struct timespec* timeout);
static void gfx2d_draw_rectangles(const struct m2d_rectangle* rects,
                                  size_t num_rects);

static const struct m2d_device_funcs gfx2d_device_funcs =
{
    .init = gfx2d_init,
    .cleanup = gfx2d_cleanup,
    .create = gfx2d_create,
    .import = gfx2d_import,
    .free = gfx2d_free,
    .sync_for_cpu = gfx2d_sync_for_cpu,
    .sync_for_gpu = gfx2d_sync_for_gpu,
    .wait = gfx2d_wait,
    .draw_rectangles = gfx2d_draw_rectangles,
};

static struct gfx2d_device dev =
{
    INIT_DEVICE(base, GFX2D_DEV_FILENAME, &gfx2d_caps, &gfx2d_device_funcs),
    .state =
    {
        .source_color = 0xffffffffu,
    },
};

struct m2d_device* m2d_get_device()
{
    return &dev.base;
}

static inline uint32_t gfx2d_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

static enum drm_mchp_gfx2d_blend_function
to_gfx2d_blend_function(enum m2d_blend_function func)
{
#define BFUNC_MAP(name) case M2D_FUNC_##name: return DRM_MCHP_GFX2D_BFUNC_##name

    switch (func)
    {
        BFUNC_MAP(ADD);
        BFUNC_MAP(SUBTRACT);
        BFUNC_MAP(REVERSE);
        BFUNC_MAP(MIN);
        BFUNC_MAP(MAX);
    default:
        LIBM2D_ERROR("invalid blend function\n");
        break;
    }

    return DRM_MCHP_GFX2D_BFUNC_ADD;
}

static enum m2d_blend_function
from_gfx2d_blend_function(enum drm_mchp_gfx2d_blend_function func)
{
#define BFUNC_MAP2(name) case DRM_MCHP_GFX2D_BFUNC_##name: return M2D_FUNC_##name

    switch (func)
    {
        BFUNC_MAP2(ADD);
        BFUNC_MAP2(SUBTRACT);
        BFUNC_MAP2(REVERSE);
        BFUNC_MAP2(MIN);
        BFUNC_MAP2(MAX);
    default:
        LIBM2D_ERROR("invalid blend function\n");
        break;
    }

    return M2D_FUNC_ADD;
}

static inline const char* gfx2d_blend_function_name(enum drm_mchp_gfx2d_blend_function func)
{
    return m2d_blend_function_name(from_gfx2d_blend_function(func));
}

static enum drm_mchp_gfx2d_blend_factor
to_gfx2d_blend_factor(enum m2d_blend_factor factor)
{
#define BFACTOR_MAP(name) case M2D_BLEND_##name: return DRM_MCHP_GFX2D_BFACTOR_##name

    switch (factor)
    {
        BFACTOR_MAP(ZERO);
        BFACTOR_MAP(ONE);
        BFACTOR_MAP(SRC_COLOR);
        BFACTOR_MAP(ONE_MINUS_SRC_COLOR);
        BFACTOR_MAP(DST_COLOR);
        BFACTOR_MAP(ONE_MINUS_DST_COLOR);
        BFACTOR_MAP(SRC_ALPHA);
        BFACTOR_MAP(ONE_MINUS_SRC_ALPHA);
        BFACTOR_MAP(DST_ALPHA);
        BFACTOR_MAP(ONE_MINUS_DST_ALPHA);
        BFACTOR_MAP(CONSTANT_COLOR);
        BFACTOR_MAP(ONE_MINUS_CONSTANT_COLOR);
        BFACTOR_MAP(CONSTANT_ALPHA);
        BFACTOR_MAP(ONE_MINUS_CONSTANT_ALPHA);
        BFACTOR_MAP(SRC_ALPHA_SATURATE);
    default:
        LIBM2D_ERROR("invalid blend factor\n");
        break;
    }

    return DRM_MCHP_GFX2D_BFACTOR_ZERO;
}

static enum m2d_blend_factor
from_gfx2d_blend_factor(enum drm_mchp_gfx2d_blend_factor factor)
{
#define BFACTOR_MAP2(name) case DRM_MCHP_GFX2D_BFACTOR_##name: return M2D_BLEND_##name

    switch (factor)
    {
        BFACTOR_MAP2(ZERO);
        BFACTOR_MAP2(ONE);
        BFACTOR_MAP2(SRC_COLOR);
        BFACTOR_MAP2(ONE_MINUS_SRC_COLOR);
        BFACTOR_MAP2(DST_COLOR);
        BFACTOR_MAP2(ONE_MINUS_DST_COLOR);
        BFACTOR_MAP2(SRC_ALPHA);
        BFACTOR_MAP2(ONE_MINUS_SRC_ALPHA);
        BFACTOR_MAP2(DST_ALPHA);
        BFACTOR_MAP2(ONE_MINUS_DST_ALPHA);
        BFACTOR_MAP2(CONSTANT_COLOR);
        BFACTOR_MAP2(ONE_MINUS_CONSTANT_COLOR);
        BFACTOR_MAP2(CONSTANT_ALPHA);
        BFACTOR_MAP2(ONE_MINUS_CONSTANT_ALPHA);
        BFACTOR_MAP2(SRC_ALPHA_SATURATE);
    default:
        LIBM2D_ERROR("invalid blend factor\n");
        break;
    }

    return M2D_BLEND_ZERO;
}

static inline const char* gfx2d_blend_factor_name(enum drm_mchp_gfx2d_blend_factor factor)
{
    return m2d_blend_factor_name(from_gfx2d_blend_factor(factor));
}

static enum drm_mchp_gfx2d_pixel_format
to_gfx2d_format(enum m2d_pixel_format format)
{
    switch (format)
    {
    case M2D_PF_ARGB8888:
        return DRM_MCHP_GFX2D_PF_ARGB32;

    case M2D_PF_RGB565:
        return DRM_MCHP_GFX2D_PF_RGB16;

    case M2D_PF_A8:
        return DRM_MCHP_GFX2D_PF_A8;
    }

    return DRM_MCHP_GFX2D_PF_ARGB32;
}

static bool gfx2d_surface_is_valid(size_t width, size_t height,
                                   enum m2d_pixel_format format, size_t stride)
{
    if ((width & ~GFX2D_DIM_MASK) || (height & ~GFX2D_DIM_MASK))
    {
        LIBM2D_ERROR("GFX2D doesn't support this size: [%zux%zu]\n", width, height);
        return false;
    }

    if (stride & ~GFX2D_DIM_MASK)
    {
        LIBM2D_ERROR("GFX2D doesn't support this stride: %zu\n", stride);
        return false;
    }

    switch (format)
    {
    case M2D_PF_ARGB8888:
    case M2D_PF_RGB565:
    case M2D_PF_A8:
        break;

    default:
        LIBM2D_ERROR("unsupported pixel format: %s\n", m2d_format_name(format));
        return false;
    }

    return true;
}

static int gfx2d_init()
{
    return 0;
}

static void gfx2d_cleanup()
{
}

static struct m2d_buffer* gfx2d_create(size_t width, size_t height,
                                       enum m2d_pixel_format format,
                                       size_t* stride)
{
    struct drm_mchp_gfx2d_alloc_buffer args;
    struct gfx2d_buffer* priv_buf;
    struct m2d_buffer* buf;
    size_t size;

    if (!gfx2d_surface_is_valid(width, height, format, *stride))
        return NULL;

    size = height * *stride;

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for buffer: %s\n", strerror(errno));
        goto out;
    }
    buf = &priv_buf->base;

    priv_buf->imported = false;
    priv_buf->direction = DRM_MCHP_GFX2D_DIR_BIDIRECTIONAL;

    memset(&args, 0, sizeof(args));
    args.size = size;
    args.width = (uint16_t)width;
    args.height = (uint16_t)height;
    args.stride = (uint16_t)*stride;
    args.format = to_gfx2d_format(format);
    args.direction = priv_buf->direction;
    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_ALLOC_BUFFER, &args) < 0)
    {
        LIBM2D_ERROR("could not create buffer: %s\n", strerror(errno));
        goto out_free;
    }

    priv_buf->handle = args.handle;
    buf->cpu_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         dev.base.fd, args.offset);
    if (buf->cpu_addr == MAP_FAILED)
    {
        LIBM2D_ERROR("could not map dumb buffer: %s\n", strerror(errno));
        goto out_drm_close;
    }

    return buf;

out_drm_close:
    drmCloseBufferHandle(dev.base.fd, priv_buf->handle);

out_free:
    free(priv_buf);

out:
    return NULL;
}

static struct m2d_buffer* gfx2d_import(const struct m2d_import_desc* desc)
{
    struct drm_mchp_gfx2d_import_buffer args;
    struct gfx2d_buffer* priv_buf;
    struct m2d_buffer* buf;

    if (!gfx2d_surface_is_valid(desc->width, desc->height, desc->format, desc->stride))
        return NULL;

    priv_buf = calloc(1, sizeof(*priv_buf));
    if (!priv_buf)
    {
        LIBM2D_ERROR("could not allocate memory for imported dumb buffer: %s\n", strerror(errno));
        return NULL;
    }
    buf = &priv_buf->base;

    priv_buf->imported = true;
    priv_buf->direction = DRM_MCHP_GFX2D_DIR_NONE;

    memset(&args, 0, sizeof(args));
    args.fd = desc->fd;
    args.width = (uint16_t)desc->width;
    args.height = (uint16_t)desc->height;
    args.stride = (uint16_t)desc->stride;
    args.format = to_gfx2d_format(desc->format);
    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_IMPORT_BUFFER, &args) < 0)
    {
        LIBM2D_ERROR("could not get an handle from a DRM PRIME file descriptor: %s\n", strerror(errno));
        free(priv_buf);
        return NULL;
    }

    priv_buf->handle = args.handle;

    return buf;
}

static void gfx2d_free(struct m2d_buffer* buf)
{
    struct gfx2d_buffer* priv_buf = to_gfx2d_buffer(buf);

    if (!priv_buf->imported && buf->cpu_addr != MAP_FAILED)
    {
        size_t size = buf->height * buf->stride;

        munmap(buf->cpu_addr, size);
    }

    if (priv_buf->tmp_handle)
    {
        if (drmCloseBufferHandle(dev.base.fd, priv_buf->tmp_handle))
            LIBM2D_ERROR("could not free tmp buffer: %s\n", strerror(errno));
    }

    if (priv_buf->handle)
    {
        if (drmCloseBufferHandle(dev.base.fd, priv_buf->handle))
            LIBM2D_ERROR("could not free buffer: %s\n", strerror(errno));
    }

    free(priv_buf);
}

static int gfx2d_sync_for_cpu(struct m2d_buffer* buf,
                              const struct timespec* timeout)
{
    struct gfx2d_buffer* priv_buf = to_gfx2d_buffer(buf);
    struct drm_mchp_gfx2d_sync_for_cpu args;

    memset(&args, 0, sizeof(args));
    args.handle = priv_buf->handle;
    if (timeout)
    {
        args.timeout.tv_sec = timeout->tv_sec;
        args.timeout.tv_nsec = timeout->tv_nsec;
    }
    else
    {
        args.flags = DRM_MCHP_GFX2D_WAIT_NONBLOCK;
    }

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_SYNC_FOR_CPU, &args) < 0)
    {
        LIBM2D_ERROR("failed to synchronize buffer %u for CPU: %s\n", buf->id, strerror(errno));
        return -1;
    }

    return 0;
}

static int gfx2d_sync_for_gpu(struct m2d_buffer* buf)
{
    struct gfx2d_buffer* priv_buf = to_gfx2d_buffer(buf);
    struct drm_mchp_gfx2d_sync_for_gpu args;

    if (priv_buf->imported || priv_buf->direction == DRM_MCHP_GFX2D_DIR_NONE)
        return 0;

    memset(&args, 0, sizeof(args));
    args.handle = priv_buf->handle;

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_SYNC_FOR_GPU, &args) < 0)
    {
        LIBM2D_ERROR("failed to synchronize buffer %u for GPU: %s\n", buf->id, strerror(errno));
        return -1;
    }

    return 0;
}

static int gfx2d_wait(const struct m2d_buffer* buf, const struct timespec* timeout)
{
    const struct gfx2d_buffer* priv_buf = to_gfx2d_buffer(buf);
    struct drm_mchp_gfx2d_wait args;

    memset(&args, 0, sizeof(args));
    args.handle = priv_buf->handle;
    if (timeout)
    {
        args.timeout.tv_sec = timeout->tv_sec;
        args.timeout.tv_nsec = timeout->tv_nsec;
    }
    else
    {
        args.flags = DRM_MCHP_GFX2D_WAIT_NONBLOCK;
    }

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_WAIT, &args) < 0)
    {
        LIBM2D_ERROR("failed to wait for buffer %u: %s\n", buf->id, strerror(errno));
        return -1;
    }

    return 0;
}

void m2d_set_target(struct m2d_buffer* buf)
{
    dev.state.target = to_gfx2d_buffer(buf);
}

void m2d_set_source(enum m2d_source_id id, struct m2d_buffer* buf, dim_t x, dim_t y)
{
    if (id >= M2D_MAX_SOURCES)
        return;

    struct gfx2d_source* source = &dev.state.sources[id];
    source->buf = to_gfx2d_buffer(buf);
    source->x = x;
    source->y = y;
}

void m2d_source_enable(enum m2d_source_id id, bool enabled)
{
    if (id >= M2D_MAX_SOURCES)
        return;

    dev.state.sources[id].enabled = enabled;
}

void m2d_source_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    dev.state.source_color = gfx2d_color(red, green, blue, alpha);
}

void m2d_blend_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    dev.state.blend_color = gfx2d_color(red, green, blue, alpha);
}

void m2d_blend_enable(bool enabled)
{
    dev.state.blend_enabled = enabled;
}

void m2d_blend_functions(enum m2d_blend_function rgb_func,
                         enum m2d_blend_function alpha_func)
{
    (void)alpha_func;

    dev.state.function = to_gfx2d_blend_function(rgb_func);
}

static enum drm_mchp_gfx2d_blend_factor gfx2d_fix_afactor(enum drm_mchp_gfx2d_blend_factor afactor)
{
    switch (afactor)
    {
    case DRM_MCHP_GFX2D_BFACTOR_CONSTANT_COLOR:
        return DRM_MCHP_GFX2D_BFACTOR_CONSTANT_ALPHA;
    case DRM_MCHP_GFX2D_BFACTOR_ONE_MINUS_CONSTANT_COLOR:
        return DRM_MCHP_GFX2D_BFACTOR_ONE_MINUS_CONSTANT_ALPHA;
    default:
        break;
    }

    return afactor;
}

void m2d_blend_factors(enum m2d_blend_factor src_rgb_factor,
                       enum m2d_blend_factor dst_rgb_factor,
                       enum m2d_blend_factor src_alpha_factor,
                       enum m2d_blend_factor dst_alpha_factor)
{
    dev.state.scfactor = to_gfx2d_blend_factor(src_rgb_factor);
    dev.state.dcfactor = to_gfx2d_blend_factor(dst_rgb_factor);
    dev.state.safactor = gfx2d_fix_afactor(to_gfx2d_blend_factor(src_alpha_factor));
    dev.state.dafactor = gfx2d_fix_afactor(to_gfx2d_blend_factor(dst_alpha_factor));
}

static int gfx2d_get_tmp_handle(struct gfx2d_buffer* priv_buf)
{
    if (unlikely(!priv_buf->tmp_handle))
    {
        size_t stride = priv_buf->base.width * sizeof(uint32_t);
        size_t size = priv_buf->base.height * stride;
        struct drm_mchp_gfx2d_alloc_buffer args;

        memset(&args, 0, sizeof(args));
        args.size = size;
        args.width = (uint16_t)priv_buf->base.width;
        args.height = (uint16_t)priv_buf->base.height;
        args.stride = (uint16_t)stride;
        args.format = DRM_MCHP_GFX2D_PF_ARGB32;
        args.direction = DRM_MCHP_GFX2D_DIR_BIDIRECTIONAL;
        if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_ALLOC_BUFFER, &args) < 0)
        {
            LIBM2D_ERROR("could not create tmp bo for buffer %u: %s\n",
                         priv_buf->base.id, strerror(errno));
        }
        else
        {
            priv_buf->tmp_handle = args.handle;
        }
    }

    return priv_buf->tmp_handle;
}

static int gfx2d_submit_blend(struct drm_mchp_gfx2d_submit* args)
{
    LIBM2D_TRACE("blend src color: %08X\n", args->blend.src_color);
    LIBM2D_TRACE("blend dst color: %08X\n", args->blend.dst_color);
    LIBM2D_TRACE("blend function: %s\n", gfx2d_blend_function_name(args->blend.function));
    LIBM2D_TRACE("blend src color factor: %s\n", gfx2d_blend_factor_name(args->blend.scfactor));
    LIBM2D_TRACE("blend dst color factor: %s\n", gfx2d_blend_factor_name(args->blend.dcfactor));
    LIBM2D_TRACE("blend src alpha factor: %s\n", gfx2d_blend_factor_name(args->blend.safactor));
    LIBM2D_TRACE("blend dst alpha factor: %s\n", gfx2d_blend_factor_name(args->blend.dafactor));

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, args) < 0)
    {
        LIBM2D_ERROR("can't submit BLEND commands: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static const struct gfx2d_source* gfx2d_get_dst_or_target(struct gfx2d_source* tmp)
{
    const struct gfx2d_source* dst = &dev.state.sources[M2D_DST];

    if (dst->enabled && dst->buf)
        return dst;

    tmp->buf = dev.state.target;
    tmp->x = 0;
    tmp->y = 0;
    tmp->enabled = true;
    return tmp;
}

static void gfx2d_blend(const struct m2d_rectangle* rects, size_t num_rects)
{
    struct gfx2d_buffer* target = dev.state.target;
    const struct gfx2d_source* src = &dev.state.sources[M2D_SRC];
    struct gfx2d_source tmp;
    const struct gfx2d_source* dst = gfx2d_get_dst_or_target(&tmp);
    struct drm_mchp_gfx2d_submit args;

    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_SRC), src->buf->base.id, src->x, src->y);
    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_DST), dst->buf->base.id, dst->x, dst->y);

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_BLEND;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.sources[1].handle = src->buf->handle;
    args.sources[1].x = src->x;
    args.sources[1].y = src->y;

    if (unlikely(dev.state.source_color != 0xffffffffu))
    {
        uint32_t handle = gfx2d_get_tmp_handle(target);

        if (!handle)
            return;

        LIBM2D_TRACE("source color: %08X\n", dev.state.source_color);

        /* Don't care about the DST (source 0) surface here. */
        args.target_handle = handle;
        args.sources[0].handle = src->buf->handle;
        args.sources[0].x = src->x;
        args.sources[0].y = src->y;
        args.blend.src_color = dev.state.source_color;
        args.blend.function = DRM_MCHP_GFX2D_BFUNC_ADD;
        args.blend.safactor = DRM_MCHP_GFX2D_BFACTOR_CONSTANT_ALPHA;
        args.blend.dafactor = DRM_MCHP_GFX2D_BFACTOR_ZERO;
        args.blend.scfactor = DRM_MCHP_GFX2D_BFACTOR_CONSTANT_COLOR;
        args.blend.dcfactor = DRM_MCHP_GFX2D_BFACTOR_ZERO;
        if (gfx2d_submit_blend(&args))
            return;

        args.sources[1].handle = handle;
        args.sources[1].x = 0;
        args.sources[1].y = 0;
    }

    args.target_handle = target->handle;
    args.sources[0].handle = dst->buf->handle;
    args.sources[0].x = dst->x;
    args.sources[0].y = dst->y;
    args.blend.src_color = dev.state.blend_color;
    args.blend.dst_color = dev.state.blend_color;
    args.blend.function = dev.state.function;
    args.blend.safactor = dev.state.safactor;
    args.blend.dafactor = dev.state.dafactor;
    args.blend.scfactor = dev.state.scfactor;
    args.blend.dcfactor = dev.state.dcfactor;
    if (!gfx2d_submit_blend(&args))
    {
        LIBM2D_DEBUG("blending %zu rectangle(s)\n", num_rects);
        m2d_print_rectangles(rects, num_rects);
    }
}

static void gfx2d_copy(const struct m2d_rectangle* rects, size_t num_rects)
{
    const struct gfx2d_source* src = &dev.state.sources[M2D_SRC];
    struct drm_mchp_gfx2d_submit args;

    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_SRC), src->buf->base.id, src->x, src->y);

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_COPY;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.target_handle = dev.state.target->handle;

    args.sources[0].handle = src->buf->handle;
    args.sources[0].x = src->x;
    args.sources[0].y = src->y;

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, &args) < 0)
    {
        LIBM2D_ERROR("can't submit COPY commands: %s\n", strerror(errno));
    }
    else
    {
        LIBM2D_DEBUG("copying %zu rectangle(s)\n", num_rects);
        m2d_print_rectangles(rects, num_rects);
    }
}

static int gfx2d_fill_target(const struct m2d_rectangle* rects, size_t num_rects,
                             uint32_t target_handle)
{
    struct drm_mchp_gfx2d_submit args;

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_FILL;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.target_handle = target_handle;

    args.fill.color = dev.state.source_color;

    if (drmIoctl(dev.base.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, &args) < 0)
    {
        LIBM2D_ERROR("can't submit FILL commands: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static void gfx2d_fill(const struct m2d_rectangle* rects, size_t num_rects)
{
    if (!gfx2d_fill_target(rects, num_rects, dev.state.target->handle))
    {
        LIBM2D_DEBUG("filling %zu rectangle(s) with ARGB color %08X\n",
                     num_rects, dev.state.source_color);
        m2d_print_rectangles(rects, num_rects);
    }
}

static void gfx2d_blend_with_source_color(const struct m2d_rectangle* rects, size_t num_rects)
{
    struct gfx2d_buffer* target = dev.state.target;
    struct gfx2d_source tmp;
    const struct gfx2d_source* dst = gfx2d_get_dst_or_target(&tmp);
    struct drm_mchp_gfx2d_submit args;
    uint32_t handle;

    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_DST), dst->buf->base.id, dst->x, dst->y);

    LIBM2D_TRACE("source color: %08X\n", dev.state.source_color);

    handle = gfx2d_get_tmp_handle(target);
    if (!handle || gfx2d_fill_target(rects, num_rects, handle))
        return;

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_BLEND;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.target_handle = target->handle;
    args.sources[0].handle = dst->buf->handle;
    args.sources[0].x = dst->x;
    args.sources[0].y = dst->y;
    args.sources[1].handle = handle;
    args.sources[1].x = 0;
    args.sources[1].y = 0;
    args.blend.src_color = dev.state.blend_color;
    args.blend.dst_color = dev.state.blend_color;
    args.blend.function = dev.state.function;
    args.blend.safactor = dev.state.safactor;
    args.blend.dafactor = dev.state.dafactor;
    args.blend.scfactor = dev.state.scfactor;
    args.blend.dcfactor = dev.state.dcfactor;
    if (!gfx2d_submit_blend(&args))
    {
        LIBM2D_DEBUG("blending %zu rectangle(s)\n", num_rects);
        m2d_print_rectangles(rects, num_rects);
    }
}

static void gfx2d_draw_rectangles(const struct m2d_rectangle* rects,
                                  size_t num_rects)
{
    const struct gfx2d_source* src = &dev.state.sources[M2D_SRC];
    void (*func)(const struct m2d_rectangle*, size_t);
    bool src_enabled = src->enabled && src->buf;

    if (!dev.state.target)
    {
        LIBM2D_ERROR("no target surface\n");
        return;
    }

    if (dev.state.blend_enabled)
        func = src_enabled ? gfx2d_blend : gfx2d_blend_with_source_color;
    else if (src_enabled)
        func = gfx2d_copy;
    else
        func = gfx2d_fill;

    LIBM2D_DEBUG("writing target surface pixels into buffer %u\n",
                 dev.state.target->base.id);
    func(rects, num_rects);
}

void m2d_line_width(dim_t width)
{
    (void)width;
}
