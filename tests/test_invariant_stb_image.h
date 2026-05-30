#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <signal.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
/* Limit memory usage to prevent runaway allocations */
#define STBI_MALLOC(sz)    malloc(sz)
#define STBI_REALLOC(p,sz) realloc(p,sz)
#define STBI_FREE(p)       free(p)

#include "stb_image.h"

/* Signal handler state for catching crashes */
static volatile sig_atomic_t crash_detected = 0;
static sigjmp_buf crash_jmp;

static void crash_handler(int sig) {
    (void)sig;
    crash_detected = 1;
    siglongjmp(crash_jmp, 1);
}

/* Helper: attempt to load image from raw bytes, assert no crash and
   that if loading succeeds the returned dimensions are sane */
static void try_load_image(const unsigned char *data, size_t len, const char *label) {
    int w = 0, h = 0, channels = 0;
    unsigned char *img = NULL;

    /* Install signal handlers to catch memory corruption crashes */
    struct sigaction sa_segv, sa_bus, sa_abrt, old_segv, old_bus, old_abrt;
    memset(&sa_segv, 0, sizeof(sa_segv));
    sa_segv.sa_handler = crash_handler;
    sigemptyset(&sa_segv.sa_mask);
    sa_segv.sa_flags = SA_RESETHAND;

    sa_bus = sa_segv;
    sa_abrt = sa_segv;

    sigaction(SIGSEGV, &sa_segv, &old_segv);
    sigaction(SIGBUS,  &sa_bus,  &old_bus);
    sigaction(SIGABRT, &sa_abrt, &old_abrt);

    crash_detected = 0;

    if (sigsetjmp(crash_jmp, 1) == 0) {
        img = stbi_load_from_memory(data, (int)len, &w, &h, &channels, 0);

        if (img != NULL) {
            /* Security invariant: if loading succeeds, dimensions must be
               within sane bounds — no overflow-inducing values */
            ck_assert_msg(w > 0 && w <= 65535,
                "Loaded image width out of sane bounds (%d) for payload: %s", w, label);
            ck_assert_msg(h > 0 && h <= 65535,
                "Loaded image height out of sane bounds (%d) for payload: %s", h, label);
            ck_assert_msg(channels >= 1 && channels <= 4,
                "Loaded image channels out of sane bounds (%d) for payload: %s", channels, label);

            /* Verify we can safely read the claimed buffer without fault */
            size_t expected_size = (size_t)w * (size_t)h * (size_t)channels;
            ck_assert_msg(expected_size > 0 && expected_size <= (size_t)256 * 1024 * 1024,
                "Loaded image buffer size implausible (%zu) for payload: %s",
                expected_size, label);

            /* Touch first and last byte to verify buffer is accessible */
            volatile unsigned char first = img[0];
            volatile unsigned char last  = img[expected_size - 1];
            (void)first; (void)last;

            stbi_image_free(img);
        }
        /* If img == NULL, loading failed gracefully — that is acceptable */
    } else {
        /* A signal was caught — this indicates memory corruption / crash */
        ck_abort_msg("CRASH detected (signal) while processing payload: %s", label);
    }

    /* Restore original signal handlers */
    sigaction(SIGSEGV, &old_segv, NULL);
    sigaction(SIGBUS,  &old_bus,  NULL);
    sigaction(SIGABRT, &old_abrt, NULL);

    ck_assert_msg(!crash_detected,
        "Crash flag set while processing payload: %s", label);
}

/* ------------------------------------------------------------------ */
/* Crafted adversarial payloads                                        */
/* ------------------------------------------------------------------ */

/* PNG with manipulated IHDR: width=0xFFFFFFFF, height=0xFFFFFFFF */
static const unsigned char png_huge_dims[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, /* PNG signature */
    0x00, 0x00, 0x00, 0x0D,                           /* IHDR length = 13 */
    0x49, 0x48, 0x44, 0x52,                           /* "IHDR" */
    0xFF, 0xFF, 0xFF, 0xFF,                           /* width  = 4294967295 */
    0xFF, 0xFF, 0xFF, 0xFF,                           /* height = 4294967295 */
    0x08, 0x02,                                       /* bit depth=8, color=RGB */
    0x00, 0x00, 0x00,                                 /* compression, filter, interlace */
    0x00, 0x00, 0x00, 0x00,                           /* CRC (invalid) */
    0x00, 0x00, 0x00, 0x00,                           /* IDAT length = 0 */
    0x49, 0x44, 0x41, 0x54,                           /* "IDAT" */
    0x00, 0x00, 0x00, 0x00,                           /* CRC */
    0x00, 0x00, 0x00, 0x00,                           /* IEND length = 0 */
    0x49, 0x45, 0x4E, 0x44,                           /* "IEND" */
    0xAE, 0x42, 0x60, 0x82                            /* CRC */
};

/* PNG with width=1, height=1 but IDAT claims huge compressed data */
static const unsigned char png_idat_overflow[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01,  /* width=1 */
    0x00, 0x00, 0x00, 0x01,  /* height=1 */
    0x08, 0x02, 0x00, 0x00, 0x00,
    0x90, 0x77, 0x53, 0xDE,  /* CRC */
    0xFF, 0xFF, 0xFF, 0xFF,  /* IDAT length = 4294967295 (overflow) */
    0x49, 0x44, 0x41, 0x54,
    /* truncated data */
    0x78, 0x9C, 0x62, 0x60, 0x60, 0x60, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x01
};

/* PNG with zero-length IDAT */
static const unsigned char png_zero_idat[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00,
    0x90, 0x77, 0x53, 0xDE,
    0x00, 0x00, 0x00, 0x00,
    0x49, 0x44, 0x41, 0x54,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x49, 0x45, 0x4E, 0x44,
    0xAE, 0x42, 0x60, 0x82
};

/* JPEG with manipulated SOF0 dimensions */
static const unsigned char jpeg_huge_dims[] = {
    0xFF, 0xD8,              /* SOI */
    0xFF, 0xE0,              /* APP0 */
    0x00, 0x10,              /* length=16 */
    0x4A, 0x46, 0x49, 0x46, 0x00, /* "JFIF\0" */
    0x01, 0x01, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0xFF, 0xC0,              /* SOF0 */
    0x00, 0x11,              /* length=17 */
    0x08,                    /* precision=8 */
    0xFF, 0xFF,              /* height=65535 */
    0xFF, 0xFF,              /* width=65535 */
    0x03,                    /* components=3 */
    0x01, 0x11, 0x00,
    0x02, 0x11, 0x01,
    0x03, 0x11, 0x01,
    0xFF, 0xD9               /* EOI */
};

/* BMP with negative height (bottom-up) and huge width */
static const unsigned char bmp_overflow[] = {
    0x42, 0x4D,              /* "BM" */
    0xFF, 0xFF, 0xFF, 0xFF,  /* file size (bogus) */
    0x00, 0x00, 0x00, 0x00,  /* reserved */
    0x36, 0x00, 0x00, 0x00,  /* pixel data offset=54 */
    0x28, 0x00, 0x00, 0x00,  /* BITMAPINFOHEADER size=40 */
    0xFF, 0xFF, 0x7F, 0x00,  /* width=8388607 */
    0x01, 0x00, 0x00, 0x80,  /* height=-1 (negative = bottom-up) */
    0x01, 0x00,              /* planes=1 */
    0x18, 0x00,              /* bpp=24 */
    0x00, 0x00, 0x00, 0x00,  /* compression=BI_RGB */
    0xFF, 0xFF, 0xFF, 0xFF,  /* image size (bogus) */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    /* minimal pixel data */
    0x00, 0x00, 0x00
};

/* GIF with huge logical screen dimensions */
static const unsigned char gif_huge_dims[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, /* "GIF89a" */
    0xFF, 0xFF,              /* logical screen width=65535 */
    0xFF, 0xFF,              /* logical screen height=65535 */
    0xF7,                    /* packed: GCT present, color res=8, sort=0, GCT size=256 */
    0x00,                    /* background color index */
    0x00,                    /* pixel aspect ratio */
    /* Global Color Table: 256 * 3 = 768 bytes (all zeros) */
};

/* TGA with manipulated header: huge dimensions */
static const unsigned char tga_huge_dims[] = {
    0x00,                    /* ID length */
    0x00,                    /* color map type: none */
    0x02,                    /* image type: uncompressed true-color */
    0x00, 0x00, 0x00, 0x00, 0x00, /* color map spec */
    0x00, 0x00,              /* x origin */
    0x00, 0x00,              /* y origin */
    0xFF, 0xFF,              /* width=65535 */
    0xFF, 0xFF,              /* height=65535 */
    0x18,                    /* bits per pixel=24 */
    0x00,                    /* image descriptor */
    /* no pixel data */
};

/* Truncated PNG (just signature) */
static const unsigned char png_truncated[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

/* All-zeros buffer */
static const unsigned char all_zeros[64] = { 0 };

/* All-0xFF buffer */
static const unsigned char all_ff[64] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

/* PNG with filter byte set to invalid value */
static const unsigned char png_bad_filter[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x04,  /* width=4 */
    0x00, 0x00, 0x00, 0x04,  /* height=4 */
    0x08, 0x02, 0x00, 0x00, 0x00,
    0x26, 0x93, 0x09, 0x29,
    /* IDAT with invalid filter byte 0xFF */
    0x