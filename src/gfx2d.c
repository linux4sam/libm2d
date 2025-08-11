/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d/m2d.h"
#include "m2d_priv.h"
#include "gitversion.h"

#include <drm/microchip_drm.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>

#define GFX2D_TIMEOUT_SECS 1

#define GFX2D_DEV_FILENAME "microchip-gfx2d"

#define GFX2D_DIM_MASK  0x1fffu

struct m2d_buffer
{
    uint32_t id;

    bool imported;
    enum drm_mchp_gfx2d_direction direction;

    /*
     * A virtual address that can be used by the CPU from the userspace,
     * by libcairo for instance, to access the memory behind the GEM DRM
     * object. Likely returned by mmap().
     */
    void* cpu_addr;

    size_t width; /* Width in pixels of the image/texture/frame buffer ... */
    size_t height; /* Height in pixels of the image/texture/frame buffer ... */
    size_t stride; /* Size in bytes between two consecutive pixel rows in the memory area. */
    enum m2d_pixel_format format; /* describe the layout of the pixel components (red, green, blue, alpha) in memory. */

    uint32_t handle;
};

struct m2d_source
{
    struct m2d_buffer* buf;
    dim_t x;
    dim_t y;
    bool enabled;
};

struct gfx2d_state
{
    struct m2d_buffer* target;

    uint32_t source_color;
    struct m2d_source sources[M2D_MAX_SOURCES];

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
    int fd;
    uint32_t next_id;

    struct gfx2d_state state;
};

static struct gfx2d_device dev =
{
    .fd = -1,
};

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

int m2d_init()
{
    LIBM2D_INFO("Version %s\n", M2D_VERSION);
    LIBM2D_INFO("Git Version %s\n", GIT_VERSION);

    memset(&dev, 0, sizeof(dev));

    dev.fd = drmOpenWithType(GFX2D_DEV_FILENAME, NULL, DRM_NODE_RENDER);
    if (dev.fd < 0)
    {
        LIBM2D_ERROR("can't open DRM render node %s: %s\n", GFX2D_DEV_FILENAME, strerror(errno));
        return -1;
    }

    return 0;
}

void m2d_cleanup()
{
    LIBM2D_TRACE("cleaning libm2d up\n");

    if (dev.fd < 0)
    {
        LIBM2D_ERROR("the DRM render node %s is not opened\n", GFX2D_DEV_FILENAME);
        return;
    }

    if (close(dev.fd))
        LIBM2D_ERROR("can't close DRM render node %s: %s\n", GFX2D_DEV_FILENAME, strerror(errno));

    dev.fd = -1;
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

struct m2d_buffer* m2d_alloc(size_t width, size_t height,
                             enum m2d_pixel_format format, size_t stride)
{
    struct drm_mchp_gfx2d_alloc_buffer args;
    struct m2d_buffer* buf;
    size_t size;

    if (dev.fd < 0)
        return NULL;

    if (!gfx2d_surface_is_valid(width, height, format, stride))
        return NULL;

    size = height * stride;

    buf = calloc(1, sizeof(*buf));
    if (!buf)
    {
        LIBM2D_ERROR("could not allocate memory for buffer: %s\n", strerror(errno));
        goto fail;
    }

    buf->id = dev.next_id++;
    buf->direction = DRM_MCHP_GFX2D_DIR_BIDIRECTIONAL;
    buf->width = width;
    buf->height = height;
    buf->format = format;
    buf->stride = stride;
    buf->cpu_addr = MAP_FAILED;

    memset(&args, 0, sizeof(args));
    args.size = size;
    args.width = (uint16_t)width;
    args.height = (uint16_t)height;
    args.stride = (uint16_t)stride;
    args.format = to_gfx2d_format(format);
    args.direction = buf->direction;
    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_ALLOC_BUFFER, &args) < 0)
    {
        LIBM2D_ERROR("could not create buffer: %s\n", strerror(errno));
        goto fail;
    }

    buf->handle = args.handle;
    buf->cpu_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         dev.fd, args.offset);
    if (buf->cpu_addr == MAP_FAILED)
    {
        LIBM2D_ERROR("could not map dumb buffer: %s\n", strerror(errno));
        goto fail;
    }

    LIBM2D_DEBUG("allocated buffer %u (size: [%zux%zu], format: %s)\n",
                 buf->id, width, height, m2d_format_name(format));

    return buf;

fail:
    m2d_free(buf);
    return NULL;
}

struct m2d_buffer* m2d_import(const struct m2d_import_desc* desc)
{
    struct drm_mchp_gfx2d_import_buffer args;
    struct m2d_buffer* buf;

    if (dev.fd < 0)
        return NULL;

    if (!gfx2d_surface_is_valid(desc->width, desc->height, desc->format, desc->stride))
        return NULL;

    buf = calloc(1, sizeof(*buf));
    if (!buf)
    {
        LIBM2D_ERROR("could not allocate memory for imported dumb buffer: %s\n", strerror(errno));
        goto fail;
    }

    buf->id = dev.next_id++;
    buf->imported = true;
    buf->direction = DRM_MCHP_GFX2D_DIR_NONE;
    buf->width = desc->width;
    buf->height = desc->height;
    buf->format = desc->format;
    buf->stride = desc->stride;
    buf->cpu_addr = desc->cpu_addr;

    memset(&args, 0, sizeof(args));
    args.fd = desc->fd;
    args.width = (uint16_t)desc->width;
    args.height = (uint16_t)desc->height;
    args.stride = (uint16_t)desc->stride;
    args.format = to_gfx2d_format(desc->format);
    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_IMPORT_BUFFER, &args) < 0)
    {
        LIBM2D_ERROR("could not get an handle from a DRM PRIME file descriptor: %s\n", strerror(errno));
        goto fail;
    }

    buf->handle = args.handle;

    LIBM2D_DEBUG("imported buffer %u from file descriptor %d (size: [%zux%zu], format: %s)\n",
                 buf->id, desc->fd, desc->width, desc->height, m2d_format_name(desc->format));

    return buf;

fail:
    m2d_free(buf);
    return NULL;
}

void m2d_free(struct m2d_buffer* buf)
{
    if (!buf)
        return;

    if (!buf->imported && buf->cpu_addr != MAP_FAILED)
    {
        size_t size = buf->height * buf->stride;

        munmap(buf->cpu_addr, size);
    }

    if (buf->handle)
    {
        struct drm_mchp_gfx2d_free_buffer args;

        memset(&args, 0, sizeof(args));
        args.handle = buf->handle;
        if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_FREE_BUFFER, &args) < 0)
            LIBM2D_ERROR("could not free buffer: %s\n", strerror(errno));
    }

    LIBM2D_DEBUG("freed buffer %u\n", buf->id);

    free(buf);
}

int m2d_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout)
{
    struct drm_mchp_gfx2d_sync_for_cpu args;

    if (dev.fd <0)
        return -1;

    if (!buf)
        return 0;

    memset(&args, 0, sizeof(args));
    args.handle = buf->handle;
    if (timeout)
    {
        args.timeout.tv_sec = timeout->tv_sec;
        args.timeout.tv_nsec = timeout->tv_nsec;
    }
    else
    {
        args.flags = DRM_MCHP_GFX2D_WAIT_NONBLOCK;
    }

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_SYNC_FOR_CPU, &args) < 0)
    {
        LIBM2D_ERROR("failed to synchronize buffer %u for CPU: %s\n", buf->id, strerror(errno));
        return -1;
    }

    LIBM2D_TRACE("synchronize buffer %u for CPU\n", buf->id);
    return 0;
}

void m2d_sync_for_gpu(struct m2d_buffer* buf)
{
    struct drm_mchp_gfx2d_sync_for_gpu args;

    if (!buf || buf->imported || buf->direction == DRM_MCHP_GFX2D_DIR_NONE)
        return;

    memset(&args, 0, sizeof(args));
    args.handle = buf->handle;

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_SYNC_FOR_GPU, &args) < 0)
        LIBM2D_ERROR("failed to synchronize buffer %u for GPU: %s\n", buf->id, strerror(errno));
    else
        LIBM2D_TRACE("synchronize buffer %u for GPU\n", buf->id);
}

void* m2d_get_data(struct m2d_buffer* buf)
{
    return buf->cpu_addr;
}

int m2d_wait(const struct m2d_buffer* buf, const struct timespec* timeout)
{
    struct drm_mchp_gfx2d_wait args;

    if (dev.fd < 0)
        return -1;

    if (!buf)
        return 0;

    memset(&args, 0, sizeof(args));
    args.handle = buf->handle;
    if (timeout)
    {
        args.timeout.tv_sec = timeout->tv_sec;
        args.timeout.tv_nsec = timeout->tv_nsec;
    }
    else
    {
        args.flags = DRM_MCHP_GFX2D_WAIT_NONBLOCK;
    }

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_WAIT, &args) < 0)
    {
        LIBM2D_ERROR("failed to wait for buffer %u: %s\n", buf->id, strerror(errno));
        return -1;
    }

    LIBM2D_TRACE("wait for buffer %u\n", buf->id);
    return 0;
}

void m2d_set_target(struct m2d_buffer* buf)
{
    dev.state.target = buf;
}

void m2d_set_source(enum m2d_source_id id, struct m2d_buffer* buf, dim_t x, dim_t y)
{
    if (id >= M2D_MAX_SOURCES)
        return;

    struct m2d_source* source = &dev.state.sources[id];
    source->buf = buf;
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
                         enum m2d_blend_function /*alpha_func*/)
{
    dev.state.function = to_gfx2d_blend_function(rgb_func);
}

void m2d_blend_factors(enum m2d_blend_factor src_rgb_factor,
                       enum m2d_blend_factor dst_rgb_factor,
                       enum m2d_blend_factor src_alpha_factor,
                       enum m2d_blend_factor dst_alpha_factor)
{
    dev.state.scfactor = to_gfx2d_blend_factor(src_rgb_factor);
    dev.state.dcfactor = to_gfx2d_blend_factor(dst_rgb_factor);
    dev.state.safactor = to_gfx2d_blend_factor(src_alpha_factor);
    dev.state.dafactor = to_gfx2d_blend_factor(dst_alpha_factor);
}

static void gfx2d_blend(const struct m2d_rectangle* rects, size_t num_rects)
{
    const struct m2d_source* src = &dev.state.sources[M2D_SRC];
    const struct m2d_source* dst = &dev.state.sources[M2D_DST];
    struct drm_mchp_gfx2d_submit args;

    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_SRC), src->buf->id, src->x, src->y);
    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_DST), dst->buf->id, dst->x, dst->y);

    LIBM2D_TRACE("blend function: %s\n", m2d_blend_function_name(dev.state.function));
    LIBM2D_TRACE("blend src color factor: %s\n", m2d_blend_factor_name(dev.state.scfactor));
    LIBM2D_TRACE("blend dst color factor: %s\n", m2d_blend_factor_name(dev.state.dcfactor));
    LIBM2D_TRACE("blend src alpha factor: %s\n", m2d_blend_factor_name(dev.state.safactor));
    LIBM2D_TRACE("blend dst alpha factor: %s\n", m2d_blend_factor_name(dev.state.dafactor));
    LIBM2D_TRACE("blend src color: %08X\n", dev.state.source_color);
    LIBM2D_TRACE("blend dst color: %08X\n", dev.state.blend_color);

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_BLEND;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.blend.function = dev.state.function;
    args.blend.safactor = dev.state.safactor;
    args.blend.dafactor = dev.state.dafactor;
    args.blend.scfactor = dev.state.scfactor;
    args.blend.dcfactor = dev.state.dcfactor;
    args.blend.dst_color = dev.state.blend_color;
    args.blend.src_color = dev.state.source_color;

    args.target_handle = dev.state.target->handle;

    args.sources[0].handle = dst->buf->handle;
    args.sources[0].x = dst->x;
    args.sources[0].y = dst->y;

    args.sources[1].handle = src->buf->handle;
    args.sources[1].x = src->x;
    args.sources[1].y = src->y;

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, &args) < 0)
    {
        LIBM2D_ERROR("can't submit BLEND commands: %s\n", strerror(errno));
    }
    else
    {
        LIBM2D_DEBUG("blending %zu rectangle(s)\n", num_rects);
        m2d_print_rectangles(rects, num_rects);
    }
}

static void gfx2d_copy(const struct m2d_rectangle* rects, size_t num_rects)
{
    const struct m2d_source* src = &dev.state.sources[M2D_SRC];
    struct drm_mchp_gfx2d_submit args;

    LIBM2D_DEBUG("reading %s surface pixels from buffer %u {origin: (%d,%d)}\n",
                 m2d_source_name(M2D_SRC), src->buf->id, src->x, src->y);

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_COPY;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.target_handle = dev.state.target->handle;

    args.sources[0].handle = src->buf->handle;
    args.sources[0].x = src->x;
    args.sources[0].y = src->y;

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, &args) < 0)
    {
        LIBM2D_ERROR("can't submit COPY commands: %s\n", strerror(errno));
    }
    else
    {
        LIBM2D_DEBUG("copying %zu rectangle(s)\n", num_rects);
        m2d_print_rectangles(rects, num_rects);
    }
}

static void gfx2d_fill(const struct m2d_rectangle* rects, size_t num_rects)
{
    struct drm_mchp_gfx2d_submit args;

    memset(&args, 0, sizeof(args));
    args.operation = DRM_MCHP_GFX2D_OP_FILL;

    args.rectangles = (uint64_t)(intptr_t)rects;
    args.num_rectangles = num_rects;

    args.target_handle = dev.state.target->handle;

    args.fill.color = dev.state.source_color;

    if (drmIoctl(dev.fd, DRM_IOCTL_MCHP_GFX2D_SUBMIT, &args) < 0)
    {
        LIBM2D_ERROR("can't submit FILL commands: %s\n", strerror(errno));
    }
    else
    {
        LIBM2D_DEBUG("filling %zu rectangle(s) with ARGB color %08X\n",
                     num_rects, dev.state.source_color);
        m2d_print_rectangles(rects, num_rects);
    }
}

void m2d_draw_rectangles(const struct m2d_rectangle* rects, size_t num_rects)
{
    void (*func)(const struct m2d_rectangle*, size_t);
    uint32_t i;

    if (dev.fd < 0)
    {
        LIBM2D_ERROR("the DRM render node %s is not opened\n", GFX2D_DEV_FILENAME);
        return;
    }

    if (!dev.state.target)
    {
        LIBM2D_ERROR("no target surface\n");
        return;
    }

    if (dev.state.blend_enabled)
    {
        for (i = 0; i < 2; i++)
        {
            const struct m2d_source* source = &dev.state.sources[i];

            if (!source->enabled || !source->buf)
            {
                LIBM2D_ERROR("GFX2D can't blend if %s source is disabled or not set\n",
                             m2d_source_name((enum m2d_source_id)i));
                return;
            }
        }

        func = gfx2d_blend;
    }
    else if (dev.state.sources[0].enabled)
    {
        if (!dev.state.sources[0].buf)
        {
            LIBM2D_ERROR("GFX2D can't copy if %s source is not set\n",
                         m2d_source_name(M2D_SRC));
            return;
        }

        func = gfx2d_copy;
    }
    else
    {
        func = gfx2d_fill;
    }

    LIBM2D_DEBUG("writing target surface pixels into buffer %u\n",
                 dev.state.target->id);
    func(rects, num_rects);
}

void m2d_line_width(dim_t /*width*/)
{
}

void m2d_draw_lines(const struct m2d_line* /*lines*/, size_t /*num_lines*/)
{
}
