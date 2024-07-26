![Microchip](docs/microchip_logo.png)

# Microchip 2D Graphics API

## Dependencies

- libdrm >= 2.4.0
- cairo >= 1.14.6 (conditional)

## Building

Make sure you have the required dependencis listed above.

    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
    ninja -C build -j $(nproc)

## License

libm2d is released under the terms of the `Apache 2` license. See the [COPYING](COPYING)
file for more information.
