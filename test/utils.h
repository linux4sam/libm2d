/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 * Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "m2d/m2d.h"

struct m2d_buf* load_png(const char* filename, void* handle);

void fps_start(void);
void fps_frame(void);

#endif
