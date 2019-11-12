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

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

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
	dst.pitch = m2d_format_pitch(dst.format, SCREEN_WIDTH);
	dst.x = x;
	dst.y = y;
	dst.width = 50;
	dst.height = 50;
	dst.dir = M2D_XY00;

	if (m2d_copy(handle, &src, &dst)) {
		ret = -1;
		goto abort;
	}

	if (m2d_flush(handle) != 0) {
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
	struct m2d_buf* dst;
	struct m2d_buf* src;

	if (argc != 2) {
		printf("usage: %s GEM_NAME\n", argv[0]);
		return -1;
	}

	srand(time(NULL));

	if (m2d_open(&handle) != 0) {
		return -1;
	}

	dst = m2d_alloc_from_name(handle, atoi(argv[1]));
	assert(dst);

	src = load_png("copy.png", handle);
	assert(src);

	fps_start();

	while (1) {
		int x = rand() % (SCREEN_WIDTH-50) + 0;
		int y = rand() % (SCREEN_HEIGHT-50) + 0;

		copy(handle, src, dst, x, y);

		fps_frame();
	}

	m2d_free(handle, src);
	m2d_free(handle, dst);
	m2d_close(handle);

	return 0;
}
