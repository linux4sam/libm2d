/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d_priv.h"

#include <stdio.h>
#include <stdlib.h>

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

    default:
        break;
    }

    return 0;
}
