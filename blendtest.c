/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "m2d.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int fourcc_to_m2d(uint32_t fourcc)
{
	switch (fourcc)
	{
		//case DRM_FORMAT_XRGB4444: return M2D_ARGB16;
	case DRM_FORMAT_ARGB4444: return M2D_ARGB16;
		//case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ARGB1555: return M2D_TRGB16;
	case DRM_FORMAT_RGB565: return M2D_RGB16;
	case DRM_FORMAT_RGB888: return M2D_RGB24;
		//case DRM_FORMAT_XRGB8888: return M2D_ARGB32;
	case DRM_FORMAT_ARGB8888: return M2D_ARGB32;
	case DRM_FORMAT_RGBA8888: return M2D_RGBA32;
	}

	printf("unsupported format: %d\n", fourcc);

	return 0;
}


static int format_pitch(uint32_t format, uint32_t width)
{
	switch (format)
	{
	case M2D_ARGB16:
	case M2D_RGB16:
	case M2D_RGBT16:
	case M2D_TRGB16:
		return width * 2;
	case M2D_ARGB32:
	case M2D_RGBA32:
		return width * 4;
	default:
		fprintf(stderr, "unsupported pitch format: %d\n", format);
		break;
	}

	return 0;
}

static struct m2d_buf* load_png(const char* filename, void* handle)
{
	cairo_t* cr;
	cairo_surface_t* surface;
	cairo_surface_t* image;

	printf("loading image...\n");
	image = cairo_image_surface_create_from_png(filename);
	assert(image);

	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);

	struct m2d_buf* src = m2d_alloc(handle, width*height*4);

	printf("creating surface %d,%d ...\n", width, height);

	cairo_format_t format = CAIRO_FORMAT_ARGB32;
	surface = cairo_image_surface_create_for_data((void*)src->vaddr,
						      format,
						      width, height,
						      cairo_format_stride_for_width(format, width));
	assert(surface);

	cr = cairo_create(surface);
	assert(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, image, 0, 0);

	cairo_paint(cr);

	cairo_surface_destroy(image);
	cairo_surface_destroy(surface);
	cairo_destroy(cr);

	return src;
}

static int blend(void* handle, struct m2d_buf* srca, struct m2d_buf* srcb, struct m2d_buf* dst, enum m2d_blend_func mode)
{
	struct m2d_surface src0;
	struct m2d_surface src1;
	struct m2d_surface dst0;
	int ret = 0;

	src0.buf = srca;
	src0.format = M2D_ARGB8888;
	src0.pitch = format_pitch(src0.format, 272);
	src0.x = 0;
	src0.y = 0;
	src0.width = 272;
	src0.height = 272;
	src0.dir = M2D_XY00;
	src0.fact = M2D_SRC_COLOR;

	src1.buf = srcb;
	src1.format = M2D_ARGB8888;
	src1.pitch = format_pitch(src1.format, 272);
	src1.x = 0;
	src1.y = 0;
	src1.width = 272;
	src1.height = 272;
	src1.dir = M2D_XY00;

	dst0.buf = dst;
	dst0.format = M2D_RGB16;
	dst0.pitch = format_pitch(dst0.format, 480);
	dst0.x = 0;
	dst0.y = 0;
	dst0.width = 272;
	dst0.height = 272;
	dst0.dir = M2D_XY00;
	dst0.fact = M2D_DST_COLOR;

	if (m2d_blend(handle,
		      &src0,
		      &src1,
		      &dst0,
		      mode))
	{
		ret = -1;
		goto abort;
	}

	if (m2d_flush(handle) != 0)
	{
		ret = -1;
		goto abort;
	}

	return ret;
abort:
	printf("blend error\n");
	return ret;
}

int main(int argc, char** argv)
{
	void* handle;
	int mode;

	srand(time(NULL));

	if (m2d_open(&handle) != 0)
	{
		return -1;
	}

	struct m2d_buf* dst = m2d_alloc_from_name(handle, atoi(argv[1]));
	assert(dst);

	struct m2d_buf* src0 = load_png("blend0.png", handle);
	assert(src0);

	struct m2d_buf* src1 = load_png("blend1.png", handle);
	assert(src1);

	while (1) {
		for (mode = M2D_BLEND_ADD; mode <= M2D_BLEND_MAX; mode++) {
			blend(handle, src0, src1, dst, mode);

			struct timespec ts = {2,0};
			nanosleep(&ts, NULL);
		}
	}

	m2d_free(src0);
	m2d_free(src1);
	m2d_free(dst);
	m2d_close(handle);

	return 0;
}
