/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m2d_drm.h"
#include "m2d_priv.h"
#include <errno.h>
#include <string.h>
#include <xf86drm.h>

int m2d_drm_open(struct m2d_device* dev)
{
    drmVersionPtr version;

    dev->fd = drmOpenWithType(dev->name, NULL, DRM_NODE_RENDER);
    if (dev->fd < 0)
    {
        LIBM2D_ERROR("can't open DRM render node %s: %s\n", dev->name, strerror(errno));
	return -1;
    }

    (void)version;
#if LIBM2D_ACTIVE_LEVEL <= LIBM2D_LEVEL_DEBUG
    version = drmGetVersion(dev->fd);
    if (version)
    {
        LIBM2D_DEBUG("DRM Version %d.%d.%d\n",
                     version->version_major,
                     version->version_minor,
                     version->version_patchlevel);
        LIBM2D_DEBUG("  Name: %s\n", version->name);
        LIBM2D_DEBUG("  Date: %s\n", version->date);
        LIBM2D_DEBUG("  Description: %s\n", version->desc);
        drmFreeVersion(version);
    }
#endif

    return 0;
}

void m2d_drm_close(struct m2d_device* dev)
{
    if (drmClose(dev->fd))
        LIBM2D_ERROR("can't close DRM render node %s: %s\n", dev->name, strerror(errno));
}
