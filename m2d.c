/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#include "m2d.h"
#include <assert.h>
#include <drm/atmel_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>
#include <errno.h>

#define FILL0_OPCODE 28
#define FILL0_REG 16
#define FILL0_DIR 8
#define FILL0_ARGS 0
#define FILL1_HEIGHT 16
#define FILL1_WIDTH 0
#define FILL2_DY0 16
#define FILL2_DX0 0
#define FILL3_ARGB 0

#define COPY0_OPCODE 28
#define COPY0_HWT 20
#define COPY0_REG 16
#define COPY0_DIR 8
#define COPY0_ARGS 0
#define COPY1_HEIGHT 16
#define COPY1_WIDTH 0
#define COPY2_DY0 16
#define COPY2_DX0 0
#define COPY3_SY0 16
#define COPY3_SX0 0

#define BLEND0_OPCODE 28
#define BLEND0_REG 16
#define BLEND0_DIR 8
#define BLEND0_ARGS 0
#define BLEND1_HEIGHT 16
#define BLEND1_WIDTH 0
#define BLEND2_DY0 16
#define BLEND2_DX0 0
#define BLEND3_SY0 16
#define BLEND3_SX0 0
#define BLEND4_SY1 16
#define BLEND4_SX1 0
#define BLEND5_SPE 12
#define BLEND5_FUNC 8
#define BLEND5_DFACT 4
#define BLEND5_SFACT 0

#define ROP0_OPCODE 28
#define ROP0_REG 16
#define ROP0_ARGS 0
#define ROP1_HEIGHT 16
#define ROP1_WIDTH 0
#define ROP2_DY0 16
#define ROP2_DX0 0
#define ROP3_SY0 16
#define ROP3_SX0 0
#define ROP4_SY1 16
#define ROP4_SX1 0
#define ROP5_PMASK 0
#define ROP6_ROPM 16
#define ROP6_ROPH 8
#define ROP6_ROPL 0

#define LDR0_OPCODE 28
#define LDR0_REG 16
#define LDR0_LDRCS 10
#define LDR0_LDRCID 9
#define LDR0_LDRC 8
#define LDR0_ARGS 0
#define LDR1_DATA 0

#define STR0_OPCODE 28
#define STR0_IE 24
#define STR0_REG 16
#define STR0_REGAD 12
#define STR0_ARGS 0

#define WFE0_OPCODE 28
#define WFE0_IE 24

enum reg_id
{
	GFX2D_PA0 = 0,
	GFX2D_PITCH0 = 1,
	GFX2D_CFG0 = 2,
	GFX2D_PA1 = 3,
	GFX2D_PITCH1 = 4,
	GFX2D_CFG1 = 5,
	GFX2D_PA2 = 6,
	GFX2D_PITCH2 = 7,
	GFX2D_CFG2 = 8,
	GFX2D_PA3 = 9,
	GPREG0 = 10,
	GPREG1 = 11,
	GPREG2 = 12,
	GPREG3 = 13,
	GPREG4 = 14,
	GPREG5 = 15,
};

struct device
{
	int fd;
};

static inline void word_write(uint32_t* word, uint32_t offset, uint32_t value)
{
	*word |= (value << offset);
}

static int gen_ldr_cmd(uint32_t* buf, enum reg_id reg, uint32_t value)
{
	int i = 0;
	word_write(buf + i, LDR0_OPCODE, 0x8);
	word_write(buf + i, LDR0_REG, reg);
	word_write(buf + i, STR0_ARGS, 0);
	++i;
	word_write(buf + i, LDR1_DATA, value);
	return ++i;
}

static int gen_str_cmd(uint32_t* buf, enum reg_id reg, uint32_t addr)
{
	int i = 0;
	word_write(buf + i, STR0_OPCODE, 0x9);
	word_write(buf + i, STR0_REG, reg);
	word_write(buf + i, STR0_REGAD, addr);
	word_write(buf + i, STR0_ARGS, 0);
	return ++i;
}

static int gen_wfe_cmd(uint32_t* buf)
{
	int i = 0;
	word_write(buf + i, WFE0_OPCODE, 0xA);
	return ++i;
}

static int gen_fill_cmd(uint32_t* buf, enum m2d_transfer_dir dir,uint32_t dwidth,
			uint32_t dheight, uint32_t dx, uint32_t dy,
			uint32_t argb)
{
	int i = 0;
	word_write(buf + i, FILL0_OPCODE, 0xB);
	word_write(buf + i, FILL0_DIR, dir);
	word_write(buf + i, FILL0_ARGS, 2);
	++i;
	word_write(buf + i, FILL1_WIDTH, dwidth-1);
	word_write(buf + i, FILL1_HEIGHT, dheight-1);
	++i;
	word_write(buf + i, FILL2_DY0, dy);
	word_write(buf + i, FILL2_DX0, dx);
	++i;
	word_write(buf + i, FILL3_ARGB, argb);
	return ++i;
}

static int gen_copy_cmd(uint32_t* buf, enum m2d_transfer_dir dir,uint32_t dwidth,
			uint32_t dheight, uint32_t dx, uint32_t dy,
			uint32_t sx, uint32_t sy)
{
	int i = 0;
	word_write(buf + i, COPY0_OPCODE, 0xC);
	word_write(buf + i, COPY0_HWT, 0);
	word_write(buf + i, COPY0_DIR, dir);
	word_write(buf + i, COPY0_ARGS, 2);
	++i;
	word_write(buf + i, COPY1_WIDTH, dwidth-1);
	word_write(buf + i, COPY1_HEIGHT, dheight-1);
	++i;
	word_write(buf + i, COPY2_DY0, dy);
	word_write(buf + i, COPY2_DX0, dx);
	++i;
	word_write(buf + i, COPY3_SY0, sy);
	word_write(buf + i, COPY3_SX0, sx);
	return ++i;
}

static int gen_blend_cmd(uint32_t* buf, enum m2d_transfer_dir dir,uint32_t dwidth,
			 uint32_t dheight, uint32_t dx, uint32_t dy,
			 uint32_t sx0, uint32_t sy0, uint32_t sx1, uint32_t sy1,
			 enum m2d_blend_func func,
			 enum m2d_blend_factors dfact,
			 enum m2d_blend_factors sfact)
{
	int i = 0;
	word_write(buf + i, BLEND0_OPCODE, 0xD);
	word_write(buf + i, BLEND0_DIR, dir);
	word_write(buf + i, BLEND0_ARGS, 4);
	++i;
	word_write(buf + i, BLEND1_WIDTH, dwidth-1);
	word_write(buf + i, BLEND1_HEIGHT, dheight-1);
	++i;
	word_write(buf + i, BLEND2_DY0, dy);
	word_write(buf + i, BLEND2_DX0, dx);
	++i;
	word_write(buf + i, BLEND3_SY0, sy0);
	word_write(buf + i, BLEND3_SX0, sx0);
	++i;
	word_write(buf + i, BLEND4_SY1, sy1);
	word_write(buf + i, BLEND4_SX1, sx1);
	++i;
	if ((func & 0xf) == M2D_BLEND_SPE)
		word_write(buf + i, BLEND5_SPE, func >> 4);
	word_write(buf + i, BLEND5_FUNC, func & 0xf);
	word_write(buf + i, BLEND5_DFACT, dfact);
	word_write(buf + i, BLEND5_SFACT, sfact);
	return ++i;
}

static int gen_rop_cmd(uint32_t* buf, enum m2d_transfer_dir dir,uint32_t dwidth,
		       uint32_t dheight, uint32_t dx, uint32_t dy,
		       uint32_t sx0, uint32_t sy0, uint32_t sx1, uint32_t sy1,
		       uint32_t pmask,
		       uint8_t ropl,
		       uint8_t roph,
		       enum m2d_rop_mode mode)
{
	int i = 0;
	word_write(buf + i, ROP0_OPCODE, 0xE);
	word_write(buf + i, ROP0_ARGS, 5);
	++i;
	word_write(buf + i, ROP1_WIDTH, dwidth-1);
	word_write(buf + i, ROP1_HEIGHT, dheight-1);
	++i;
	word_write(buf + i, ROP2_DY0, dy);
	word_write(buf + i, ROP2_DX0, dx);
	++i;
	word_write(buf + i, ROP3_SY0, sy0);
	word_write(buf + i, ROP3_SX0, sx0);
	++i;
	word_write(buf + i, ROP4_SY1, sy1);
	word_write(buf + i, ROP4_SX1, sx1);
	++i;
	word_write(buf + i, ROP5_PMASK, pmask);
	++i;
	word_write(buf + i, ROP6_ROPL, ropl);
	word_write(buf + i, ROP6_ROPH, roph);
	word_write(buf + i, ROP6_ROPM, mode);
	return ++i;
}

int m2d_open(void **handle)
{
	const char* device_file = "atmel-hlcdc";
	//const char* device_file = "/dev/dri/card0";
	struct device* h = calloc(1, sizeof(struct device));
	assert(h);

	h->fd = drmOpen(device_file, NULL);
	//h->fd = open(device_file, O_RDWR, 0);

	*handle = h;
	return 0;
}

void m2d_close(void *handle)
{
	struct device* h = handle;
	assert(h);
	m2d_flush(handle);
	close(h->fd);
	free(h);
}

int m2d_fill(void *handle, uint32_t argb, struct m2d_surface *dst)
{
	struct device* h = handle;
	struct drm_gfx2d_submit req;
	uint32_t data[32] = {0};
	uint32_t *buf = data;

	assert(h);

	buf += gen_ldr_cmd(buf, GFX2D_PA0, dst->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH0, dst->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG0, dst->format);
	buf += gen_fill_cmd(buf, dst->dir, dst->width, dst->height,
			    dst->x, dst->y, argb);
	req.size = (buf - data);
	req.buf = (__u32)data;

	return ioctl(h->fd, DRM_IOCTL_GFX2D_SUBMIT, &req, sizeof(req));
}

int m2d_copy(void *handle, struct m2d_surface* src, struct m2d_surface* dst)
{
	struct device* h = handle;
	struct drm_gfx2d_submit req;
	uint32_t data[32] = {0};
	uint32_t *buf = data;

	assert(h);

	buf += gen_ldr_cmd(buf, GFX2D_PA0, dst->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH0, dst->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG0, dst->format);
	buf += gen_ldr_cmd(buf, GFX2D_PA1, src->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH1, src->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG1, src->format);
	buf += gen_copy_cmd(buf, dst->dir, dst->width, dst->height,
			    dst->x, dst->y, src->x, src->y);
	req.size = (buf - data);
	req.buf = (__u32)data;

	return ioctl(h->fd, DRM_IOCTL_GFX2D_SUBMIT, &req, sizeof(req));
}

int m2d_blend(void *handle, struct m2d_surface* src0,
	      struct m2d_surface* src1,
	      struct m2d_surface* dst,
	      enum m2d_blend_func func)
{
	struct device* h = handle;
	struct drm_gfx2d_submit submit;
	uint32_t data[32] = {0};
	uint32_t *buf = data;

	assert(h);

	buf += gen_ldr_cmd(buf, GFX2D_PA0, dst->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH0, dst->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG0, dst->format);
	buf += gen_ldr_cmd(buf, GFX2D_PA1, src0->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH1, src0->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG1, src0->format);
	buf += gen_ldr_cmd(buf, GFX2D_PA2, src1->buf->paddr);
	buf += gen_ldr_cmd(buf, GFX2D_PITCH2, src1->pitch);
	buf += gen_ldr_cmd(buf, GFX2D_CFG2, src1->format);
	buf += gen_blend_cmd(buf, dst->dir, dst->width, dst->height,
			     dst->x, dst->y,
			     src0->x, src0->y,
			     src1->x, src1->y,
			     func,
			     dst->fact,
			     src0->fact);
	submit.size = (buf - data);
	submit.buf = (__u32)data;
	return ioctl(h->fd, DRM_IOCTL_GFX2D_SUBMIT, &submit, sizeof(submit));
}

int m2d_rop(void *handle, enum m2d_rop_mode mode, struct m2d_surface *sp[], int nsurfaces,
	    uint8_t ropl,
	    uint8_t roph)
{
	struct device* h = handle;
	struct drm_gfx2d_submit req;
	uint32_t data[128] = {0};
	uint32_t *buf = data;
	int x;

	assert(h);

	for (x = 0; x < nsurfaces;x++) {
		buf += gen_ldr_cmd(buf, GFX2D_PA0+(x*3), sp[x]->buf->paddr);
		buf += gen_ldr_cmd(buf, GFX2D_PITCH0+(x*3), sp[x]->pitch);
		buf += gen_ldr_cmd(buf, GFX2D_CFG0+(x*3), sp[x]->format);
	}

	struct m2d_surface *dst = sp[0];
	struct m2d_surface *src0 = sp[1];
	struct m2d_surface *src1 = sp[2];
	struct m2d_surface *src2 = sp[3];

	buf += gen_rop_cmd(buf, dst->dir, dst->width, dst->height,
			   dst->x, dst->y, src0->x, src0->y,
			   src1->x, src1->y, src2->buf->paddr,
			   ropl, roph, mode);
	req.size = (buf - data);
	req.buf = (__u32)data;

	return ioctl(h->fd, DRM_IOCTL_GFX2D_SUBMIT, &req, sizeof(req));
}

static void* map_gem(int fd, uint32_t name, uint32_t* size)
{
	struct drm_gem_open gem;
	struct drm_mode_map_dumb args;
	int err;
	void* ptr;

	memset(&gem, 0, sizeof(gem));
	gem.name = name;
	err = drmIoctl(fd, DRM_IOCTL_GEM_OPEN, &gem);
	if (err < 0) {
		printf("could not flink %s\n", strerror(-err));
		return NULL;
	}

	memset(&args, 0, sizeof(args));
	args.handle = gem.handle;

	err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &args);
	if (err < 0) {
		printf("could not map dumb %s\n", strerror(-errno));
		return NULL;
	}

	if (size)
		*size = gem.size;

	ptr = mmap(0, gem.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   fd, args.offset);
	if (ptr == MAP_FAILED) {
		printf("could not mmap dumb %s\n", strerror(-errno));
		return NULL;
	}

	return ptr;
}

struct m2d_buf *m2d_alloc_from_name(void* handle, uint32_t name)
{
	uint32_t size;
	struct device* h = handle;
	assert(h);

	void* ptr = map_gem(h->fd, name, &size);
	if (ptr) {

		struct drm_gfx2d_gem_addr req;
		req.name = name;
		if (ioctl(h->fd, DRM_IOCTL_GFX2D_GEM_ADDR, &req, sizeof(req)) == 0) {
			struct m2d_buf* buf = calloc(1, sizeof(struct m2d_buf));
			buf->name = name;
			buf->paddr = req.paddr;
			buf->size = size;
			buf->vaddr = ptr;

			printf("gem paddr: 0x%x\n", buf->paddr);

			return buf;
		}
	}

	return 0;
}

static void* create_dumb_buffer(int fd, int size, uint32_t* name)
{
	struct drm_mode_create_dumb args1;
	memset(&args1, 0, sizeof(args1));
	args1.width = size;
	args1.height = 1;
	args1.bpp = 8;

	int err = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &args1);
	if (err < 0) {
		printf("error: DRM_IOCTL_MODE_CREATE_DUMB\n");
		return NULL;
	}

	struct drm_mode_map_dumb args2;
	void *ptr;
	memset(&args2, 0, sizeof(args2));
	args2.handle = args1.handle;

	err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &args2);
	if (err < 0) {
		printf("error: DRM_IOCTL_MODE_MAP_DUMB\n");
		return NULL;
	}

	ptr = mmap(0, args1.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   fd, args2.offset);
	if (ptr == MAP_FAILED)
	{
		printf("error: mmap failed\n");
		return NULL;
	}

	struct drm_gem_flink flink;
	memset(&flink, 0, sizeof(flink));
	flink.handle = args1.handle;
	err = drmIoctl(fd, DRM_IOCTL_GEM_FLINK, &flink);
	if (err < 0) {
		printf("error: DRM_IOCTL_GEM_FLINK\n");
		return NULL;
	}

	printf("dumb buffer gem name: %d\n", flink.name);

	*name = flink.name;

	return ptr;
}

struct m2d_buf *m2d_alloc(void* handle, uint32_t size)
{
	struct device* h = handle;
	uint32_t name;
	void* ptr;

	assert(h);

	ptr = create_dumb_buffer(h->fd, size, &name);
	if (ptr) {
		struct drm_gfx2d_gem_addr req;
		req.name = name;

		if (ioctl(h->fd, DRM_IOCTL_GFX2D_GEM_ADDR, &req, sizeof(req)) == 0) {
			struct m2d_buf* buf = calloc(1, sizeof(struct m2d_buf));
			buf->name = name;
			buf->paddr = req.paddr;
			buf->vaddr = ptr;
			buf->size = size;
			return buf;
		} else {
			// TODO: free dumb buffer
		}
	}

	return 0;
}

void m2d_free(struct m2d_buf* buf)
{
	if (buf->vaddr)
		munmap(buf->vaddr, buf->size);

	free(buf);
}

int m2d_wfe(void *handle)
{
	struct device* h = handle;
	struct drm_gfx2d_submit submit;
	uint32_t data[32] = {0};
	uint32_t *buf = data;

	assert(h);
	buf += gen_wfe_cmd(buf);

	return ioctl(h->fd, DRM_IOCTL_GFX2D_SUBMIT, &submit, sizeof(submit));
}

int m2d_flush(void *handle)
{
	struct device* h = handle;
	assert(h);

	return ioctl(h->fd, DRM_IOCTL_GFX2D_FLUSH);
}

int m2d_fd(void* handle)
{
	struct device* h = handle;
	assert(h);

	return h->fd;
}
