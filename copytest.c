/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "m2d.h"
#include "m2d_utils.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int copy(void* handle, struct m2d_buf* srcb, struct m2d_buf* dstb,
		int x, int y)
{
	struct m2d_surface src;
	struct m2d_surface dst;
	int ret = 0;

	src.buf = srcb;
	src.format = M2D_ARGB8888;
	src.pitch = m2d_format_pitch(src.format, 50);
	src.x = 0;
	src.y = 0;
	src.width = 50;
	src.height = 50;
	src.dir = M2D_XY00;

	dst.buf = dstb;
	dst.format = M2D_RGB16;
	dst.pitch = m2d_format_pitch(dst.format, 480);
	dst.x = x;
	dst.y = y;
	dst.width = 50;
	dst.height = 50;
	dst.dir = M2D_XY00;

	if (m2d_copy(handle, &src, &dst))
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
	printf("copy error\n");
	return ret;
}

int main(int argc, char** argv)
{
	void* handle;

	srand(time(NULL));

	if (m2d_open(&handle) != 0)
	{
		return -1;
	}

	struct m2d_buf* dst = m2d_alloc_from_name(handle, atoi(argv[1]));
	assert(dst);

	struct m2d_buf* src = load_png("copy.png", handle);
	assert(src);

	int count = 0;
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	while (1)
	{
		int x = rand() % (480-50) + 0;
		int y = rand() % (272-50) + 0;

		copy(handle, src, dst, x, y);

		if (++count == 1000)
		{
			struct timespec end;
			clock_gettime(CLOCK_MONOTONIC, &end);

			if (end.tv_nsec < start.tv_nsec) {
				end.tv_nsec += 1000000000;
				end.tv_sec--;
			}

			printf("%ld.%09ld\n", (long)(end.tv_sec - start.tv_sec),
			       end.tv_nsec - start.tv_nsec);

			count = 0;
			clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		}
	}

	m2d_free(src);
	m2d_free(dst);
	m2d_close(handle);

	return 0;
}
