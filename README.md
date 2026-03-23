# Multi 2D Graphics

## Overview

**Multi 2D Graphics** or simply libm2d is a free and open-source library for GNU Linux, which provides a common API to abstract 2D GPUs.
This API is inspired from OpenGL but adapted to 2D GPUs.

## Supported GPUs

Currently libm2d supports the following 2D GPUs:
  - Microchip GFX2D
  - Vivante GC520UL (with either nano2D or etnaviv driver)

## Dependencies

Based on the choice of the GPU libm2d is compiled for, the list of dependencies varies.

### Microchip GFX2D

  - libdrm >= 2.4.0

### Vivante GC520UL with nano2D driver

  - nano2D >= 2.0.41

### Vivante GC520UL with etnaviv driver

  - libdrm >= 2.4.0
  - libdrm_etnaviv >= 2.4.0

Optionally, if tests are enabled then extra dependencies are added to the list:

  - cairo >= 1.14.6

## Building

This project uses cmake. Only the `GPU` option is mandatory to select the GPU and driver libm2d is built for:
  - `microchip,sam9x60-gfx2d` for the Microchip GFX2D embeded in SAM9X60 SoCs
  - `microchip,sam9x7-gfx2d` for the Microchip GFX2D embedded in SAM9X75 SoCs
  - `vivante,gc-nano2d` for the Vivante GC520UL with nano2D driver
  - `vivante,gc-etnaviv` for the Vivante GC520UL with etnaviv driver

The `ENABLE_TESTS` option triggers the compilation of libm2d test applications like `m2d_test`.

For instance, for the Microchip GFX2D GPU embedded in SAM9X75 SoCs with m2d_test:

```shell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DGPU=microchip,sam9x7-gfx2d -DENABLE_TESTS=ON
ninja -C build -j $(nproc)
```

## License

libm2d is released under the terms of the `Apache 2` license. See the [COPYING](COPYING)
file for more information.
