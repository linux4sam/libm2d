/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <m2d/m2d.h>

struct m2d_buffer* load_png(const char* filename);

void fps_start(void);
void fps_frame(void);

#endif
