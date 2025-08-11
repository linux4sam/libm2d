/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d/version.h"
#include "m2d_priv.h"
#include "gitversion.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>

static struct m2d_device* dev;

int m2d_init()
{
    drmVersionPtr version;

    LIBM2D_INFO("Version %s\n", M2D_VERSION);
    LIBM2D_INFO("Git Version %s\n", GIT_VERSION);

    dev = m2d_get_device();

    dev->next_id = 0;
    dev->fd = drmOpenWithType(dev->name, NULL, DRM_NODE_RENDER);
    if (dev->fd < 0)
    {
        LIBM2D_ERROR("can't open DRM render node %s: %s\n", dev->name, strerror(errno));
        goto error;
    }

    (void)version;
#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_DEBUG
    version = drmGetVersion(dev->fd);
    if (version)
    {
        LIBM2D_DEBUG("DRM Version %d.%d.%d\n",
                     version->version_major,
                     version->version_minor,
                     version->version_patchlevel);
        LIBM2D_DEBUG("  Name: %s\n", version->name);
        LIBM2D_DEBUG("  Date: %s\n", version->date);
        LIBM2D_DEBUG("  Description: %s\n", version->desc);
        drmFreeVersion(version);
    }
#endif

    if (dev->funcs->init())
        goto drm_close;

    return 0;

drm_close:
    drmClose(dev->fd);
    dev->fd = -1;

error:
    dev = NULL;
    return -1;
}

void m2d_cleanup()
{
    LIBM2D_TRACE("cleaning libm2d up\n");

    if (!dev || dev->fd < 0)
    {
        LIBM2D_ERROR("the DRM render node %s is not opened\n", dev->name);
        return;
    }

    dev->funcs->cleanup();

    if (drmClose(dev->fd))
        LIBM2D_ERROR("can't close DRM render node %s: %s\n", dev->name, strerror(errno));

    dev->fd = -1;
    dev = NULL;
}

const struct m2d_capabilities* m2d_get_capabilities()
{
    return dev ? dev->caps : NULL;
}

struct m2d_buffer* m2d_alloc(size_t width, size_t height,
                             enum m2d_pixel_format format, size_t stride)
{
    struct m2d_buffer* buf;

    if (dev->fd < 0)
        return NULL;

    buf = dev->funcs->create(width, height, format, &stride);
    if (!buf)
    {
        LIBM2D_ERROR("failed to create new buffer\n");
        return NULL;
    }

    buf->id = dev->next_id++;
    buf->width = width;
    buf->height = height;
    buf->format = format;
    buf->stride = stride;

    LIBM2D_DEBUG("allocated buffer %u (size: [%zux%zu], format: %s)\n",
                 buf->id, width, height, m2d_format_name(format));

    return buf;
}

struct m2d_buffer* m2d_import(const struct m2d_import_desc* desc)
{
    struct m2d_buffer* buf;

    if (dev->fd < 0)
        return NULL;

    buf = dev->funcs->import(desc);
    if (!buf)
    {
        LIBM2D_ERROR("failed to import buffer\n");
        return NULL;
    }

    buf->id = dev->next_id++;
    buf->width = desc->width;
    buf->height = desc->height;
    buf->format = desc->format;
    buf->stride = desc->stride;
    buf->cpu_addr = desc->cpu_addr;

    LIBM2D_DEBUG("imported buffer %u from file descriptor %d (size: [%zux%zu], format: %s)\n",
                 buf->id, desc->fd, desc->width, desc->height, m2d_format_name(desc->format));

    return buf;
}

void m2d_free(struct m2d_buffer* buf)
{
    uint32_t id;

    if (!buf)
        return;

    id = buf->id;
    dev->funcs->free(buf);

    (void)id;
    LIBM2D_DEBUG("freed buffer %u\n", id);
}

int m2d_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout)
{
    if (dev->fd < 0)
        return -1;

    if (!buf)
        return 0;

    if (dev->funcs->sync_for_cpu(buf, timeout))
        return -1;

    LIBM2D_TRACE("synchronize buffer %u for CPU\n", buf->id);

    return 0;
}

void m2d_sync_for_gpu(struct m2d_buffer* buf)
{
    if (dev->fd < 0)
        return;

    if (!buf)
        return;

    if (dev->funcs->sync_for_gpu(buf))
        return;

    LIBM2D_TRACE("synchronize buffer %u for GPU\n", buf->id);
}

int m2d_wait(const struct m2d_buffer* buf, const struct timespec* timeout)
{
    if (dev->fd < 0)
        return -1;

    if (!buf)
        return 0;

    if (dev->funcs->wait(buf, timeout))
        return -1;

    LIBM2D_TRACE("wait for buffer %u\n", buf->id);

    return 0;
}

void* m2d_get_data(struct m2d_buffer* buf)
{
    return buf->cpu_addr;
}

size_t m2d_get_stride(const struct m2d_buffer* buf)
{
    return buf->stride;
}

void m2d_draw_rectangles(const struct m2d_rectangle* rects, size_t num_rects)
{
    if (dev->fd < 0)
    {
        LIBM2D_ERROR("the DRM render node %s is not opened\n", dev->name);
        return;
    }

    dev->funcs->draw_rectangles(rects, num_rects);
}

const char* m2d_format_name(enum m2d_pixel_format format)
{
#define FORMAT_TO_STR(name) case M2D_PF_##name: return #name

    switch (format)
    {
        FORMAT_TO_STR(ARGB8888);
        FORMAT_TO_STR(RGB565);
        FORMAT_TO_STR(A8);
    default:
        break;
    }

    return "unknown";
}

const char* m2d_blend_function_name(enum m2d_blend_function function)
{
#define BFUNC_TO_STR(name) case M2D_FUNC_##name: return #name

    switch (function)
    {
        BFUNC_TO_STR(ADD);
        BFUNC_TO_STR(SUBTRACT);
        BFUNC_TO_STR(REVERSE);
        BFUNC_TO_STR(MIN);
        BFUNC_TO_STR(MAX);
    default:
        break;
    }

    return "unknown";
}

const char* m2d_blend_factor_name(enum m2d_blend_factor factor)
{
#define BFACT_TO_STR(name) case M2D_BLEND_##name: return #name

    switch (factor)
    {
        BFACT_TO_STR(ZERO);
        BFACT_TO_STR(ONE);
        BFACT_TO_STR(SRC_COLOR);
        BFACT_TO_STR(ONE_MINUS_SRC_COLOR);
        BFACT_TO_STR(DST_COLOR);
        BFACT_TO_STR(ONE_MINUS_DST_COLOR);
        BFACT_TO_STR(SRC_ALPHA);
        BFACT_TO_STR(ONE_MINUS_SRC_ALPHA);
        BFACT_TO_STR(DST_ALPHA);
        BFACT_TO_STR(ONE_MINUS_DST_ALPHA);
        BFACT_TO_STR(CONSTANT_COLOR);
        BFACT_TO_STR(ONE_MINUS_CONSTANT_COLOR);
        BFACT_TO_STR(CONSTANT_ALPHA);
        BFACT_TO_STR(ONE_MINUS_CONSTANT_ALPHA);
        BFACT_TO_STR(SRC_ALPHA_SATURATE);
    default:
        break;
    }

    return "unknown";
}

const char* m2d_source_name(enum m2d_source_id id)
{
#define SOURCE_TO_STR(name) case M2D_##name: return #name

    switch (id)
    {
        SOURCE_TO_STR(SRC);
        SOURCE_TO_STR(DST);
        SOURCE_TO_STR(MSK);
    default:
        break;
    }

    return "unknown";
}

bool m2d_intersect(const struct m2d_rectangle* a,
		   const struct m2d_rectangle* b,
		   struct m2d_rectangle* result)
{
    dim_t min_x = (dim_t)max_int(a->x, b->x);
    dim_t max_x = (dim_t)min_int((int)a->x + a->w, (int)b->x + b->w);
    dim_t min_y = (dim_t)max_int(a->y, b->y);
    dim_t max_y = (dim_t)min_int((int)a->y + a->h, (int)b->y + b->h);

    if (min_x >= max_x)
        return false;

    if (min_y >= max_y)
        return false;

    result->x = min_x;
    result->y = min_y;
    result->w = max_x - min_x;
    result->h = max_y - min_y;
    return true;
}

static int m2d_active_log_level()
{
    static int level = -1;

    if (unlikely(level < 0))
    {
        const char *env = getenv("LIBM2D_DEBUG");

        if (!env || env[0] == '\0')
        {
            level = LIBM2D_LEVEL_OFF;
        }
        else
        {
            char *endptr = NULL;

            level = strtol(env, &endptr, 0);
            if (level < 0 || level > LIBM2D_LEVEL_OFF ||
                !endptr || *endptr != '\0')
                level = LIBM2D_LEVEL_OFF;
        }
    }

    return level;
}

void m2d_log_v(int level, const char* format, va_list ap)
{
    char prefix;

    if (level < m2d_active_log_level())
        return;

    switch (level)
    {
    case LIBM2D_LEVEL_TRACE:
        prefix = 'T';
        break;

    case LIBM2D_LEVEL_DEBUG:
        prefix = 'D';
        break;

    case LIBM2D_LEVEL_INFO:
        prefix = 'I';
        break;

    case LIBM2D_LEVEL_WARN:
        prefix = 'W';
        break;

    case LIBM2D_LEVEL_ERROR:
        prefix = 'E';
        break;

    default:
        prefix = 'U';
        break;
    }

    fprintf(stderr, "libm2d (%c%c) : ", prefix, prefix);
    vfprintf(stderr, format, ap);
}

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_TRACE
void m2d_print_rectangles(const struct m2d_rectangle* rects, size_t num_rects)
{
    size_t i;

    for (i = 0; i < num_rects; i++)
    {
        const struct m2d_rectangle* r = &rects[i];

        trace_msg("rectangle %zu {origin: (%d,%d), size: [%dx%d]}\n",
                  i, r->x, r->y, r->w, r->h);
    }
}
#endif

size_t m2d_byte_per_pixel(enum m2d_pixel_format format)
{
    switch (format)
    {
    case M2D_PF_ARGB8888:
        return 4;

    case M2D_PF_RGB565:
        return 2;

    case M2D_PF_A8:
        return 1;

    default:
        break;
    }

    return 0;
}
