/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_CAIRO
#include <cairo.h>

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
#endif

static void timespec_diff(struct timespec *start, struct timespec *stop,
			  struct timespec *result)
{
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return;
}

static struct timespec start;
static unsigned int frames = 0;

void fps_start(void)
{
	clock_gettime(CLOCK_MONOTONIC, &start);
}

void fps_frame(void)
{
	struct timespec now;
	struct timespec diff;
	unsigned int ms;
	double sec;

	frames++;

	clock_gettime(CLOCK_MONOTONIC, &now);

	timespec_diff(&start, &now, &diff);

	ms = (diff.tv_sec) * 1000 + (diff.tv_nsec) / 1000000;
	sec = ms / 1000.;
	if (sec >= 1.0) {
		printf("%.2f fps\n", frames / sec);
		clock_gettime(CLOCK_MONOTONIC, &start);
		frames = 0;
	}
}
