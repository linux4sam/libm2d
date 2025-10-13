/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef M2D_DRM_H
#define M2D_DRM_H

struct m2d_device;

int m2d_drm_open(struct m2d_device* dev);
void m2d_drm_close(struct m2d_device* dev);

#endif /* M2D_DRM_H */
