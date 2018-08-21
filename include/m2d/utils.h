/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __M2D_UTILS_H__
#define __M2D_UTILS_H__
/**
 * @file utils.h
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C"  {
#endif

	/**
	 * Convert a fourcc format to an m2d format.
	 *
	 * @param fourcc The fourcc format.
	 * @return The m2d format or < 0 on error.
	 */
	int m2d_format_from_fourcc(uint32_t fourcc);

	/**
	 * Given a format and width, return the pitch of the buffer.
	 *
	 * @param format The m2d format.
	 * @param width The width of the buffer in number of pixels.
	 * @return The pitch or < 0 on error.
	 */
	int m2d_format_pitch(uint32_t format, uint32_t width);

#ifdef __cplusplus
}
#endif

#endif
