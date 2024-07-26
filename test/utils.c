/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "utils.h"
#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <time.h>

struct m2d_buffer* load_png(const char* filename)
{
	struct m2d_buffer* buf = NULL;
	cairo_t* cr;
	cairo_surface_t* target;
	cairo_surface_t* source;
	size_t width;
	size_t height;
	size_t stride;

	source = cairo_image_surface_create_from_png(filename);
	if (!source)
		goto end;

	width = cairo_image_surface_get_width(source);
	height = cairo_image_surface_get_height(source);
	stride = cairo_image_surface_get_stride(source);

	buf = m2d_alloc(width, height, M2D_PF_ARGB8888, stride);
	if (!buf)
		goto destroy_source;

	target = cairo_image_surface_create_for_data(m2d_get_data(buf),
						     CAIRO_FORMAT_ARGB32,
						     width, height,
						     stride);
	if (!target)
		goto free_buffer;

	cr = cairo_create(target);
	cairo_surface_destroy(target);
	if (!cr)
		goto free_buffer;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, source, 0, 0);

	cairo_paint(cr);

	cairo_surface_destroy(source);
	cairo_destroy(cr);

	return buf;

free_buffer:
	m2d_free(buf);
	buf = NULL;

destroy_source:
	cairo_surface_destroy(source);

end:
	return buf;
}

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
