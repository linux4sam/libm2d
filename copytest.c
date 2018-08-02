/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */

#include "m2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cairo.h>
#include <assert.h>
#include <xf86drm.h>
#include <string.h>
#include <sys/mman.h>
#include <drm_fourcc.h>

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

void* dumb_buffer(int fd, int width, int height, uint32_t* name)
{
	struct drm_mode_create_dumb args1;
	memset(&args1, 0, sizeof(args1));
	args1.width = width;
	args1.height = height; //TODO: this needs to be virtual based on format
	args1.bpp = 32;

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

	printf("fumb buffer gem name: %d\n", flink.name);

	*name = flink.name;

	return ptr;
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

static int load_png(const char* filename, void* ptr)
{
	cairo_t* cr;
	cairo_surface_t* surface;
	cairo_surface_t* image;

	printf("loading image...\n");
	image = cairo_image_surface_create_from_png(filename);
	assert(image);

	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);

	printf("creating surface %d,%d ...\n", width, height);

	cairo_format_t format = CAIRO_FORMAT_ARGB32;
	surface = cairo_image_surface_create_for_data(ptr,
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

	return 0;
}

static int copy(void* handle, struct m2d_buf* srcb, struct m2d_buf* dstb,
		int x, int y)
{
	struct m2d_surface src;
	struct m2d_surface dst;
	int ret = 0;

	src.buf = srcb;
	src.format = M2D_ARGB8888;
	src.pitch = format_pitch(src.format, 100);
	src.x = 0;
	src.y = 0;
	src.width = 100;
	src.height = 100;
	src.dir = M2D_XY00;

	dst.buf = dstb;
	dst.format = M2D_RGB16;
	dst.pitch = format_pitch(dst.format, 480);
	dst.x = x;
	dst.y = y;
	dst.width = 100;
	dst.height = 100;
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

	uint32_t name;
	void* ptr = dumb_buffer(m2d_fd(handle), 100, 100, &name);

	struct m2d_buf* src = m2d_alloc_from_name(handle, name);

//	struct m2d_buf* src = m2d_alloc(handle, 4 * 100 * 100);
	assert(src);

	load_png("image.png", (void*)ptr);
#if 0
	printf("filling 0x%x\n", ptr);
	uint32_t* addr = (uint32_t*)ptr/*src->vaddr*/;
	for (int x = 0; x < 100 * 100;x++)
		     *(addr+x) = 0xffff0000;
	printf("filling done\n");
#endif

	int count = 0;
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	while (1)
	{
		int x = rand() % (480-100) + 0;
		int y = rand() % (272-100) + 0;

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
