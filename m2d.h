#ifndef __M2D_H__
#define __M2D_H__

#include <stdint.h>

#define M2D_VERSION_MAJOR   0
#define M2D_VERSION_MINOR   1

#ifdef __cplusplus
extern "C"  {
#endif

	enum m2d_format
	{
		M2D_RGB444 = 0,
		M2D_ARGB4444 = 1,
		M2D_RGB5551 = 2,
		M2D_RGB555 = 3,
		M2D_ARGB8888 = 4,
		M2D_RGBA8888 = 5,
	};

	enum m2d_blend_factors
	{
		M2D_ZERO                  = 0,
		M2D_ONE                   = 1,
		M2D_SRC_COLOR = 2,
		M2D_ONE_MINUS_SRC_COLOR   = 3,
		M2D_DST_COLOR             = 4,
		M2D_ONE_MINUS_DST_COLOR   = 5,
		M2D_SRC_ALPHA   = 6,
		M2D_ONE_MINUS_SRC_ALPHA   = 7,
		M2D_DST_ALPHA   = 8,
		M2D_ONE_MINUS_DST_ALPHA   = 9,
		M2D_CONSTANT_COLOR   = 10,
		M2D_ONE_MINUS_CONSTANT_COLOR   = 11,
		M2D_CONSTANT_ALPHA   = 12,
		M2D_ONE_MINUS_CONSTANT_ALPHA   = 13,
		M2D_SRC_ALPHA_SATURATE   = 14,
	};

	enum m2d_rop_mode
	{
		ROP_MODE_ROP2 = 0,
		ROP_MODE_ROP3 = 1,
		ROP_MODE_ROP4 = 2,
	};

	enum m2d_blend_func
	{
		M2D_ADD = 0,
		M2D_SUBTRACT = 1,
		M2D_REVERSE = 2,
		M2D_MIN = 3,
		M2D_MAX = 4,
		M2D_SPE = 5,

		// Special Blend w/M2D_SPE
		M2D_SPE_LIGHTEN = M2D_SPE | (1<<4),
		M2D_SPE_DARKEN = M2D_SPE | (1<<5),
		M2D_SPE_MULTIPLY = M2D_SPE | (1<<6),
		M2D_SPE_AVERAGE = M2D_SPE | (1<<7),
		M2D_SPE_ADD = M2D_SPE | (1<<8),
		M2D_SPE_SUBTRACT = M2D_SPE | (1<<9),
		M2D_SPE_DIFFERENCE = M2D_SPE | (1<<10),
		M2D_SPE_NEGOTIATION = M2D_SPE | (1<<11),
		M2D_SPE_SCREEN = M2D_SPE | (1<<12),
		M2D_SPE_OVERLAY = M2D_SPE | (1<<13),
		M2D_SPE_DODGE = M2D_SPE | (1<<14),
		M2D_SPE_BURN = M2D_SPE | (1<<15),
		M2D_SPE_REFLECT = M2D_SPE | (1<<16),
		M2D_SPE_GLOW = M2D_SPE | (1<<17),
	};

	enum m2d_transfer_dir
	{
		M2D_XY00 = 0,
		M2D_XY01 = 1,
		M2D_XY10 = 2,
		M2D_XY11 = 3,
	};

	struct m2d_surface
	{
		enum m2d_format format;
		enum m2d_transfer_dir dir;

		struct m2d_buf* buf;

		int x;
		int y;
		int pitch;
		int width;
		int height;

		enum m2d_blend_func func;
		enum m2d_blend_factors fact;

		int global_alpha;
	};

	struct m2d_buf
	{
		void *handle;
		void *vaddr;
		int paddr;
		int size;
	};

	int m2d_open(void **handle);
	int m2d_close(void *handle);
	int m2d_flush(void *handle);

	int m2d_fill(void* handle, uint32_t argb, struct m2d_surface *dst);
	int m2d_copy(void* handle, struct m2d_surface* src, struct m2d_surface* dst);
	int m2d_blend(void *handle, struct m2d_surface* src0,
		      struct m2d_surface* src1,
		      struct m2d_surface* dst);
	//int m2d_rop(void *handle, enum m2d_rop_mode, struct m2d_surface *sp[], int surfaces)

	struct m2d_buf *m2d_alloc(int size);
	struct m2d_buf *m2d_buf_from_virt_addr(void *vaddr, int size);
	int m2d_free(struct m2d_buf *buf);

#ifdef __cplusplus
}
#endif

#endif
