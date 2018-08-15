/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#include "m2d.h"
#include "m2d_utils.h"
#include <drm_fourcc.h>
#include <stdio.h>

int m2d_format_from_fourcc(uint32_t fourcc)
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

	fprintf(stderr, "error: unsupported format: %d\n", fourcc);

	return 0;
}

int m2d_format_pitch(uint32_t format, uint32_t width)
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
