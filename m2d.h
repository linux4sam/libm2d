/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __M2D_H__
#define __M2D_H__

#include <stdint.h>

#define M2D_VERSION_MAJOR   1
#define M2D_VERSION_MINOR   0

#ifdef __cplusplus
extern "C"  {
#endif

	/**
	 * Supported surface formats.
	 */
	enum m2d_format
	{
		M2D_A4IDX4 = 0,
		M2D_A8 = 1,
		M2D_IDX8 = 2,
		M2D_A8IDX8 = 3,
		M2D_RGB12 = 4,
		M2D_ARGB16 = 5,
		M2D_RGB15 = 6,
		M2D_TRGB16 = 7,
		M2D_RGBT16 = 8,
		M2D_RGB16 = 9,
		M2D_RGB24 = 10,
		M2D_ARGB32 = 11,
		M2D_RGBA32 = 12,

		/* format aliases */
		M2D_RGB5551 = M2D_RGBT16,
		M2D_RGB1555 = M2D_TRGB16,
		M2D_ARGB8888 = M2D_ARGB32,
		M2D_RGBA8888 = M2D_RGBA32,
	};

	/**
	 * Source and destination blend factors used by m2d_blend().
	 */
	enum m2d_blend_factors
	{
		M2D_ZERO = 0,
		M2D_ONE = 1,
		M2D_SRC_COLOR = 2,
		M2D_ONE_MINUS_SRC_COLOR = 3,
		M2D_DST_COLOR = 4,
		M2D_ONE_MINUS_DST_COLOR = 5,
		M2D_SRC_ALPHA = 6,
		M2D_ONE_MINUS_SRC_ALPHA = 7,
		M2D_DST_ALPHA = 8,
		M2D_ONE_MINUS_DST_ALPHA = 9,
		M2D_CONSTANT_COLOR = 10,
		M2D_ONE_MINUS_CONSTANT_COLOR = 11,
		M2D_CONSTANT_ALPHA = 12,
		M2D_ONE_MINUS_CONSTANT_ALPHA = 13,
		M2D_SRC_ALPHA_SATURATE = 14,
	};

	/**
	 * Raster operation modes.
	 */
	enum m2d_rop_mode
	{
		ROP_MODE_ROP2 = 0,
		ROP_MODE_ROP3 = 1,
		ROP_MODE_ROP4 = 2,
	};

	/**
	 * Blend functions used by m2d_blend().
	 */
	enum m2d_blend_func
	{
		M2D_BLEND_ADD = 0,
		M2D_BLEND_SUBTRACT = 1,
		M2D_BLEND_REVERSE = 2,
		M2D_BLEND_MIN = 3,
		M2D_BLEND_MAX = 4,
		M2D_BLEND_SPE = 5,

		/* Special Blend w/M2D_BLEND_SPE */
		M2D_BLEND_SPE_LIGHTEN = M2D_BLEND_SPE | (1<<4),
		M2D_BLEND_SPE_DARKEN = M2D_BLEND_SPE | (1<<5),
		M2D_BLEND_SPE_MULTIPLY = M2D_BLEND_SPE | (1<<6),
		M2D_BLEND_SPE_AVERAGE = M2D_BLEND_SPE | (1<<7),
		M2D_BLEND_SPE_ADD = M2D_BLEND_SPE | (1<<8),
		M2D_BLEND_SPE_SUBTRACT = M2D_BLEND_SPE | (1<<9),
		M2D_BLEND_SPE_DIFFERENCE = M2D_BLEND_SPE | (1<<10),
		M2D_BLEND_SPE_NEGOTIATION = M2D_BLEND_SPE | (1<<11),
		M2D_BLEND_SPE_SCREEN = M2D_BLEND_SPE | (1<<12),
		M2D_BLEND_SPE_OVERLAY = M2D_BLEND_SPE | (1<<13),
		M2D_BLEND_SPE_DODGE = M2D_BLEND_SPE | (1<<14),
		M2D_BLEND_SPE_BURN = M2D_BLEND_SPE | (1<<15),
		M2D_BLEND_SPE_REFLECT = M2D_BLEND_SPE | (1<<16),
		M2D_BLEND_SPE_GLOW = M2D_BLEND_SPE | (1<<17),
	};

	enum m2d_transfer_dir
	{
		M2D_XY00 = 0,
		M2D_XY01 = 1,
		M2D_XY10 = 2,
		M2D_XY11 = 3,
	};

	/**
	 * A surface operation definition.
	 */
	struct m2d_surface
	{
		enum m2d_format format;
		struct m2d_buf* buf;
		int pitch;

		int x;
		int y;
		int width;
		int height;

		enum m2d_transfer_dir dir;
		enum m2d_blend_factors fact;
	};

	/**
	 * A buffer, which can be assigned to a surface.
	 */
	struct m2d_buf
	{
		/** Size of the buffer. */
		uint32_t size;
		/** GEM name. */
		uint32_t name;
		/** Physical address of the buffer. */
		uint32_t paddr;
		/** Virtual, usually mmap()'ed address of the buffer. */
		void* vaddr;
	};

	/**
	 * Open the device and get a handle.
	 * @return 0 on success, non-zero on error.
	 */
	int m2d_open(void** handle);

	/**
	 * Close the device and free any resources.
	 */
	void m2d_close(void* handle);

	/**
	 * Allocate a new surface buffer given the specified size.
	 * @param size Size in bytes.
	 * @return NULL on error.
	 */
	struct m2d_buf* m2d_alloc(void* handle, uint32_t size);

	/**
	 * Allocate a new surface buffer with a pre-existing GEM name.
	 * @param name GEM object name.
	 */
	struct m2d_buf* m2d_alloc_from_name(void* handle, uint32_t name);

	/**
	 * Free an allocated surface buffer.
	 */
	void m2d_free(struct m2d_buf* buf);

	/**
	 * Wait for vsync from LCD controller.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_wfe(void* handle);

	/**
	 * Fill a destination surface with the given ARGB value.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_fill(void* handle, uint32_t argb, struct m2d_surface* dst);

	/**
	 * Copy a source surface to a destination surface.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_copy(void* handle, struct m2d_surface* src,
		     struct m2d_surface* dst);

	/**
	 * Blend 2 source surfaces into a destination surface.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_blend(void* handle, struct m2d_surface* src0,
		      struct m2d_surface* src1,
		      struct m2d_surface* dst,
		      enum m2d_blend_func func);

	/**
	 * Raster operation from 1 to 3 source surfaces, into a destination
	 * surface.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_rop(void* handle, enum m2d_rop_mode mode,
		    struct m2d_surface* sp[], int nsurfaces,
		    uint8_t ropl, uint8_t roph);

	/**
	 * Flush any pending requests to the GPU.
	 *
	 * Submitted requests are always buffered.  This causes the GPU to act
	 * on any pending request.
	 *
	 * @return 0 on success, non-zero on error.
	 * @note This may block if the internal buffer is full.
	 */
	int m2d_flush(void* handle);

	/**
	 * Get the internal DRI file descriptor.
	 *
	 * @return < 0 on error, otherwise the file descriptor.
	 */
	int m2d_fd(void* handle);

#ifdef __cplusplus
}
#endif

#endif
