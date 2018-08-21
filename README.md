![Microchip](docs/microchip_logo.png)

# Microchip 2D Graphics API

## Dependencies

- libdrm >= 2.4.0
- cairo >= 1.14.6

## Building

Make sure you have the required dependencis listed above.  To cross compile, put
the cross gcc path in your environment PATH variable and use the appropriate
prefix for --host on the configure line.  For example:

    ./autogen.sh
    ./configure --host=arm-buildroot-linux-gnueabihf
    make

If you wish to statically link the applications, add the following to your
configure line:

    ./configure --enable-static --disable-shared LDFLAGS="-static"


## API Documentation

If you have doxygen installed, you can generate the API documentation by running:

    make docs

The resulting documentation will be in the docs directory.

## License

libm2d is released under the terms of the `MIT` license. See the `COPYING`
file for more information.
