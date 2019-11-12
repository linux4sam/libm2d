/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "m2d/m2d.h"
#include "m2d/utils.h"
#include "utils.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void parse_color(uint32_t in, uint8_t* r, uint8_t* g, uint8_t* b,  uint8_t* a)
{
	*a = (in >> 24) & 0xff;
	*r = (in >> 16) & 0xff;
	*g = (in >> 8) & 0xff;
	*b = in & 0xff;
}

static void genmask(unsigned char* dst, unsigned char* src, int w, int h)
{
	int y;
	int x;
	uint32_t* src0 = (uint32_t*)src;
	uint32_t* dst0 = (uint32_t*)dst;

	memset(dst, 0, w * h / 8);

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
			uint32_t offset = (w*y) + x;
			parse_color(*(src0 + offset), &r, &g, &b, &a);

			if (a)
			{
				uint32_t word = offset / 32;
				uint32_t bit = offset % 32;
				dst0[word] |= (1<<bit);
			}
		}
	}
}

static struct m2d_buf* load_png_as_mask(const char* filename, void* handle)
{
	cairo_surface_t* image;
	cairo_format_t format = CAIRO_FORMAT_ARGB32;
	int width;
	int height;
	int stride;
	struct m2d_buf* src;

	printf("loading image...\n");
	image = cairo_image_surface_create_from_png(filename);
	assert(image);

	width = cairo_image_surface_get_width(image);
	height = cairo_image_surface_get_height(image);
	stride = cairo_format_stride_for_width(format, width);

	src = m2d_alloc(handle, stride * height);

	printf("creating mask surface %d,%d ...\n", width, height);

	genmask(src->vaddr, cairo_image_surface_get_data(image),
		width, height);
	cairo_surface_destroy(image);

	return src;
}

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

static int rop(void* handle,
	       struct m2d_buf* srca,
	       struct m2d_buf* srcb,
	       struct m2d_buf* srcc,
	       struct m2d_buf* dst)
{
	struct m2d_surface src0;
	struct m2d_surface src1;
	struct m2d_surface src2;
	struct m2d_surface dst0;
	int ret = 0;

	src0.buf = srca;
	src0.format = M2D_ARGB8888;
	//src0.format = M2D_RGB16;
	src0.pitch = m2d_format_pitch(src0.format, 272);
	src0.x = 0;
	src0.y = 0;
	src0.width = 272;
	src0.height = 272;
	src0.dir = M2D_XY00;

	src1.buf = srcb;
	src1.format = M2D_ARGB8888;
	//src1.format = M2D_RGB16;
	src1.pitch = m2d_format_pitch(src1.format, 272);
	src1.x = 0;
	src1.y = 0;
	src1.width = 272;
	src1.height = 272;
	src1.dir = M2D_XY00;

	src2.buf = srcc;
	src2.format = M2D_ARGB8888;
	//src2.format = M2D_RGB16;
	src2.pitch = m2d_format_pitch(src2.format, 272);
	src2.x = 0;
	src2.y = 0;
	src2.width = 272;
	src2.height = 272;
	src2.dir = M2D_XY00;

	dst0.buf = dst;
	dst0.format = M2D_RGB16;
	dst0.pitch = m2d_format_pitch(dst0.format, SCREEN_WIDTH);
	dst0.x = 0;
	dst0.y = 0;
	dst0.width = 272;
	dst0.height = 272;
	dst0.dir = M2D_XY00;

	for (int x = 0; x <= 0xff; x++) {
		struct m2d_surface* sp[] = { &dst0, &src0, &src1, &src2 };
		struct timespec ts = {1, 0};

		if (m2d_rop(handle, ROP_MODE_ROP4, sp, 4, x, 0)) {
			//if (m2d_rop(handle, ROP_MODE_ROP3, sp, 3, x, 0)) {
			//if (m2d_rop(handle, ROP_MODE_ROP2, sp, 2, x, 0)) {
			ret = -1;
			goto abort;
		}

		if (m2d_flush(handle) != 0) {
			ret = -1;
			goto abort;
		}

		nanosleep(&ts, NULL);
	}

	return ret;
abort:
	printf("rop error\n");
	return ret;
}

int main(int argc, char** argv)
{
	void* handle;
	struct m2d_buf* dst;
	struct m2d_buf* src0;
	struct m2d_buf* src1;
	struct m2d_buf* src2;

	if (m2d_open(&handle) != 0) {
		return -1;
	}

	dst = m2d_alloc_from_name(handle, atoi(argv[1]));
	assert(dst);

	src0 = load_png("rop0.png", handle);
	assert(src0);

	src1 = load_png("rop1.png", handle);
	assert(src1);

	src2 = load_png_as_mask("rop2.png", handle);
	assert(src2);

	rop(handle, src0, src1, src2, dst);

	m2d_free(handle, src0);
	m2d_free(handle, src1);
	m2d_free(handle, src2);
	m2d_free(handle, dst);
	m2d_close(handle);

	return 0;
}
