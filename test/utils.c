/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "utils.h"
#include <cairo.h>
#include <stdio.h>
#include <assert.h>

struct m2d_buf* load_png(const char* filename, void* handle)
{
	cairo_t* cr;
	cairo_surface_t* surface;
	cairo_surface_t* image;
	int width;
	int height;
	struct m2d_buf* src;
	cairo_format_t format = CAIRO_FORMAT_ARGB32;

	printf("loading image...\n");
	image = cairo_image_surface_create_from_png(filename);
	assert(image);

	width = cairo_image_surface_get_width(image);
	height = cairo_image_surface_get_height(image);

	src = m2d_alloc(handle, width*height*4);

	printf("creating surface %d,%d ...\n", width, height);

	surface = cairo_image_surface_create_for_data(src->vaddr,
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
