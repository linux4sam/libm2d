/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __M2D_H__
#define __M2D_H__
/**
 * @file
 * @brief Microchip 2D API
 */

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
		/** 4-bit indexed color, with 4-bit alpha value */
		M2D_A4IDX4 = 0,
		/** 8 bits per pixel alpha, with user-defined constant color */
		M2D_A8 = 1,
		/** 8 bits indexed color, uses the Color Look-Up Table to expand to true color */
		M2D_IDX8 = 2,
		/** 8-bit indexed color, with 8-bit alpha value */
		M2D_A8IDX8 = 3,
		/** 12 bits per pixel, 4 bits per color channel */
		M2D_RGB12 = 4,
		/** 16 bits per pixel with 4-bit width alpha value, and 4 bits per color channel */
		M2D_ARGB16 = 5,
		/** 15 bits per pixel, 5 bits per color channel */
		M2D_RGB15 = 6,
		/** 16 bits per pixel, 5 bits for the red and blue channels and 6 bits for the green channel */
		M2D_TRGB16 = 7,
		/** 16 bits per pixel, with 1 bit for transparency and 5 bits for color channels */
		M2D_RGBT16 = 8,
		/** 16 bits per pixel, 5 bits for the red and blue channels and 6 bits for the green channel */
		M2D_RGB16 = 9,
		/** 24 bits per pixel, 8 bits for alpha and color channels */
		M2D_RGB24 = 10,
		/** 32 bits per pixel, 8 bits for alpha and color channels */
		M2D_ARGB32 = 11,
		/** 32 bits per pixel, 8 bits for alpha and color channels */
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
		/** @f[ (0,0,0,0) @f] */
		M2D_ZERO = 0,
		/** @f[ (1,1,1,1) @f] */
		M2D_ONE = 1,
		/** @f[ (A_s,R_s,G_s,B_s) @f] */
		M2D_SRC_COLOR = 2,
		/** @f[ (1,1,1,1) - (A_s,R_s,G_s,B_s) @f] */
		M2D_ONE_MINUS_SRC_COLOR = 3,
		/** @f[ (A_d,R_d,G_d,B_d) @f] */
		M2D_DST_COLOR = 4,
		/** @f[ (1,1,1,1)-(A_d,R_d,G_d,B_d) @f] */
		M2D_ONE_MINUS_DST_COLOR = 5,
		/** @f[ (A_s,A_s,A_s,A_s) @f] */
		M2D_SRC_ALPHA = 6,
		/** @f[ (1,1,1,1)-(A_s,A_s,A_s,A_s) @f] */
		M2D_ONE_MINUS_SRC_ALPHA = 7,
		/** @f[ (A_d,A_d,A_d,A_d) @f] */
		M2D_DST_ALPHA = 8,
		/** @f[ (1,1,1,1)-(A_d,A_d,A_d,A_d) @f] */
		M2D_ONE_MINUS_DST_ALPHA = 9,
		/** @f[ (A_c,R_c,G_c,B_c) @f] */
		M2D_CONSTANT_COLOR = 10,
		/** @f[ (1,1,1,1) - (A_c,R_c,G_c,B_c) @f] */
		M2D_ONE_MINUS_CONSTANT_COLOR = 11,
		/** @f[ (A_c,A_c,A_c,A_c) @f] */
		M2D_CONSTANT_ALPHA = 12,
		/** @f[ (1,1,1,1)-(A_c,A_c,A_c,A_c) @f] */
		M2D_ONE_MINUS_CONSTANT_ALPHA = 13,
		/** @f[ (1,i,i,i) @f] where i is equal to the minimum between @f[ A_s @f] and @f[ 1-A_d @f] */
		M2D_SRC_ALPHA_SATURATE = 14,
	};

	/**
	 * Raster operation modes.
	 */
	enum m2d_rop_mode
	{

		/** ROP2 mode */
		ROP_MODE_ROP2 = 0,
		/** ROP3 mode */
		ROP_MODE_ROP3 = 1,
		/** ROP4 mode */
		ROP_MODE_ROP4 = 2,
	};

	/**
	 * Blend functions used by m2d_blend().
	 */
	enum m2d_blend_func
	{
		/** @f[ C_f = S*C_s + D*C_d @f] */
		M2D_BLEND_ADD = 0,
		/** @f[ C_f = S*C_s - D*C_d @f] */
		M2D_BLEND_SUBTRACT = 1,
		/** @f[ C_f = D*C_d - S*C_s @f] */
		M2D_BLEND_REVERSE = 2,
		/** @f[ C_f = min(C_s,C_d) @f] */
		M2D_BLEND_MIN = 3,
		/** @f[ C_f = max(C_s,C_d) @f] */
		M2D_BLEND_MAX = 4,
		/** Special Blending Functions */
		M2D_BLEND_SPE = 5,

		/** @f[ max(src1, src2) @f] */
		M2D_BLEND_SPE_LIGHTEN = M2D_BLEND_SPE | (1<<4),
                /** @f[ min(src1, src2) @f] */
		M2D_BLEND_SPE_DARKEN = M2D_BLEND_SPE | (1<<5),
		/** @f[ (src1 * src2) / 255 @f] */
		M2D_BLEND_SPE_MULTIPLY = M2D_BLEND_SPE | (1<<6),
		/** @f[ (src1 + src2) / 2 @f] */
		M2D_BLEND_SPE_AVERAGE = M2D_BLEND_SPE | (1<<7),
		/** @f[ src1 + src2 (saturated) @f] */
		M2D_BLEND_SPE_ADD = M2D_BLEND_SPE | (1<<8),
		/** @f[ src1 + src2 - 255 (saturated) @f] */
		M2D_BLEND_SPE_SUBTRACT = M2D_BLEND_SPE | (1<<9),
		/** @f[ abs(src1 - src2) @f] */
		M2D_BLEND_SPE_DIFFERENCE = M2D_BLEND_SPE | (1<<10),
		/** @f[ 255 - abs(255 - src1 - src2) @f] */
		M2D_BLEND_SPE_NEGATION = M2D_BLEND_SPE | (1<<11),
		/** @f[ 255 - (((255 - src1) * (255 - src2)) / 256) @f] */
		M2D_BLEND_SPE_SCREEN = M2D_BLEND_SPE | (1<<12),
		/** @f[ (src2 < 128) ? (2 * src1 * src2 / 255) : (255 - 2 * (255 - src1) * (255 - src2) / 255) @f] */
		M2D_BLEND_SPE_OVERLAY = M2D_BLEND_SPE | (1<<13),
		/** @f[ (src2 == 255) ? src2 : min(255, ((src1 << 8) / (255 - src2)) @f] */
		M2D_BLEND_SPE_DODGE = M2D_BLEND_SPE | (1<<14),
		/** @f[ (src2 == 0) ? src2 : max(0, (255 - ((255 - src1) << 8 ) / src2)))) @f] */
		M2D_BLEND_SPE_BURN = M2D_BLEND_SPE | (1<<15),
		/** @f[ (src2 == 255) ? src2 : min(255, (src1 * src1 / (255 - src2))) @f] */
		M2D_BLEND_SPE_REFLECT = M2D_BLEND_SPE | (1<<16),
		/** @f[ (src1 == 255) ? src1 : min(255, (src2 * src2 / (255 - src1))) @f] */
		M2D_BLEND_SPE_GLOW = M2D_BLEND_SPE | (1<<17),
	};

	/**
	 * Transfer direction selection.
	 */
	enum m2d_transfer_dir
	{
		/** Horizontal forward, vertical forward */
		M2D_XY00 = 0,
		/** Horizontal forward, vertical backward */
		M2D_XY01 = 1,
		/** Horizontal backward, vertical forward */
		M2D_XY10 = 2,
		/** Horizontal backward, vertical backward */
		M2D_XY11 = 3,
	};

	/**
	 * A surface operation definition.
	 */
	struct m2d_surface
	{
		/** Format for the surface. */
		enum m2d_format format;
		/** Buffer for the surface. */
		struct m2d_buf* buf;
		/** Pitch of the surface */
		int pitch;

		/**
		 * X coordinate of the operation, not to be confused with the
		 * X coordinate of the surface.
		 */
		int x;
		/**
		 * Y coordinate of the operation, not to be confused with the
		 * Y coordinate of the surface.
		 */
		int y;
		/**
		 * Width of the operation, not to be confused with the
		 * width of the surface.
		 */
		int width;
		/**
		 * Height of the operation, not to be confused with the
		 * height of the surface.
		 */
		int height;

		/**  Direction for the transfer. */
		enum m2d_transfer_dir dir;
		/** When blending, the blending factor for this surface (either dst or src). */
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
        /** gem-handle needed to destroy the buffer. */
        uint32_t gem_handle;
	};

	/**
	 * Open the device and get a handle.
	 *
	 * @param handle Pointer to handle to the allocated m2d instance.
	 * @return 0 on success, non-zero on error.
	 */
	int m2d_open(void** handle);

	/**
	 * Close the device and free any resources.
	 *
	 * @param handle Handle to the m2d instance.
	 */
	void m2d_close(void* handle);

	/**
	 * Allocate a new surface buffer given the specified size.
	 *
	 * @param handle Handle to the m2d instance.
	 * @param size Size in bytes.
	 * @return NULL on error.
	 */
	struct m2d_buf* m2d_alloc(void* handle, uint32_t size);

	/**
	 * Allocate a new surface buffer with a pre-existing GEM name.
	 *
	 * @param handle Handle to the m2d instance.
	 * @param name GEM object name.
	 * @return NULL on error.
	 */
	struct m2d_buf* m2d_alloc_from_name(void* handle, uint32_t name);

	/**
	 * Allocate a new surface buffer with a pre-existing virtual address.
	 *
	 * @param handle Handle to the m2d instance.
	 * @param virt Virtual address.
	 * @return NULL on error.
	 */
	struct m2d_buf* m2d_alloc_from_virt(void* handle, void* virt, uint32_t size);

	/**
	 * Free an allocated surface buffer.
	 */
	void m2d_free(void* handle, struct m2d_buf* buf);

	/**
	 * Wait for vsync from LCD controller.
	 *
	 * @param handle Handle to the m2d instance.
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_wfe(void* handle);

	/**
	 * Fill a destination surface with the given ARGB value.
	 *
	 * @param handle Handle to the m2d instance.
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_fill(void* handle, uint32_t argb, struct m2d_surface* dst);

	/**
	 * Copy a source surface to a destination surface.
	 *
	 * @param handle Handle to the m2d instance.
	 * @return 0 on success, non-zero on error.
	 * @note You must call m2d_flush() to submit any pending requests.
	 */
	int m2d_copy(void* handle, struct m2d_surface* src,
		     struct m2d_surface* dst);

	/**
	 * Blend 2 source surfaces into a destination surface.
	 *
	 * @param handle Handle to the m2d instance.
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
	 * @param handle Handle to the m2d instance.
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
	 * @param handle Handle to the m2d instance.
	 * @return 0 on success, non-zero on error.
	 * @note This may block if the internal buffer is full.
	 */
	int m2d_flush(void* handle);

	/**
	 * Get the internal DRI file descriptor.
	 *
	 * @param handle Handle to the m2d instance.
	 * @return < 0 on error, otherwise the file descriptor.
	 */
	int m2d_fd(void* handle);


#ifdef __cplusplus
}
#endif

#endif
