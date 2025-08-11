/*
 * Copyright (C) 2024 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "utils.h"
#include <drm_fourcc.h>
#include <getopt.h>
#include <m2d/m2d.h>
#include <planes/kms.h>
#include <planes/plane.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

struct m2d_test
{
    const char* name;
    void (*func)(void);
};

static struct m2d_buffer* framebuffer;
static size_t screen_width;
static size_t screen_height;

static size_t stride(enum m2d_pixel_format format, size_t width)
{
    size_t bits_per_pixel;

    switch (format)
    {
    case M2D_PF_ARGB8888:
        bits_per_pixel = 32;
        break;

    case M2D_PF_RGB565:
        bits_per_pixel = 16;
        break;

    case M2D_PF_A8:
        bits_per_pixel = 8;
        break;

    default:
        bits_per_pixel = 0;
        break;
    }

    /*
     * The least multiple of sizeof(uint32_t) so data are 4-byte aligned:
     * pixman, hence cairo, requires such an alignment.
     */
    return ((width * bits_per_pixel + 0x1f) >> 5) * sizeof(uint32_t);
}

static void fill_background(uint8_t red, uint8_t green, uint8_t blue)
{
    struct m2d_rectangle rect;

    m2d_source_enable(M2D_SRC, false);
    m2d_source_enable(M2D_DST, false);
    m2d_blend_enable(false);

    m2d_source_color(red, green, blue, 255);

    memset(&rect, 0, sizeof(rect));
    rect.w = screen_width;
    rect.h = screen_height;
    m2d_draw_rectangles(&rect, 1);

    m2d_source_color(255, 255, 255, 255);
}

static void draw_background(struct m2d_buffer* bg)
{
    struct m2d_rectangle rect;

    m2d_source_enable(M2D_SRC, true);
    m2d_source_enable(M2D_DST, false);
    m2d_blend_enable(false);

    m2d_set_source(M2D_SRC, bg, 0, 0);
    rect.x = 0;
    rect.y = 0;
    rect.w = screen_width;
    rect.h = screen_height;
    m2d_draw_rectangles(&rect, 1);
}

static void fill(void)
{
    fill_background(0, 0, 0);
    usleep(250000);
    fill_background(255, 0, 0);
    usleep(250000);
    fill_background(0, 255, 0);
    usleep(250000);
    fill_background(0, 0, 255);
    usleep(250000);
    fill_background(255, 255, 255);
    usleep(250000);
}

static void draw_rectangles(void)
{
    struct m2d_rectangle rects[10];
    struct m2d_rectangle* rect;
    uint32_t i;

    fill_background(0, 0, 0); /* black */

    memset(rects, 0, sizeof(rects));

    m2d_source_enable(M2D_SRC, false);
    m2d_source_enable(M2D_DST, false);
    m2d_blend_enable(false);

    m2d_source_color(rand() & 255, rand() & 255, rand() & 255, 255);
    for (i = 0; i < ARRAY_SIZE(rects); i++)
    {
        rect = &rects[i];
        rect->w = 50;
        rect->h = 50;
        rect->x = rand() % (screen_width - rect->w);
        rect->y = rand() % (screen_height - rect->h);
    }
    m2d_draw_rectangles(rects, ARRAY_SIZE(rects));

    sleep(1);

    rect = &rects[0];
    for (i = 0; i < 100; i++)
    {
        size_t sizes[] = {50, 100, 150};

        m2d_source_color(rand() & 255, rand() & 255, rand() & 255, 255);
        rect->w = sizes[rand() % ARRAY_SIZE(sizes)];
        rect->h = sizes[rand() % ARRAY_SIZE(sizes)];
        rect->x = rand() % (screen_width - rect->w);
        rect->y = rand() % (screen_height - rect->h);
        m2d_draw_rectangles(rects, 1);
        usleep(100000);
    }

    m2d_source_color(255, 255, 255, 255);

    sleep(1);
}

static void draw_images(void)
{
    struct m2d_buffer* bg;
    struct m2d_buffer* bg2;
    struct m2d_rectangle rect;
    char filename[256];
    int x, y;

    snprintf(filename, sizeof(filename), "%s/background_%ux%u.png",
             TESTDATA, screen_width, screen_height);
    bg = load_png(filename);
    if (!bg)
        return;

    snprintf(filename, sizeof(filename), "%s/background2_%ux%u.png",
             TESTDATA, screen_width, screen_height);
    bg2 = load_png(filename);
    if (!bg2)
        goto free_bg;

    draw_background(bg);

    sleep(3);

    m2d_set_source(M2D_SRC, bg2, 0, 0);
    rect.w = 100;
    rect.h = 100;
    for (y = 0; y < (int)screen_height; y += rect.h)
    {
        for (x = 0; x < (int)screen_width; x += rect.w)
        {
            rect.x = x;
            rect.y = y;
            m2d_draw_rectangles(&rect, 1);
            usleep(100000);
        }
    }

    sleep(1);

    m2d_free(bg2);
free_bg:
    m2d_free(bg);
}

static void blend_images(void)
{
    struct m2d_buffer* bg;
    struct m2d_buffer* on;
    struct m2d_buffer* off;
    struct m2d_buffer* up;
    struct m2d_buffer* down;
    struct m2d_rectangle rect;
    char filename[256];

    snprintf(filename, sizeof(filename), "%s/background2_%ux%u.png",
             TESTDATA, screen_width, screen_height);
    bg = load_png(filename);
    if (!bg)
        return;

    snprintf(filename, sizeof(filename), "%s/on.png", TESTDATA);
    on = load_png(filename);
    if (!on)
        goto free_bg;

    snprintf(filename, sizeof(filename), "%s/off.png", TESTDATA);
    off = load_png(filename);
    if (!off)
        goto free_on;

    snprintf(filename, sizeof(filename), "%s/up.png", TESTDATA);
    up = load_png(filename);
    if (!up)
        goto free_off;

    snprintf(filename, sizeof(filename), "%s/down.png", TESTDATA);
    down = load_png(filename);
    if (!down)
        goto free_up;

    draw_background(bg);

    sleep(3);

    m2d_set_source(M2D_DST, framebuffer, 0, 0);
    m2d_source_enable(M2D_DST, true);

    m2d_blend_enable(true);
    m2d_blend_functions(M2D_FUNC_ADD, M2D_FUNC_ADD);
    m2d_blend_factors(M2D_BLEND_SRC_ALPHA, M2D_BLEND_ONE_MINUS_SRC_ALPHA,
                      M2D_BLEND_ONE, M2D_BLEND_ONE_MINUS_SRC_ALPHA);

    rect.w = 75;
    rect.h = 75;
    rect.x = 10;
    rect.y = 10;
    m2d_set_source(M2D_SRC, up, rect.x, rect.y);
    m2d_draw_rectangles(&rect, 1);

    rect.x = screen_width - 10 - rect.w;
    m2d_set_source(M2D_SRC, down, rect.x, rect.y);
    m2d_draw_rectangles(&rect, 1);

    rect.w = 100;
    rect.h = 100;
    rect.x = 10;
    rect.y = screen_height - 10 - rect.h;
    m2d_set_source(M2D_SRC, on, rect.x, rect.y);
    m2d_draw_rectangles(&rect, 1);

    rect.x = screen_width - 10 - rect.w;
    m2d_set_source(M2D_SRC, off, rect.x, rect.y);
    m2d_draw_rectangles(&rect, 1);

    sleep(1);

    m2d_free(down);
free_up:
    m2d_free(up);
free_off:
    m2d_free(off);
free_on:
    m2d_free(on);
free_bg:
    m2d_free(bg);
}

static const struct m2d_test tests[] =
{
    { "Fill", fill },
    { "DrawRectangles", draw_rectangles },
    { "DrawImages", draw_images },
    { "BlendImages", blend_images },
    { NULL, NULL}
};

static void list_tests(void)
{
    const struct m2d_test* test;

    fprintf(stderr, "\nTests:\n");
    for (test = tests; test->name; test++)
        fprintf(stderr, "- %s\n", test->name);
    fprintf(stderr, "\n");
}

static void help(const char* program)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS]\n"
            "\n"
            "OPTIONS:\n"
            "  -h, --help        Display this message.\n"
            "\n"
            "  -a, --autoplay      Automatic progression.\n"
            "  -d, --delay <sec>   When 'autoplay' option is set, delay in seconds before moving to the next test.\n"
            "  -m, --mdelay <usec> When 'autoplay' option is set, delay in milliseconds before moving to the next test.\n"
            "  -M, --manual        Manual progression: hit a key to move to the next test.\n"
            "\n"
            "  -l, --list          List tests\n"
            "  -f, --filter <name> Execute only the <name> test\n",
            program);
}

static int autoplay = -1;
static unsigned long udelay = 0;
static int manual = -1;
static const struct m2d_test* single_test = NULL;

static void parse_args(int argc, char* argv[])
{
    static const struct option long_options[] =
    {
        {"help", no_argument, NULL, 'h'},

        {"autoplay", no_argument, NULL, 'a'},
        {"delay", required_argument, NULL, 'd'},
        {"mdelay", required_argument, NULL, 'm'},
        {"manual", no_argument, NULL, 'M'},
        {"list", no_argument, NULL, 'l'},
        {"filter", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };
    const char* test_name = NULL;

    while (1)
    {
        int option_index = 0;
        unsigned long value;
        char* end;
        int c;

        c = getopt_long(argc, argv, "had:m:Mlf:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            help(argv[0]);
            exit(EXIT_SUCCESS);
            //break;

        case 'a':
            autoplay = 1;
            break;

        case 'M':
            manual = 1;
            break;

        case 'd':
            end = NULL;
            value = strtoul(optarg, &end, 0);
            if (!end || *end != '\0')
            {
                fprintf(stderr, "invalid value for delay: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            udelay = value * 1000000UL;
            break;

        case 'm':
            end = NULL;
            value = strtoul(optarg, &end, 0);
            if (!end || *end != '\0')
            {
                fprintf(stderr, "invalid value for mdelay: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            udelay = value * 1000UL;
            break;

        case 'l':
            list_tests();
            exit(EXIT_SUCCESS);
            //break;

        case 'f':
            test_name = optarg;
            break;

        default:
            help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (autoplay < 0 && manual < 0)
    {
        autoplay = 1;
        manual = 0;
    }

    if (autoplay > 0 && manual > 0)
    {
        fprintf(stderr, "'autoplay' and 'manual' options are exclusive\n");
        exit(EXIT_FAILURE);
    }

    if (autoplay && !udelay)
    {
        udelay = 500000UL;
    }

    if (test_name)
    {
        const struct m2d_test* test;

        for (test = tests; test->name; test++)
        {
            if (!strcmp(test->name, test_name))
            {
                single_test = test;
                break;
            }
        }

        if (!single_test)
        {
            fprintf(stderr, "unknown test: %s\n", test_name);
            exit(EXIT_FAILURE);
        }
    }
}

static void next(void)
{
    if (autoplay > 0)
    {
        usleep(udelay);
    }
    else
    {
        int c;

        fprintf(stderr, "\npress ENTER to continue\n");
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

int main(int argc, char* argv[])
{
    struct kms_device* device = NULL;
    struct plane_data* plane = NULL;
    struct m2d_import_desc desc;
    int ret = EXIT_FAILURE;
    int fd;

    parse_args(argc, argv);

    srand(time(NULL));

    if (m2d_init() < 0)
        goto end;

    fd = drmOpen("atmel-hlcdc", 0);
    if (fd < 0)
        goto cleanup_m2d;

    device = kms_device_open(fd);
    if (!device)
        goto close_drm;

    screen_width = device->screens[0]->width;
    screen_height = device->screens[0]->height;

    plane = plane_create(device, DRM_PLANE_TYPE_PRIMARY, 0,
                         device->screens[0]->width,
                         device->screens[0]->height,
                         DRM_FORMAT_RGB565);
    if (!plane)
        goto close_kms;

    /* plane will be unmapped by plane_free(). */
    if (plane_fb_map(plane) || plane_fb_export(plane))
        goto release_plane;

    plane_apply(plane);

    memset(&desc, 0, sizeof(desc));
    desc.width = device->screens[0]->width;
    desc.height = device->screens[0]->height;
    desc.format = M2D_PF_RGB565;
    desc.stride = stride(desc.format, desc.width);
    desc.fd = plane->prime_fds[0];
    desc.cpu_addr = plane->bufs[0];
    framebuffer = m2d_import(&desc);
    if (!framebuffer)
        goto release_plane;

    /* Default settings. */
    m2d_set_target(framebuffer);
    m2d_blend_functions(M2D_FUNC_ADD, M2D_FUNC_ADD);
    m2d_blend_factors(M2D_BLEND_SRC_ALPHA, M2D_BLEND_ONE_MINUS_SRC_ALPHA,
                      M2D_BLEND_ONE, M2D_BLEND_ONE_MINUS_SRC_ALPHA);

    if (single_test)
    {
        single_test->func();
        next();
    }
    else
    {
        const struct m2d_test* test;

        for (test = tests; test->func; test++)
        {
            test->func();
            next();
        }
    }

    ret = EXIT_SUCCESS;

//free_framebuffer:
    m2d_free(framebuffer);

release_plane:
    plane_free(plane);

close_kms:
    kms_device_close(device);

close_drm:
    drmClose(fd);

cleanup_m2d:
    m2d_cleanup();

end:
    return ret;
}
