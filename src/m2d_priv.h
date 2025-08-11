/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef M2D_PRIV_H
#define M2D_PRIV_H

#include "m2d/m2d.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>

#define unlikely(expr) (__builtin_expect (!!(expr), 0))

#ifndef FIELD_PREP
#define FIELD_PREP(_mask, _val) (((_val) << (ffsll(_mask) - 1)) & (_mask))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define DEFINE_MIN_MAX(type)                     \
static inline type min_##type(type a, type b)    \
{                                                \
    return a < b ? a : b;                        \
}                                                \
                                                 \
static inline type max_##type(type a, type b)    \
{                                                \
    return a > b ? a : b;                        \
}

DEFINE_MIN_MAX(int)

#ifndef container_of
#define container_of(ptr, type, member) ((type *)((unsigned char *)(ptr) - offsetof(type, member)))
#endif

struct m2d_buffer
{
    uint32_t id;

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
};

struct m2d_device_funcs
{
    int (*init)();
    void (*cleanup)();

    struct m2d_buffer* (*create)(size_t width, size_t height,
                                 enum m2d_pixel_format format, size_t* stride);
    struct m2d_buffer* (*import)(const struct m2d_import_desc* desc);
    void (*free)(struct m2d_buffer* buf);
    int (*sync_for_cpu)(struct m2d_buffer* buf, const struct timespec* timeout);
    int (*sync_for_gpu)(struct m2d_buffer* buf);
    int (*wait)(const struct m2d_buffer* buf, const struct timespec* timeout);
    void (*draw_rectangles)(const struct m2d_rectangle* rects, size_t num_rects);
    void (*draw_lines)(const struct m2d_line* lines, size_t num_lines);
};

struct m2d_device
{
    const char* name;
    const struct m2d_capabilities* caps;
    const struct m2d_device_funcs* funcs;

    int fd;
    uint32_t next_id;
};

#define INIT_DEVICE(member, nm, cp, fn)             \
    .member =                                       \
    {                                               \
        .name = nm,                                 \
        .caps = cp,                                 \
        .funcs = fn,                                \
        .fd = -1,                                   \
    }

struct m2d_device* m2d_get_device();

bool m2d_intersect(const struct m2d_rectangle* a,
                   const struct m2d_rectangle* b,
                   struct m2d_rectangle* result);

#define LIBM2D_LEVEL_TRACE 0
#define LIBM2D_LEVEL_DEBUG 1
#define LIBM2D_LEVEL_INFO 2
#define LIBM2D_LEVEL_WARN 3
#define LIBM2D_LEVEL_ERROR 4
#define LIBM2D_LEVEL_OFF 5

#ifndef LIBM2D_ACTIVE_LEVEL
#define LIBM2D_ACTIVE_LEVEL LIBM2D_LEVEL_INFO
#endif

void m2d_log_v(int level, const char* format, va_list ap);

static inline void m2d_log(int level, const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    m2d_log_v(level, format, ap);
    va_end(ap);
}

#define DEFINE_MSG(_prefix, _level)                                     \
    static inline void _prefix##_msg_v(const char* format, va_list ap)  \
    {                                                                   \
	m2d_log_v(_level, format, ap);                                  \
    }                                                                   \
                                                                        \
    static inline void _prefix##_msg(const char* format, ...)           \
    {                                                                   \
	va_list ap;                                                     \
                                                                        \
	va_start(ap, format);                                           \
	_prefix##_msg_v(format, ap);                                    \
	va_end(ap);                                                     \
    }

DEFINE_MSG(trace, LIBM2D_LEVEL_TRACE)
DEFINE_MSG(debug, LIBM2D_LEVEL_DEBUG)
DEFINE_MSG(info, LIBM2D_LEVEL_INFO)
DEFINE_MSG(warn, LIBM2D_LEVEL_WARN)
DEFINE_MSG(error, LIBM2D_LEVEL_ERROR)

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_TRACE
#define LIBM2D_TRACE(format, ...) trace_msg(format, ##__VA_ARGS__)
#else
#define LIBM2D_TRACE(...) ((void)0)
#endif

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_DEBUG
#define LIBM2D_DEBUG(format, ...) debug_msg(format, ##__VA_ARGS__)
#else
#define LIBM2D_DEBUG(...) ((void)0)
#endif

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_INFO
#define LIBM2D_INFO(format, ...) info_msg(format, ##__VA_ARGS__)
#else
#define LIBM2D_INFO(...) ((void)0)
#endif

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_WARN
#define LIBM2D_WARN(format, ...) warn_msg(format, ##__VA_ARGS__)
#else
#define LIBM2D_WARN(...) ((void)0)
#endif

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_ERROR
#define LIBM2D_ERROR(format, ...) error_msg(format, ##__VA_ARGS__)
#else
#define LIBM2D_ERROR(...) ((void)0)
#endif

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_TRACE
void m2d_print_rectangles(const struct m2d_rectangle* rects, size_t num_rects);
#else
static inline void
m2d_print_rectangles(const struct m2d_rectangle* rects, size_t num_rects)
{
    (void)rects;
    (void)num_rects;
}
#endif

size_t m2d_byte_per_pixel(enum m2d_pixel_format format);

#endif /* M2D_PRIV_H */
