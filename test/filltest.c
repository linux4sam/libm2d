/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "m2d/m2d.h"
#include "m2d/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int fill(void* handle, struct m2d_buf* buf, uint32_t rgba,
		int x, int y, int w, int h, int pitch)
{
	struct m2d_surface src;
	int ret = 0;

	src.buf = buf;
	src.format = M2D_RGB16;
	src.pitch = pitch;
	src.x = x;
	src.y = y;
	src.width = w;
	src.height = h;
	src.dir = M2D_XY00;

	if (m2d_fill(handle, rgba, &src)) {
		ret = -1;
		goto abort;
	}

	if (m2d_flush(handle) != 0) {
		ret = -1;
		goto abort;
	}

	return ret;
abort:
	printf("fill error\n");
	return ret;
}

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

int main(int argc, char** argv)
{
	void* handle;
	struct m2d_buf* buf;
	uint32_t rgbai = 0;
	uint32_t rgba[] = {
		0x55ff0000,
		0x5500ff00,
		0x550000ff,
	};
	int pitch = m2d_format_pitch(M2D_RGB16, SCREEN_WIDTH);
	int count = 0;
	struct timespec start;

	srand(time(NULL));

	if (m2d_open(&handle) != 0) {
		return -1;
	}

	buf = m2d_alloc_from_name(handle, atoi(argv[1]));

	clock_gettime(CLOCK_MONOTONIC, &start);

	while (1) {
		int x = rand() % (SCREEN_WIDTH-50) + 0;
		int y = rand() % (SCREEN_HEIGHT-50) + 0;
		int w = 50;
		int h = 50;

		fill(handle, buf, rgba[rgbai++ % (sizeof(rgba)/sizeof(rgba[0]))],
		     x, y, w, h, pitch);

		if (++count == 1000) {
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

	m2d_free(buf);
	m2d_close(handle);

	return 0;
}
