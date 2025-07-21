/*
 * Copyright (C) 2025 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d/m2d.h"
#include "m2d_priv.h"
#include "vivante_gc/cmdstream.xml.h"
#include "vivante_gc/state.xml.h"
#include "vivante_gc/state_2d.xml.h"
#include "vivante_gc/vivante_gc.h"
#include "vivante_gc/vivante_gc_priv.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GC_MAX_SOURCES 8
#define GC_SINGLE_SOURCE ~0
#define GC_IS_SINGLE_SOURCE(index) (((index) & ~0x7) != 0)

#define BLOCK8(name, index) (GC_IS_SINGLE_SOURCE(index) ? VIVS_DE_ ## name : (VIVS_DE_BLOCK8_ ## name(index)))

#define GC_DEST_CONFIG(_cmd) ((dev.dest_config & ~VIVS_DE_DEST_CONFIG_COMMAND__MASK) | VIVS_DE_DEST_CONFIG_COMMAND_ ## _cmd)

#define GC_RESERVE_LOAD_STATE(_count) ALIGN(1 + (_count), 2)
#define GC_RESERVE_LOAD_TARGET (GC_RESERVE_LOAD_STATE(3) + (3 * GC_RESERVE_LOAD_STATE(1)))
#define GC_RESERVE_LOAD_ALPHA_BLENDING (5 * GC_RESERVE_LOAD_STATE(1))
#define GC_RESERVE_LOAD_SOURCE (GC_RESERVE_LOAD_ALPHA_BLENDING + (5 * GC_RESERVE_LOAD_STATE(1)))
#define GC_RESERVE_SOURCES(_num_sources) ((_num_sources) * )
#define GC_RESERVE_DRAW_2D(_num_rects) ((1 + (_num_rects)) << 1)

#define GC_RESERVE_INIT (GC_RESERVE_LOAD_STATE(GC_MAX_SOURCES) + (4 * GC_RESERVE_LOAD_STATE(1)))

#define GC_RESERVE_CLEAR_FIRST ((2 * GC_RESERVE_LOAD_STATE(1)) + GC_RESERVE_LOAD_ALPHA_BLENDING)

#define GC_RESERVE_BLIT_FIRST GC_RESERVE_LOAD_STATE(1)
#define GC_RESERVE_BLIT_SOURCE GC_RESERVE_LOAD_SOURCE
#define GC_RESERVE_BLIT_RECT ((2 * GC_RESERVE_LOAD_STATE(1)) + GC_RESERVE_DRAW_2D(1))

#define GC_RESERVE_MULTI_BLIT_FIRST GC_RESERVE_LOAD_STATE(1)
#define GC_RESERVE_MULTI_BLIT_SOURCES (GC_MAX_SOURCES * (GC_RESERVE_LOAD_SOURCE + (5 * GC_RESERVE_LOAD_STATE(1))))
#define GC_RESERVE_MULTI_BLIT_RECT (GC_RESERVE_LOAD_STATE(1) + GC_RESERVE_DRAW_2D(1))

#define gc_on_error(_expr) do { error = _expr; if (error) { goto on_error; } } while (0)

#define gc_load_state_header(_memory, _address, _count)                 \
    do {                                                                \
        uint32_t __address = (_address);                                \
        uint32_t __count = (_count);                                    \
        *_memory++ = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |           \
                     VIV_FE_LOAD_STATE_HEADER_OFFSET(__address >> 2) |  \
                     VIV_FE_LOAD_STATE_HEADER_COUNT(__count);           \
    } while (0)

#define gc_load_state(_memory, _address, _value)     \
    do {                                             \
        gc_load_state_header(_memory, _address, 1);  \
        *_memory++ = (_value);                       \
    } while (0)

#define GC_NEED_DUMMY(_count) (((_count) & 1) == 0)
#define GC_DUMMY 0xdeadbeef

#define GC_ALPHA_MASK 0xff000000u
#define GC_COLOR_MASK 0x00ffffffu

struct gc_source
{
    struct gc_buffer* buf;
    dim_t x;
    dim_t y;
    uint32_t source_color;
    uint32_t blend_color;
    enum m2d_blend_factor src_color_factor;
    enum m2d_blend_factor dst_color_factor;
    enum m2d_blend_factor src_alpha_factor;
    enum m2d_blend_factor dst_alpha_factor;
    bool blend_enabled;
    bool enabled;
};

enum gc_global_alpha_mode
{
    GAM_NORMAL,
    GAM_GLOBAL,
    GAM_SCALED,
};

enum gc_src_global_premultiply
{
    SGPM_DISABLE,
    SGPM_ALPHA,
    SGPM_COLOR,
};

struct gc_gpu_state
{
    struct gc_buffer* target;
    struct gc_source sources[GC_MAX_SOURCES];
    size_t current_source;
};

struct gc_device
{
    struct m2d_device base;
    uint32_t dest_config;
    struct gc_buffer* current_target;
    struct gc_gpu_state state;
};

struct gc_reloc
{
    const struct m2d_buffer* buf;
    size_t offset;
};

static const struct m2d_capabilities gc_caps =
{
    .stride_alignment = GC_STRIDE_ALIGNMENT,
    .blit_max_sources = GC_MAX_SOURCES,
    .per_source_blend_params = true,
    .draw_lines = false, /* not implemented yet */
    .stretched_blit = false, /* not implemented yet */
};

static int gc_init();
static void gc_cleanup();
static struct m2d_buffer* gc_create(size_t width, size_t height,
                                    enum m2d_pixel_format format,
                                    size_t* stride);
static struct m2d_buffer* gc_import(const struct m2d_import_desc* desc);
static void gc_draw_rectangles(const struct m2d_rectangle* rects,
                               size_t num_rects);

static const struct m2d_device_funcs gc_device_funcs =
{
    .init = gc_init,
    .cleanup = gc_cleanup,
    .create = gc_create,
    .import = gc_import,
    .free = gc_free,
    .sync_for_cpu = gc_sync_for_cpu,
    .sync_for_gpu = gc_sync_for_gpu,
    .wait = gc_wait,
    .draw_rectangles = gc_draw_rectangles,
};

static struct gc_device dev =
{
    INIT_DEVICE(base, NULL, &gc_caps, &gc_device_funcs),
};

static inline struct gc_buffer* to_gc_buffer(const struct m2d_buffer* buf)
{
    return buf ? container_of(buf, struct gc_buffer, base) : NULL;
}

static uint32_t to_gc_format(enum m2d_pixel_format format)
{
    switch (format)
    {
    case M2D_PF_ARGB8888:
        return DE_FORMAT_A8R8G8B8;

    case M2D_PF_RGB565:
        return DE_FORMAT_R5G6B5;

    case M2D_PF_A8:
        return DE_FORMAT_A8;
    }

    return DE_FORMAT_A8R8G8B8;
}

static struct m2d_buffer* gc_create(size_t width, size_t height,
                                    enum m2d_pixel_format format, size_t* size)
{
    struct m2d_buffer* buf;

    buf = gc_driver_create(width, height, format, size);
    if (buf)
        to_gc_buffer(buf)->gpu_format = to_gc_format(format);

    return buf;
}

static struct m2d_buffer* gc_import(const struct m2d_import_desc* desc)
{
    struct m2d_buffer* buf;

    buf = gc_driver_import(desc);
    if (buf)
        to_gc_buffer(buf)->gpu_format = to_gc_format(desc->format);

    return buf;
}

static inline uint32_t gc_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

static size_t to_gc_source_index(enum m2d_source_id id)
{
    switch (id)
    {
    case M2D_DST:
        return 0;

    case M2D_SRC:
        return 1;

    case M2D_SRC0:
        return 0;

    case M2D_SRC1:
        return 1;

    case M2D_SRC2:
        return 2;

    case M2D_SRC3:
        return 3;

    case M2D_SRC4:
        return 4;

    case M2D_SRC5:
        return 5;

    case M2D_SRC6:
        return 6;

    case M2D_SRC7:
        return 7;

    default:
        break;
    }

    return 0;
}

static void gc_init_state()
{
    size_t i;

    memset(&dev.state, 0, sizeof(dev.state));
    dev.state.current_source = to_gc_source_index(M2D_SRC);

    for (i = 0; i < ARRAY_SIZE(dev.state.sources); i++)
    {
        struct gc_source* src = &dev.state.sources[i];

        src->source_color = 0xffffffffu;
    }
}

static int gc_init_gpu_registers()
{
    uint32_t* memory;
    uint32_t rop;
    size_t i;
    uint32_t* rollback = NULL;
    int error = 0;

    dev.dest_config = VIVS_DE_DEST_CONFIG_SWIZZLE(DE_SWIZZLE_ARGB) |
                      VIVS_DE_DEST_CONFIG_ENDIAN_CONTROL(0) |
                      VIVS_DE_DEST_CONFIG_STRETCH_QUAD_DISABLED;

    gc_on_error(gc_reserve(GC_RESERVE_INIT, &rollback));
    memory = rollback;

    rop = VIVS_DE_ROP_ROP_FG(0xcc) | /* SRC copy */
          VIVS_DE_ROP_ROP_BG(0xcc) | /* SRC copy */
          VIVS_DE_ROP_TYPE_ROP4;

    gc_load_state_header(memory, VIVS_DE_BLOCK8_ROP(0), GC_MAX_SOURCES);
    for (i = 0; i < GC_MAX_SOURCES; i++)
        *memory++ = rop;
    if (GC_NEED_DUMMY(GC_MAX_SOURCES))
        *memory++ = GC_DUMMY;

    gc_load_state(memory, VIVS_DE_ROP, rop);
    gc_load_state(memory, VIVS_DE_CLEAR_BYTE_MASK, 0xff);
    gc_load_state(memory, VIVS_DE_PE_DITHER_LOW, 0xffffffff);
    gc_load_state(memory, VIVS_DE_PE_DITHER_HIGH, 0xffffffff);

    m2d_assert(memory == rollback + GC_RESERVE_INIT);

    return gc_flush(false);

on_error:
    if (rollback)
        gc_truncate(rollback);
    return error;
}

static int gc_init()
{
    if (gc_driver_init())
        return -1;

    gc_init_state();

    if (gc_init_gpu_registers())
    {
        gc_driver_cleanup();
        return -1;
    }

    return 0;
}

static void gc_cleanup()
{
    gc_driver_cleanup();
}

static int gc_load_target(uint32_t** memory)
{
    const struct gc_buffer* priv_buf;
    const struct m2d_buffer* buf;
    uint32_t* mem;
    int error = 0;

    if (dev.current_target == dev.state.target)
        return 0;

    priv_buf = dev.state.target;
    buf = &priv_buf->base;

    dev.dest_config = (dev.dest_config & ~VIVS_DE_DEST_CONFIG_FORMAT__MASK) | VIVS_DE_DEST_CONFIG_FORMAT(priv_buf->gpu_format);

    mem = *memory;

    gc_load_state_header(mem, VIVS_DE_DEST_ADDRESS, 3);
    gc_on_error(gc_write_buffer_address(mem++, buf, true));
    *mem++ = VIVS_DE_DEST_STRIDE_STRIDE(buf->stride);
    *mem++ = VIVS_DE_DEST_ROTATION_CONFIG_WIDTH(buf->width) | VIVS_DE_DEST_ROTATION_CONFIG_ROTATION_DISABLE;

    gc_load_state(mem, VIVS_DE_DEST_ROTATION_HEIGHT, VIVS_DE_DEST_ROTATION_HEIGHT_HEIGHT(buf->height));
    gc_load_state(mem, VIVS_DE_CLIP_TOP_LEFT, 0);
    gc_load_state(mem, VIVS_DE_CLIP_BOTTOM_RIGHT, VIVS_DE_CLIP_BOTTOM_RIGHT_X(buf->width) | VIVS_DE_CLIP_BOTTOM_RIGHT_Y(buf->height));

    *memory = mem;

    return 0;

on_error:
    return error;
}

static int gc_get_src_alpha_modes(const struct gc_source* src,
                                  enum gc_global_alpha_mode* global_src_alpha_mode,
                                  uint8_t* src_blending_mode,
                                  bool* src_alpha_factor,
                                  bool* src_premultiply,
                                  enum gc_src_global_premultiply* src_global_premultiply,
                                  uint32_t* global_src_color)
{
    const enum m2d_blend_factor color_factor = src->src_color_factor;
    const enum m2d_blend_factor alpha_factor = src->src_alpha_factor;
    uint32_t global_color = src->enabled ? src->source_color : 0xffffffffu;
    uint32_t tmp_color;

    *global_src_alpha_mode = GAM_NORMAL;
    *src_blending_mode = DE_BLENDMODE_ONE;
    *src_alpha_factor = false;
    *src_premultiply = false;
    *src_global_premultiply = SGPM_DISABLE;

    switch (color_factor)
    {
    case M2D_BLEND_ZERO:
        *src_blending_mode = DE_BLENDMODE_ZERO;
        break;

    case M2D_BLEND_ONE:
        *src_blending_mode = DE_BLENDMODE_ONE;
        break;

    case M2D_BLEND_DST_COLOR:
        *src_blending_mode = DE_BLENDMODE_COLOR;
        break;

    case M2D_BLEND_ONE_MINUS_DST_COLOR:
        *src_blending_mode = DE_BLENDMODE_COLOR_INVERSED;
        break;

    case M2D_BLEND_SRC_ALPHA:
        *src_alpha_factor = true;
        *src_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_ONE_MINUS_SRC_ALPHA:
        *src_alpha_factor = true;
        *src_blending_mode = DE_BLENDMODE_INVERSED;
        break;

    case M2D_BLEND_DST_ALPHA:
        *src_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_ONE_MINUS_DST_ALPHA:
        *src_blending_mode = DE_BLENDMODE_INVERSED;
        break;

    case M2D_BLEND_CONSTANT_COLOR:
        global_color = m2d_multiply_colors(global_color, src->blend_color);
        *global_src_alpha_mode = GAM_GLOBAL;
        *src_alpha_factor = true;
        *src_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_ONE_MINUS_CONSTANT_COLOR:
        tmp_color = m2d_one_minus_color(src->blend_color);
        global_color = m2d_multiply_colors(global_color, tmp_color);
        *global_src_alpha_mode = GAM_GLOBAL;
        *src_alpha_factor = true;
        *src_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_CONSTANT_ALPHA:
    case M2D_BLEND_ONE_MINUS_CONSTANT_ALPHA:
        if (color_factor == M2D_BLEND_CONSTANT_ALPHA)
            tmp_color = src->blend_color;
        else
            tmp_color = m2d_one_minus_color(src->blend_color);
        tmp_color = m2d_multiply_colors(global_color, tmp_color);
        global_color = (global_color & GC_COLOR_MASK) | (tmp_color & GC_ALPHA_MASK);
        *global_src_alpha_mode = GAM_GLOBAL;
        *src_alpha_factor = true;
        *src_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_SRC_ALPHA_SATURATE:
        *src_blending_mode = DE_BLENDMODE_SATURATED_ALPHA;
        break;

    default:
        LIBM2D_ERROR("unsupported source blend factors: {color_factor=%s, alpha_factor=%s}\n",
                     m2d_blend_factor_name(color_factor),
                     m2d_blend_factor_name(alpha_factor));
        return -1;
    }

    *global_src_color = global_color;
    if ((global_color >> 24) != 0xffu && *global_src_alpha_mode == GAM_NORMAL)
        *global_src_alpha_mode = GAM_SCALED;

    if ((global_color & GC_COLOR_MASK) != GC_COLOR_MASK)
        *src_global_premultiply = SGPM_COLOR;

    return 0;
}

static int gc_get_dst_alpha_modes(const struct gc_source* src,
                                  enum gc_global_alpha_mode* global_dst_alpha_mode,
                                  uint8_t* dst_blending_mode,
                                  bool* dst_alpha_factor,
                                  bool* dst_premultiply,
                                  uint32_t* global_dst_color)
{
    const enum m2d_blend_factor color_factor = src->dst_color_factor;
    const enum m2d_blend_factor alpha_factor = src->dst_alpha_factor;

    *global_dst_alpha_mode = GAM_NORMAL;
    *dst_blending_mode = DE_BLENDMODE_ZERO;
    *dst_alpha_factor = false;
    *dst_premultiply = false;
    *global_dst_color = 0xffffffffu;

    switch (color_factor)
    {
    case M2D_BLEND_ZERO:
        *dst_blending_mode = DE_BLENDMODE_ZERO;
        break;

    case M2D_BLEND_ONE:
        *dst_blending_mode = DE_BLENDMODE_ONE;
        break;

    case M2D_BLEND_SRC_COLOR:
        *dst_blending_mode = DE_BLENDMODE_COLOR;
        break;

    case M2D_BLEND_ONE_MINUS_SRC_COLOR:
        *dst_blending_mode = DE_BLENDMODE_COLOR_INVERSED;
        break;

    case M2D_BLEND_SRC_ALPHA:
        *dst_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_ONE_MINUS_SRC_ALPHA:
        *dst_blending_mode = DE_BLENDMODE_INVERSED;
        break;

    case M2D_BLEND_DST_ALPHA:
        *dst_alpha_factor = true;
        *dst_blending_mode = DE_BLENDMODE_NORMAL;
        break;

    case M2D_BLEND_ONE_MINUS_DST_ALPHA:
        *dst_alpha_factor = true;
        *dst_blending_mode = DE_BLENDMODE_INVERSED;
        break;

    case M2D_BLEND_CONSTANT_ALPHA:
    case M2D_BLEND_ONE_MINUS_CONSTANT_ALPHA:
        *global_dst_color = src->blend_color | GC_COLOR_MASK;
        *dst_alpha_factor = true;
        *global_dst_alpha_mode = GAM_GLOBAL;
        if (color_factor == M2D_BLEND_CONSTANT_ALPHA)
            *dst_blending_mode = DE_BLENDMODE_NORMAL;
        else
            *dst_blending_mode = DE_BLENDMODE_INVERSED;
        break;

    default:
        LIBM2D_ERROR("unsupported destination blend factors: {color_factor=%s, alpha_factor=%s}\n",
                     m2d_blend_factor_name(color_factor),
                     m2d_blend_factor_name(alpha_factor));
        return -1;
    }

    return 0;
}

static int gc_load_alpha_blending(uint32_t** memory, size_t index, const struct gc_source* src)
{
    enum gc_src_global_premultiply src_global_premultiply;
    enum gc_global_alpha_mode global_src_alpha_mode;
    enum gc_global_alpha_mode global_dst_alpha_mode;
    uint32_t global_src_color;
    uint32_t global_dst_color;
    bool src_premultiply;
    bool dst_premultiply;
    bool src_alpha_factor;
    bool dst_alpha_factor;
    uint8_t src_blending_mode;
    uint8_t dst_blending_mode;
    uint32_t values[3];
    uint32_t* mem = *memory;

    if (!src->blend_enabled)
    {
        gc_load_state(mem, BLOCK8(ALPHA_CONTROL, index), VIVS_DE_ALPHA_CONTROL_ENABLE_OFF);

        values[0] = 0xffffffffu;
        values[1] = 0xffffffffu;
        values[2] = 0;

        if (GC_IS_SINGLE_SOURCE(index))
        {
            gc_load_state_header(mem, VIVS_DE_GLOBAL_SRC_COLOR, 3);
            *mem++ = values[0];
            *mem++ = values[1];
            *mem++ = values[2];
        }
        else
        {
            gc_load_state(mem, VIVS_DE_BLOCK8_GLOBAL_SRC_COLOR(index), values[0]);
            gc_load_state(mem, VIVS_DE_BLOCK8_GLOBAL_DEST_COLOR(index), values[1]);
            gc_load_state(mem, VIVS_DE_BLOCK8_COLOR_MULTIPLY_MODES(index), values[2]);
        }
        goto out;
    }

    if (gc_get_src_alpha_modes(src,
                               &global_src_alpha_mode,
                               &src_blending_mode,
                               &src_alpha_factor,
                               &src_premultiply,
                               &src_global_premultiply,
                               &global_src_color))
        return -1;

    if (gc_get_dst_alpha_modes(src,
                               &global_dst_alpha_mode,
                               &dst_blending_mode,
                               &dst_alpha_factor,
                               &dst_premultiply,
                               &global_dst_color))
        return -1;

    gc_load_state(mem, BLOCK8(ALPHA_CONTROL, index), VIVS_DE_ALPHA_CONTROL_ENABLE_ON);

    gc_load_state(mem, BLOCK8(ALPHA_MODES, index),
                  VIVS_DE_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL | /* src alpha is not inverted */
                  VIVS_DE_ALPHA_MODES_DST_ALPHA_MODE_NORMAL | /* dst alpha is not inverted */
                  VIVS_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE(global_src_alpha_mode) |
                  VIVS_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE(global_dst_alpha_mode) |
                  VIVS_DE_ALPHA_MODES_SRC_BLENDING_MODE(src_blending_mode) |
                  VIVS_DE_ALPHA_MODES_SRC_ALPHA_FACTOR(src_alpha_factor) |
                  VIVS_DE_ALPHA_MODES_DST_BLENDING_MODE(dst_blending_mode) |
                  VIVS_DE_ALPHA_MODES_DST_ALPHA_FACTOR(dst_alpha_factor));

    values[0] = global_src_color;
    values[1] = global_dst_color;
    values[2] = VIVS_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY(src_premultiply) |
                VIVS_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY(dst_premultiply) |
                VIVS_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY(src_global_premultiply) |
                VIVS_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE;

    if (GC_IS_SINGLE_SOURCE(index))
    {
        gc_load_state_header(mem, VIVS_DE_GLOBAL_SRC_COLOR, 3);
        *mem++ = values[0];
        *mem++ = values[1];
        *mem++ = values[2];
    }
    else
    {
        gc_load_state(mem, VIVS_DE_BLOCK8_GLOBAL_SRC_COLOR(index), values[0]);
        gc_load_state(mem, VIVS_DE_BLOCK8_GLOBAL_DEST_COLOR(index), values[1]);
        gc_load_state(mem, VIVS_DE_BLOCK8_COLOR_MULTIPLY_MODES(index), values[2]);
    }

out:
    *memory = mem;
    return 0;
}

static int gc_load_source(uint32_t** memory, uint32_t* source_memory,
                          size_t index, const struct gc_source* src,
                          struct gc_reloc* reloc)
{
    const struct gc_buffer* priv_buf = src->buf;
    const struct m2d_buffer* buf = &priv_buf->base;
    uint32_t* mem = *memory;
    int error = 0;

    if (!priv_buf)
    {
        LIBM2D_ERROR("can't set source surface %zu: buffer is NULL\n",
                     GC_IS_SINGLE_SOURCE(index) ? 0 : index);
        return -1;
    }

    gc_on_error(gc_load_alpha_blending(&mem, index, src));

    gc_load_state_header(mem, BLOCK8(SRC_ADDRESS, index), 1);
    reloc->buf = buf;
    reloc->offset = mem++ - source_memory;

    gc_load_state(mem, BLOCK8(SRC_STRIDE, index),
                  VIVS_DE_SRC_STRIDE_STRIDE(buf->stride));
    gc_load_state(mem, BLOCK8(SRC_ROTATION_CONFIG, index),
                  VIVS_DE_SRC_ROTATION_CONFIG_ROTATION_DISABLE |
                  VIVS_DE_SRC_ROTATION_CONFIG_WIDTH(buf->width));
    gc_load_state(mem, BLOCK8(SRC_CONFIG, index),
                  VIVS_DE_SRC_CONFIG_LOCATION_MEMORY |
                  VIVS_DE_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE |
                  VIVS_DE_SRC_CONFIG_SOURCE_FORMAT(priv_buf->gpu_format));
    gc_load_state(mem, BLOCK8(SRC_ROTATION_HEIGHT, index),
                  VIVS_DE_SRC_ROTATION_HEIGHT_HEIGHT(buf->height));

    *memory = mem;
on_error:
    return error;
}

static int gc_reloc_sources(uint32_t** memory, const uint32_t* source_memory, size_t reserved_source_size,
                            const struct gc_reloc* relocs, size_t num_relocs)
{
    uint32_t* mem = *memory;
    size_t i;
    int error = 0;

    memcpy(mem, source_memory, reserved_source_size * sizeof(uint32_t));
    for (i = 0; i < num_relocs; i++)
    {
        const struct gc_reloc* reloc = &relocs[i];

        gc_on_error(gc_write_buffer_address(&mem[reloc->offset], reloc->buf, false));
    }
    mem += reserved_source_size;

    *memory = mem;

on_error:
    return error;
}

static inline void gc_draw_2d(uint32_t** memory, const struct m2d_rectangle* r)
{
    uint32_t* m = *memory;

    *m++ = VIV_FE_DRAW_2D_HEADER_OP_DRAW_2D | VIV_FE_DRAW_2D_HEADER_COUNT(1);
    *m++ = 0; /* rectangle starts aligned */
    *m++ = VIV_FE_DRAW_2D_TOP_LEFT_X(r->x) | VIV_FE_DRAW_2D_TOP_LEFT_Y(r->y);
    *m++ = VIV_FE_DRAW_2D_BOTTOM_RIGHT_X(r->x + r->w) | VIV_FE_DRAW_2D_BOTTOM_RIGHT_Y(r->y + r->h);

    *memory = m;
}

static int gc_clear(const struct m2d_rectangle* rects, size_t num_rects, void* args)
{
    static const size_t cmd_words = 2; /* 2-word draw 2d command */
    static const size_t min_words = cmd_words + 2; /* 2-word command + 2-word rectangle */
    static const size_t max_rects = VIV_FE_DRAW_2D_HEADER_COUNT__MASK >> VIV_FE_DRAW_2D_HEADER_COUNT__SHIFT;
    const struct gc_source* src = (const struct gc_source*)args;
    struct gc_buffer* target = dev.state.target;
    size_t max_reserved = gc_get_max_reserved();
    const struct m2d_rectangle* rect = rects;
    struct m2d_rectangle clip_rect;
    uint32_t* rollback = NULL;
    size_t total_rects = 0;
    bool first = true;
    int error = 0;

    if (src->blend_enabled)
        LIBM2D_DEBUG("blending %zu rectangle%s with source color %08x into target buffer %u\n",
                     num_rects, num_rects > 1 ? "s" : "",
                     src->source_color, target->base.id);
    else
        LIBM2D_DEBUG("clearing %zu rectangle%s with source color %08x into target buffer %u\n",
                     num_rects, num_rects > 1 ? "s" : "",
                     src->source_color, target->base.id);

    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.w = target->base.width;
    clip_rect.h = target->base.height;

    while (num_rects > 0)
    {
        size_t available = gc_get_available();
        size_t i, num, count = 0;
        uint32_t* final_memory;
        uint32_t* memory;
        size_t reserved_size;

        if (available >= min_words)
            num = min_size_t((available - cmd_words) >> 1, num_rects);
        else
            num = min_size_t((max_reserved - cmd_words) >> 1, num_rects);

        num = min_size_t(num, max_rects);
        reserved_size = GC_RESERVE_DRAW_2D(num);

        if (first)
        {
            reserved_size += GC_RESERVE_CLEAR_FIRST;
            if (dev.current_target != target)
                reserved_size += GC_RESERVE_LOAD_TARGET;
        }

        gc_on_error(gc_reserve(reserved_size, &rollback));
        memory = rollback;

        if (first)
        {
            gc_on_error(gc_load_target(&memory));
            gc_load_state(memory, VIVS_DE_DEST_CONFIG, GC_DEST_CONFIG(CLEAR));
            gc_load_state(memory, VIVS_DE_CLEAR_PIXEL_VALUE32, src->source_color);
            gc_on_error(gc_load_alpha_blending(&memory, GC_SINGLE_SOURCE, src));
            first = false;
        }

        LIBM2D_TRACE("drawing %zu rectangle%s into buffer %u:\n",
                     num, num > 1 ? "s" : "", target->base.id);
        m2d_print_rectangles(rect, num);

        final_memory = memory + cmd_words; /* rectangles start aligned */
        for (i = 0; i < num; i++, rect++)
        {
            struct m2d_rectangle r;

            if (!m2d_intersect(&clip_rect, rect, &r))
                continue;

            *final_memory++ = VIV_FE_DRAW_2D_TOP_LEFT_X(r.x) | VIV_FE_DRAW_2D_TOP_LEFT_Y(r.y);
            *final_memory++ = VIV_FE_DRAW_2D_BOTTOM_RIGHT_X(r.x + r.w) |
                              VIV_FE_DRAW_2D_BOTTOM_RIGHT_Y(r.y + r.h);

            count++;
        }

        if (count)
        {
            memory[0] = VIV_FE_DRAW_2D_HEADER_OP_DRAW_2D | VIV_FE_DRAW_2D_HEADER_COUNT(count);
            memory[1] = 0;
            total_rects += count;
        }
        else
        {
            final_memory = memory;
        }

        if (final_memory != (rollback + reserved_size))
        {
            m2d_assert(final_memory <= rollback + reserved_size);
            gc_truncate(final_memory);
        }

        num_rects -= num;
    }

    if (total_rects)
        dev.current_target = dev.state.target;
    return 0;

on_error:
    if (rollback)
        gc_truncate(rollback);
    return error;
}

static int gc_blit(const struct m2d_rectangle* rects, size_t num_rects, void* args)
{
    const struct gc_source* src = (const struct gc_source*)args;
    struct gc_buffer* target = dev.state.target;
    struct m2d_rectangle target_rect;
    struct m2d_rectangle clip_rect;
    struct m2d_rectangle src_rect;
    size_t i;
    struct gc_reloc reloc;
    uint32_t source_memory[GC_RESERVE_BLIT_SOURCE];
    uint32_t* src_mem = source_memory;
    size_t reserved_source_size;
    uint32_t* rollback = NULL;
    size_t total_rects = 0;
    bool first = true;
    int error = 0;

    if (src->blend_enabled)
        LIBM2D_DEBUG("blending %zu rectangle%s from source buffer %u into target buffer %u\n",
                     num_rects, num_rects > 1 ? "s" : "",
                     src->buf->base.id, target->base.id);
    else
        LIBM2D_DEBUG("copying %zu rectangle%s from source buffer %u into target buffer %u\n",
                     num_rects, num_rects > 1 ? "s" : "",
                     src->buf->base.id, target->base.id);

    target_rect.x = 0;
    target_rect.y = 0;
    target_rect.w = target->base.width;
    target_rect.h = target->base.height;

    src_rect.x = src->x;
    src_rect.y = src->y;
    src_rect.w = src->buf->base.width;
    src_rect.h = src->buf->base.height;

    if (!m2d_intersect(&target_rect, &src_rect, &clip_rect))
        return 0;

    gc_on_error(gc_load_source(&src_mem, source_memory, GC_SINGLE_SOURCE, src, &reloc));
    reserved_source_size = src_mem - source_memory;

    for (i = 0; i < num_rects; i++)
    {
        const struct m2d_rectangle* rect = &rects[i];
        struct m2d_rectangle r;
        uint32_t* memory;
        size_t reserved_size;

        if (!m2d_intersect(&clip_rect, rect, &r))
            continue;

        reserved_size = GC_RESERVE_BLIT_RECT;
        if (first)
        {
            reserved_size += GC_RESERVE_BLIT_FIRST + reserved_source_size;
            if (dev.current_target != target)
                reserved_size += GC_RESERVE_LOAD_TARGET;
        }

        gc_on_error(gc_reserve(reserved_size, &rollback));
        memory = rollback;

        if (first)
        {
            gc_on_error(gc_load_target(&memory));
            gc_load_state(memory, VIVS_DE_DEST_CONFIG, GC_DEST_CONFIG(BIT_BLT));
            gc_on_error(gc_reloc_sources(&memory, source_memory, reserved_source_size, &reloc, 1));
            first = false;
        }

        gc_load_state(memory, VIVS_DE_SRC_ORIGIN, VIVS_DE_SRC_ORIGIN_X(r.x - src_rect.x) | VIVS_DE_SRC_ORIGIN_Y(r.y - src_rect.y));
        gc_load_state(memory, VIVS_DE_SRC_SIZE, VIVS_DE_SRC_SIZE_X(r.w) | VIVS_DE_SRC_SIZE_Y(r.h));
        gc_draw_2d(&memory, &r);

        m2d_assert(memory == rollback + reserved_size);
        total_rects++;
    }

    if (total_rects)
        dev.current_target = dev.state.target;
    return 0;

on_error:
    if (rollback)
        gc_truncate(rollback);
    return error;
}

static int gc_multi_blit(const struct m2d_rectangle* rects, size_t num_rects, void* args)
{
    struct gc_buffer* target = dev.state.target;
    uint32_t source_mask = *(const uint32_t*)args;
    struct m2d_rectangle clip_rects[GC_MAX_SOURCES];
    struct m2d_rectangle target_rect;
    char source_list[256];
    size_t i, j, index = 0;
    bool first_source = true;
    int len = 0;
    uint32_t* rollback = NULL;
    size_t total_rects = 0;
    bool first = true;
    int error = 0;

    /* LIBM2D_DEBUG() variables. */
    (void)source_list;
    (void)first_source;
    (void)len;

    target_rect.x = 0;
    target_rect.y = 0;
    target_rect.w = target->base.width;
    target_rect.h = target->base.height;

    /* Set sources. */
    for (j = 0; j < ARRAY_SIZE(dev.state.sources); j++)
    {
        const struct gc_source* src = &dev.state.sources[j];
        struct m2d_rectangle* clip_rect = &clip_rects[j];
        struct m2d_rectangle src_rect;

        if (!(source_mask & BIT(j)))
            continue;

        src_rect.x = src->x;
        src_rect.y = src->y;
        src_rect.w = src->buf->base.width;
        src_rect.h = src->buf->base.height;
        if (!m2d_intersect(&target_rect, &src_rect, clip_rect))
        {
            source_mask &= ~BIT(j);
            continue;
        }

#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_DEBUG
        if (first_source)
            len += snprintf(source_list + len,
                            max_size_t(sizeof(source_list), len) - len,
                            "%u", src->buf->base.id);
        else
            len += snprintf(source_list + len,
                            max_size_t(sizeof(source_list), len) - len,
                            ", %u", src->buf->base.id);
        first_source = false;
#endif

        index++;
    }

    if (!source_mask)
        return 0;

    LIBM2D_DEBUG("multi blitting %zu rectangle%s from source buffers {%s} into target buffer %u\n",
                 num_rects, num_rects > 1 ? "s" : "",
                 source_list, target->base.id);

    for (i = 0; i < num_rects; i++)
    {
        const struct m2d_rectangle* rect = &rects[i];
        struct gc_reloc relocs[GC_MAX_SOURCES];
        uint32_t source_memory[GC_RESERVE_MULTI_BLIT_SOURCES];
        uint32_t* src_mem = source_memory;
        uint32_t* memory;
        size_t reserved_size;
        size_t reserved_source_size;

        rollback = NULL;

        index = 0;
        for (j = 0; j < ARRAY_SIZE(dev.state.sources); j++)
        {
            const struct gc_source* src = &dev.state.sources[j];
            const struct m2d_rectangle* clip_rect = &clip_rects[j];
            struct m2d_rectangle r;
            int min_x;
            int min_y;
            int max_x;
            int max_y;

            if (!(source_mask & BIT(j)))
                continue;

            if (!m2d_intersect(rect, clip_rect, &r))
                continue;

            min_x = r.x;
            min_y = r.y;
            max_x = r.x + r.w;
            max_y = r.y + r.h;

            gc_on_error(gc_load_source(&src_mem, source_memory, index, src, &relocs[index]));

            gc_load_state(src_mem, VIVS_DE_BLOCK8_MULTI_SRC_CONFIG(index), 0);

            gc_load_state(src_mem, BLOCK8(CLIP_TOP_LEFT, index),
                          VIVS_DE_BLOCK8_CLIP_TOP_LEFT_X(min_x) |
                          VIVS_DE_BLOCK8_CLIP_TOP_LEFT_Y(min_y));
            gc_load_state(src_mem, BLOCK8(CLIP_BOTTOM_RIGHT, index),
                          VIVS_DE_BLOCK8_CLIP_BOTTOM_RIGHT_X(max_x) |
                          VIVS_DE_BLOCK8_CLIP_BOTTOM_RIGHT_Y(max_y));

            /* This is not a mistake: SRC_ORIGIN and SRC_SIZE are set from `rect`, not from `r`. */
            gc_load_state(src_mem, BLOCK8(SRC_ORIGIN, index),
                          VIVS_DE_BLOCK8_SRC_ORIGIN_X(rect->x - src->x) |
                          VIVS_DE_BLOCK8_SRC_ORIGIN_Y(rect->y - src->y));
            gc_load_state(src_mem, BLOCK8(SRC_SIZE, index),
                          VIVS_DE_BLOCK8_SRC_SIZE_X(rect->w) |
                          VIVS_DE_BLOCK8_SRC_SIZE_Y(rect->h));

            index++;
        }

        if (!index)
            continue;

        trace_msg("rectangle %zu {origin: (%d,%d), size: [%dx%d]}\n",
                  i, rect->x, rect->y, rect->w, rect->h);

        reserved_source_size = src_mem - source_memory;
        reserved_size = GC_RESERVE_MULTI_BLIT_RECT + reserved_source_size;

        if (first)
        {
            reserved_size += GC_RESERVE_MULTI_BLIT_FIRST;
            if (dev.current_target != target)
                reserved_size += GC_RESERVE_LOAD_TARGET;
        }

        gc_on_error(gc_reserve(reserved_size, &rollback));
        memory = rollback;

        if (first)
        {
            gc_on_error(gc_load_target(&memory));
            gc_load_state(memory, VIVS_DE_DEST_CONFIG, GC_DEST_CONFIG(MULTI_SOURCE_BLT));
            first = false;
        }

        gc_on_error(gc_reloc_sources(&memory, source_memory, reserved_source_size, relocs, index));

        gc_load_state(memory, VIVS_DE_DE_MULTI_SOURCE,
                      VIVS_DE_DE_MULTI_SOURCE_MAX_SOURCE(index - 1) |
                      VIVS_DE_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128 |
                      VIVS_DE_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE1);
        gc_draw_2d(&memory, rect);

        m2d_assert(memory == rollback + reserved_size);
        total_rects++;
    }

    if (total_rects)
        dev.current_target = dev.state.target;
    return 0;

on_error:
    if (rollback)
        gc_truncate(rollback);
    return error;
}

static int gc_do_draw_rectangles(const struct m2d_rectangle* rects,
                                 size_t num_rects)
{
    int (*draw_rectangles)(const struct m2d_rectangle*, size_t, void*);
    struct m2d_buffer* bufs[GC_MAX_SOURCES + 1];
    struct gc_buffer* target = dev.state.target;
    const struct gc_source* selected_source;
    size_t i, count, dst_index;
    uint32_t source_mask;
    void* args;
    int error = 0;

    if (!num_rects)
        return 0;

    if (!rects)
    {
        LIBM2D_ERROR("rects pointer is NULL\n");
        return -1;
    }

    if (!target)
    {
        LIBM2D_ERROR("no target surface\n");
        return -1;
    }

    count = 0;
    source_mask = 0;
    dst_index = to_gc_source_index(M2D_DST);
    selected_source = &dev.state.sources[dev.state.current_source];
    for (i = 0; i < ARRAY_SIZE(dev.state.sources); i++)
    {
        const struct gc_source* src = &dev.state.sources[i];

        if (!src->enabled)
            continue;

        if (!src->buf)
        {
            LIBM2D_ERROR("source %zu is enabled but has no surface\n", i);
            return -1;
        }

        if (i == dst_index && src->buf == target && !src->x && !src->y && !src->blend_enabled)
            continue;

        selected_source = src;
        source_mask |= BIT(i);
        count++;
    }

    if (count > 1)
    {
        draw_rectangles = gc_multi_blit;
        args = &source_mask;
    }
    else if (count == 1)
    {
        draw_rectangles = gc_blit;
        args = (void*)selected_source;
    }
    else
    {
        draw_rectangles = gc_clear;
        args = (void*)selected_source;
    }

    gc_on_error(draw_rectangles(rects, num_rects, args));
    gc_on_error(gc_flush(true));

    count = 0;
    bufs[count++] = &target->base;
    for (i = 0; i < ARRAY_SIZE(dev.state.sources); i++)
        if (source_mask & BIT(i))
            bufs[count++] = &dev.state.sources[i].buf->base;
    gc_on_error(gc_set_fence(bufs, count));

    return 0;

on_error:
    dev.current_target = NULL;
    return error;
}

#ifdef VIVANTE_GC_DUMP_CMD_BUF
void gc_print_cmd_buf(const uint32_t* memory, size_t size)
{
    size_t i;

    trace_msg("\n");
    if (size & 1)
        error_msg("command buffer has odd number of 32-bit words: size=%zu\n",
                  size);

    trace_msg("command buffer has %zu 32-bit words:\n", size);
    for (i = 0; i < size; i += 2)
    {
        trace_msg("%04x: 0x%08x 0x%08x\n",
                  i << 2, memory[i], memory[i + 1]);
    }
    trace_msg("\n");
}
#endif

struct m2d_device* m2d_get_device()
{
    return &dev.base;
}

static void gc_draw_rectangles(const struct m2d_rectangle*  rects, size_t num_rects)
{
    (void)gc_do_draw_rectangles(rects, num_rects);
}

void m2d_set_target(struct m2d_buffer* buf)
{
    dev.state.target = to_gc_buffer(buf);
}

void m2d_set_source(enum m2d_source_id id, struct m2d_buffer* buf, dim_t x, dim_t y)
{
    int index = to_gc_source_index(id);

    struct gc_source* source = &dev.state.sources[index];
    source->buf = to_gc_buffer(buf);
    source->x = x;
    source->y = y;
}

void m2d_source_enable(enum m2d_source_id id, bool enabled)
{
    int index = to_gc_source_index(id);

    dev.state.sources[index].enabled = enabled;
}

void m2d_select_source(enum m2d_source_id id)
{
    dev.state.current_source = to_gc_source_index(id);
}

void m2d_source_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    struct gc_source* src = &dev.state.sources[dev.state.current_source];

    src->source_color = gc_color(red, green, blue, alpha);
}

void m2d_blend_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    struct gc_source* src = &dev.state.sources[dev.state.current_source];

    src->blend_color = gc_color(red, green, blue, alpha);
}

void m2d_blend_enable(bool enabled)
{
    struct gc_source* src = &dev.state.sources[dev.state.current_source];

    src->blend_enabled = enabled;
}

void m2d_blend_functions(enum m2d_blend_function color_func,
                         enum m2d_blend_function alpha_func)
{
    (void)color_func;
    (void)alpha_func;

    if (color_func != M2D_FUNC_ADD)
        LIBM2D_ERROR("unsupported blend function for color: %s\n",
                     m2d_blend_function_name(color_func));

    if (alpha_func != M2D_FUNC_ADD)
        LIBM2D_ERROR("unsupported blend function for alpha: %s\n",
                     m2d_blend_function_name(alpha_func));
}

void m2d_blend_factors(enum m2d_blend_factor src_color_factor,
                       enum m2d_blend_factor dst_color_factor,
                       enum m2d_blend_factor src_alpha_factor,
                       enum m2d_blend_factor dst_alpha_factor)
{
    struct gc_source* src = &dev.state.sources[dev.state.current_source];

    src->src_color_factor = src_color_factor;
    src->dst_color_factor = dst_color_factor;
    src->src_alpha_factor = src_alpha_factor;
    src->dst_alpha_factor = dst_alpha_factor;
}
