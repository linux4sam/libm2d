/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __M2D_H__
#define __M2D_H__
/**
 * @file
 * @brief Microchip 2D API
 */

#include <m2d/version.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define M2D_VERSION_MAJOR   1
#define M2D_VERSION_MINOR   0

#ifdef __cplusplus
extern "C"  {
#endif

typedef int dim_t;

enum m2d_pixel_format
{
    M2D_PF_ARGB8888,
    M2D_PF_RGB565,
    M2D_PF_A8,
};

/**
 * Convert an 'enum m2d_pixel_format' into a string.
 *
 * @param[in] format The pixel format.
 * @return the string representation of the pixel format.
 */
const char* m2d_format_name(enum m2d_pixel_format format);

struct m2d_buffer;

/**
 * Create an handle to the GPU.
 */
int m2d_init();

/**
 * Close
 */
void m2d_cleanup();

/**
 * struct m2d_capabilities - Describe the hardware capabilities and requirements
 */
struct m2d_capabilities
{
    /**
     * @stride_alignment
     *
     * Define to the number of bytes required as the alignment for stride values:
     * 1 means no requirement.
     */
    uint32_t stride_alignment;

    /**
     * @max_sources
     *
     * Define the maximum number of sources per rectangle supported by the GPU
     * hardware.
     */
    uint32_t max_sources;

    /**
     * @has_dst
     * Tell whether the GPU hardware makes the difference between the target
     * and destination surfaces and manage the destination surface as a source.
     */
    bool dst_is_source;

    /**
     * @draw_lines
     *
     * Tell whether the GPU hardware can draw lines.
     */
    bool draw_lines;

    /**
     * @stretched_blit
     *
     * Tell whether the GPU hardware can stretch or shrink source surfaces.
     */
    bool stretched_blit;
};

/**
 * Get the GPU capabilities.
 */
const struct m2d_capabilities* m2d_get_capabilities();

/**
 * Allocate a new DRM GEM object to share a memory region between the userspace application and the GPU.
 *
 * @param[in] width The width in pixel of the memory region to allocate.
 * @param[in] height The height in pixel of the memory region to allocate.
 * @param[in] pixel_format The pixel format of the memory region to allocate.
 * @param[in] stride The requested size in bytes between two consecutive rows in the memory region.
 * @return a pointer to a 'struct m2d_buffer' that represents the allocated memory region.
 *
 * @note The actual stride value of the returned 'struct m2d_buffer*' may be
 *       different from @stride, hence should be retreived with @m2d_get_stride().
 */
struct m2d_buffer* m2d_alloc(size_t width, size_t height, enum m2d_pixel_format format, size_t stride);

/**
 * width: The number of pixels per row.
 * height: The number of pixels per columns (also the number of rows).
 * format: How pixels are encoded in memory.
 * stride: The size in bytes between two consecutive rows in memory.
 * fd: The DRM PRIME file descriptor for the DRM GEM object to import.
 * cpu_addr: The virtual address in the userspace process memory map for the
 *           DRM GEM object.
 */
struct m2d_import_desc {
	size_t width;
	size_t height;
	enum m2d_pixel_format format;
	size_t stride;

	int fd;
	void* cpu_addr; /* imported DRM GEM object can't be mmap'ed. */
};

/**
 * Import a DRM GEM object from a DRM PRIME file descriptor.
 *
 * Typically, the DRM PRIME file descriptor is obtained from the atmel_hlcdc
 * driver to export a DRM GEM object used as a frame buffer.
 *
 * @param[in] desc The description of the DRM GEM object to import.
 * @return a pointer to a 'struct m2d_buffer' that represents the allocated
 *         memory region.
 */
struct m2d_buffer* m2d_import(const struct m2d_import_desc* desc);

/**
 * Release a memory region created with either @m2d_alloc() or @m2d_import().
 *
 * @param[in] buf The memory region to release.
 */
void m2d_free(struct m2d_buffer* buf);

/**
 * Make the CPU claim the ownership of the DRM GEM object associated with @buf.
 *
 * @param[in] buf A pointer to a 'struct m2d_buffer'.
 * @param[in] timeout A pointer to a 'const struct timespec'.
 * @return 0 if successfull, -1 otherwise.
 */
int m2d_sync_for_cpu(struct m2d_buffer* buf, const struct timespec* timeout);

/**
 * Make the GPU claim the ownership of the DRM GEM object associated with @buf.
 *
 * @param[in] buf A pointer to a 'struct m2d_buffer'.
 */
void m2d_sync_for_gpu(struct m2d_buffer* buf);

/**
 * Get the virtual address in the userspace process memory map for the DRM GEM
 * object associated with @buf.
 *
 * @param[in] buf A pointer to a 'struct m2d_buffer'.
 * @return the virtual address for @buf and its associated DRM GEM object.
 */
void* m2d_get_data(struct m2d_buffer* buf);

/**
 * Get the stride value for @buf.
 *
 * @param[in] buf A pointer to a 'struct m2d_buffer'.
 * @return the number of bytes for @buf stride.
 */
size_t m2d_get_stride(const struct m2d_buffer* buf);

/**
 * Wait for all queued operations/commands involving @buf to complete.
 *
 * @param[in] buf A pointer to a 'const struct m2d_buffer'.
 * @param[in] timeout A pointer to a 'const struct timespec'
 * @return 0 if successfull, -1 otherwise.
 */
int m2d_wait(const struct m2d_buffer* buf, const struct timespec* timeout);

/**
 * Set the target surface in the current renderer state.
 *
 * @param[in] buf A pointer to the 'struct m2d_buffer' to be used as the GPU
 * target surface. The target surface is where the GPU draws; it is the result
 * of the GPU operation. Hence, it must not be NULL.
 */
void m2d_set_target(struct m2d_buffer* buf);

/**
 * Identifiers for source surfaces.
 *
 * - M2D_SRC: the only source for a copy operation the source surface for
 *            blending or ROP.
 * - M2D_DST: the destination surface for blending or ROP.
 * - M2D_MSK: the mask for ROP.
 */
enum m2d_source_id
{
    M2D_SRC,
    M2D_DST,
    M2D_MSK,

    M2D_MAX_SOURCES
};

const char* m2d_source_name(enum m2d_source_id);

/**
 * Set the source surface @index in the current renderer state.
 *
 * @param[in] id The id of the source surface to set.
 * @param[in] buf A pointer to the 'struct m2d_buffer' where pixels are read from
 * @param[in] x The origin x coordinate of this source surface in the target
 *              surface space coordinate.
 * @param[in] y The origin y coordiante of this source surface in the target
 *              surface space coordinate.
 *
 * @note Pixels from the source surface @index are read from the @buf
 *              'struct m2d_buffer' as if the origin of the source surface
 *              were positioned at point (@x, @y) in the target surface space.
 */
void m2d_set_source(enum m2d_source_id id, struct m2d_buffer* buf, dim_t x, dim_t y);

/**
 * Enable/disable the source surface @index in the current renderer state.
 *
 * @param[in] id The id of the source surface to enable/disable.
 * @param[in] enabled The boolean telling where input pixels are read from:
 *            - true: one or some 'struct m2d_buffer' set by @m2d_set_source();
 *                    The actual number of sources depends on the operation,
 *                    hence on @m2d_blend_enable() for instance.
 *             or
 *            - false: the constant source color set by @m2d_set_source_color()
 */
void m2d_source_enable(enum m2d_source_id id, bool enabled);

/**
 * Set the constant source color in the current renderer state.
 * red == green == blue == alpha == 255 disables the pre-multiplication.
 *
 * For GFX2D:
 * FILL operation: fill the destination surface with the constant source color.
 * BLEND operation: pre-multiply the source surface with the constant source color, if not {255, 255, 255, 255}.
 * COPY operation: unused.
 *
 * @param[in] red The red component of the constant source color.
 * @param[in] green The green component of the constant source color.
 * @param[in] blue The blue component of the constant source color.
 * @param[in] alpha The alpha component of the constant source color.
 */
void m2d_source_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

/**
 * Set the constant blend color in the current renderer state.
 *
 * @param[in] red The red component of the constant blend color.
 * @param[in] green The green component of the constant blend color.
 * @param[in] blue The blue component of the constant blend color.
 * @param[in] alpha The alpha component of the constant blend color.
 */
void m2d_blend_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

/**
 * Enable/disable blending mode in the current renderer state.
 *
 * @param[in] enabled The boolean telling whether the blending mode should be either enabled (true) or disabled (false).
 */
void m2d_blend_enable(bool enabled);

/**
 * Specify the equation for the rgb and alpha components.
 * s: the source factor ('src_rgb_factor' for rgb components, 'src_alpha_factor' for the alpha component) see @m2d_blend_factors().
 * d: the destination factor ('dst_rgb_factor' for rgb components, 'dst_alpha_factor' for the alpha component) see @m2d_blend_factors().
 * S: the source color component (red, green, blue or alpha component).
 * D: the destination color component (red, green, blue or alpha component).
 *
 * M2D_FUNC_ADD:                O = s * S + d * D
 * M2D_FUNC_SUBSTRACT:          O = s * S - d * D
 * M2D_FUNC_REVERSE_SUBTRACT:   O = d * D - s * S
 * M2D_FUNC_MIN:                O = min(S, D)
 * M2D_FUNC_MAX:                O = max(S, D)
 */
enum m2d_blend_function {
	M2D_FUNC_ADD,
	M2D_FUNC_SUBTRACT,
	M2D_FUNC_REVERSE,
	M2D_FUNC_MIN,
	M2D_FUNC_MAX,
};

/**
 * Convert an 'enum m2d_blend_function' into a string.
 *
 * @paran[in] factor The blend function.
 * @return the string representation of the blend function.
 */
const char* m2d_blend_function_name(enum m2d_blend_function function);

/**
 * Change the blend functions in the current renderer state.
 *
 * @param[in] rgb_func The function for rgb components.
 * @param[in] alpha_func The function for the alpha component (ignored for GFX2D: use the same as rgb_func).
 */
void m2d_blend_functions(enum m2d_blend_function rgb_func, enum m2d_blend_function alpha_func);

enum m2d_blend_factor {
	M2D_BLEND_ZERO,
	M2D_BLEND_ONE,
	M2D_BLEND_SRC_COLOR,
	M2D_BLEND_ONE_MINUS_SRC_COLOR,
	M2D_BLEND_DST_COLOR,
	M2D_BLEND_ONE_MINUS_DST_COLOR,
	M2D_BLEND_SRC_ALPHA,
	M2D_BLEND_ONE_MINUS_SRC_ALPHA,
	M2D_BLEND_DST_ALPHA,
	M2D_BLEND_ONE_MINUS_DST_ALPHA,
	M2D_BLEND_CONSTANT_COLOR,
	M2D_BLEND_ONE_MINUS_CONSTANT_COLOR,
	M2D_BLEND_CONSTANT_ALPHA,
	M2D_BLEND_ONE_MINUS_CONSTANT_ALPHA,
	M2D_BLEND_SRC_ALPHA_SATURATE,
};

/**
 * Convert an 'enum m2d_blend_factor' into a string.
 *
 * @paran[in] factor The blend factor.
 * @return the string representation of the blend factor.
 */
const char* m2d_blend_factor_name(enum m2d_blend_factor factor);

/**
 * Change the blend factors in the current renderer state.
 *
 * @param[in] src_rgb_factor
 * @param[in] dst_rgb_factor
 * @param[in] src_alpha_factor
 * @param[in] dst_alpha_factor
 *
 * @note factors are ignored when either M2D_FUNC_MIN or M2D_FUNC_MAX function
 *       is selected by @m2d_blend_functions().
 */
void m2d_blend_factors(enum m2d_blend_factor src_rgb_factor,
		       enum m2d_blend_factor dst_rgb_factor,
		       enum m2d_blend_factor src_alpha_factor,
		       enum m2d_blend_factor dst_alpha_factor);

/**
 * The rectangle definition for @m2d_draw_rectangles().
 *
 * x: the x coordinates of the rectangle origin in the target surface space.
 * y: the y coordinates of the rectangle origin in the target surface space.
 * w: the width in pixel of the rectangle in the target surface space.
 * h: the height in pixel of the rectangle in the target surface space.
 */
struct m2d_rectangle {
	dim_t x;
	dim_t y;
	dim_t w;
	dim_t h;
};

/**
 * Draw rectangles according to the current renderer state.
 * This is asynchronous (non-blocking).
 *
 * For instance, with GFX2D,
 * {@m2d_source_enable() , @m2d_blend_enable()}:
 * {false , false} : FILL operation with constant source color.
 * {false , true}  : Not supported.
 * {true , false}  : COPY operation.
 * {true , true}   : BLEND operation.
 *
 * @param[in] rects The array of rectangles to draw.
 * @param[in] num_rects The number of rectangles in the 'rects' array.
 */
void m2d_draw_rectangles(const struct m2d_rectangle* rects, size_t num_rects);


/* LINES OPERATIONS ARE NOT SUPPORTED BY THE GFX2D */

/**
 * The line definition for @m2d_draw_lines().
 *
 * A line between point {start_x, start_y} and point {end_x, end_y}.
 */
struct m2d_line {
	dim_t start_x;
	dim_t start_y;
	dim_t end_x;
	dim_t end_y;
};

/**
 * Set the line width for @m2d_draw_lines() in the current renderer state.
 *
 * @param[in] width The line width in pixels.
 */
void m2d_line_width(dim_t width);

/**
 * Draw lines according to the current renderer state.
 * This is asynchronous (non-blocking).
 *
 * The line color is set by @m2d_source_color().
 * The line width is set by @m2d_line_width().
 *
 * @param[in] lines The array of lines to draw.
 * @param[in] num_lines The numbers of lines in the 'lines' array.
 */
void m2d_draw_lines(const struct m2d_line* lines, size_t num_lines);

#ifdef __cplusplus
}
#endif

#endif
