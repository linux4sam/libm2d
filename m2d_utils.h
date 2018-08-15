/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __M2D_UTILS_H__
#define __M2D_UTILS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"  {
#endif

	int m2d_format_from_fourcc(uint32_t fourcc);

	int m2d_format_pitch(uint32_t format, uint32_t width);

#ifdef __cplusplus
}
#endif

#endif
