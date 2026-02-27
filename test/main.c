// SPDX-License-Identifier: MIT
/*
 * libra smoke test
 * Usage: libra_test <core.so> <rom> [num_frames]
 *
 * Runs the core for N frames (default 60), then writes:
 *   frame.ppm   — last non-NULL video frame as a PPM image
 *   audio.raw   — resampled int16 stereo PCM at 48 kHz
 *
 * Requires no display or audio hardware.
 */

#include "libra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Video capture
 * ----------------------------------------------------------------------- */

static void  *g_frame       = NULL;
static unsigned g_frame_w   = 0;
static unsigned g_frame_h   = 0;
static size_t   g_frame_pitch = 0;
static int      g_frame_fmt = 0;  /* 0=XRGB1555, 1=XRGB8888, 2=RGB565 */

static void video_cb(void *ud, const void *data,
                     unsigned w, unsigned h, size_t pitch, int pixel_format)
{
    (void)ud;
    if (!data) return; /* NULL = duplicate frame, skip */

    /* Always keep the latest frame */
    void *buf = realloc(g_frame, h * pitch);
    if (!buf) return;
    memcpy(buf, data, h * pitch);

    g_frame       = buf;
    g_frame_w     = w;
    g_frame_h     = h;
    g_frame_pitch = pitch;
    g_frame_fmt   = pixel_format;
}

/* --------------------------------------------------------------------------
 * Audio capture (fixed ring — enough for several seconds at 48 kHz)
 * ----------------------------------------------------------------------- */

#define AUDIO_MAX_FRAMES  (48000 * 4)   /* 4 s stereo @ 48 kHz */

static int16_t  g_audio[AUDIO_MAX_FRAMES * 2];
static size_t   g_audio_frames = 0;

static void audio_cb(void *ud, const int16_t *data, size_t frames)
{
    (void)ud;
    size_t space = AUDIO_MAX_FRAMES - g_audio_frames;
    if (frames > space) frames = space;
    memcpy(g_audio + g_audio_frames * 2, data, frames * 2 * sizeof(int16_t));
    g_audio_frames += frames;
}

/* --------------------------------------------------------------------------
 * Input (all zeroes — no buttons pressed)
 * ----------------------------------------------------------------------- */

static void    input_poll_cb(void *ud) { (void)ud; }
static int16_t input_state_cb(void *ud, unsigned port, unsigned device,
                               unsigned index, unsigned id)
{
    (void)ud; (void)port; (void)device; (void)index; (void)id;
    return 0;
}

/* --------------------------------------------------------------------------
 * PPM writer
 * ----------------------------------------------------------------------- */

static void ppm_write_pixel(FILE *f, uint32_t px, int fmt)
{
    uint8_t r, g, b;
    switch (fmt) {
        case 1: /* XRGB8888 — 0x00RRGGBB */
            r = (px >> 16) & 0xFF;
            g = (px >>  8) & 0xFF;
            b = (px      ) & 0xFF;
            break;
        case 2: /* RGB565 — RRRRRGGG GGGBBBBB */
            r = ((px >> 11) & 0x1F) * 255 / 31;
            g = ((px >>  5) & 0x3F) * 255 / 63;
            b = ((px      ) & 0x1F) * 255 / 31;
            break;
        default: /* XRGB1555 — 0RRRRRGGGGGBBBBB */
            r = ((px >> 10) & 0x1F) * 255 / 31;
            g = ((px >>  5) & 0x1F) * 255 / 31;
            b = ((px      ) & 0x1F) * 255 / 31;
            break;
    }
    fputc(r, f);
    fputc(g, f);
    fputc(b, f);
}

static void write_ppm(const char *path)
{
    if (!g_frame) { fprintf(stderr, "  No frame captured\n"); return; }

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }

    fprintf(f, "P6\n%u %u\n255\n", g_frame_w, g_frame_h);

    for (unsigned y = 0; y < g_frame_h; y++) {
        const uint8_t *row = (const uint8_t *)g_frame + y * g_frame_pitch;
        for (unsigned x = 0; x < g_frame_w; x++) {
            uint32_t px;
            if (g_frame_fmt == 1) {
                px = ((const uint32_t *)row)[x];
            } else {
                px = ((const uint16_t *)row)[x];
            }
            ppm_write_pixel(f, px, g_frame_fmt);
        }
    }
    fclose(f);
    printf("  frame.ppm    %ux%u fmt=%d\n", g_frame_w, g_frame_h, g_frame_fmt);
}

/* --------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <rom> [num_frames]\n", argv[0]);
        fprintf(stderr, "  Outputs: frame.ppm  audio.raw\n");
        return 1;
    }

    const char *core_path = argv[1];
    const char *rom_path  = argv[2];
    int         num_frames = (argc >= 4) ? atoi(argv[3]) : 60;
    if (num_frames <= 0) num_frames = 60;

    printf("libra smoke test\n");
    printf("  core:   %s\n", core_path);
    printf("  rom:    %s\n", rom_path);
    printf("  frames: %d\n\n", num_frames);

    /* Create context */
    libra_config_t cfg = {
        .video             = video_cb,
        .audio             = audio_cb,
        .input_poll        = input_poll_cb,
        .input_state       = input_state_cb,
        .audio_output_rate = 48000,
    };

    libra_ctx_t *ctx = libra_create(&cfg);
    if (!ctx) { fprintf(stderr, "libra_create failed\n"); return 1; }

    libra_set_system_directory(ctx, "/tmp");
    libra_set_save_directory(ctx, "/tmp");
    libra_set_assets_directory(ctx, "/tmp");

    /* Load core */
    printf("Loading core...\n");
    if (!libra_load_core(ctx, core_path)) {
        fprintf(stderr, "Failed to load core\n");
        libra_destroy(ctx);
        return 1;
    }
    printf("  name:       %s\n", libra_core_name(ctx));
    printf("  extensions: %s\n\n", libra_core_extensions(ctx));

    /* Load game */
    printf("Loading game...\n");
    if (!libra_load_game(ctx, rom_path)) {
        fprintf(stderr, "Failed to load game\n");
        libra_destroy(ctx);
        return 1;
    }

    unsigned base_w, base_h;
    float aspect;
    libra_get_geometry(ctx, &base_w, &base_h, &aspect);
    printf("  geometry:   %ux%u (aspect %.3f)\n", base_w, base_h, aspect);
    printf("  fps:        %.4f\n", libra_get_fps(ctx));
    printf("  samplerate: %.0f Hz\n\n", libra_get_sample_rate(ctx));

    /* Run frames */
    printf("Running %d frames...\n", num_frames);
    for (int i = 0; i < num_frames; i++)
        libra_run(ctx);
    printf("  Done.\n\n");

    /* Outputs */
    printf("Writing outputs...\n");
    write_ppm("frame.ppm");

    if (g_audio_frames > 0) {
        FILE *af = fopen("audio.raw", "wb");
        if (af) {
            fwrite(g_audio, sizeof(int16_t), g_audio_frames * 2, af);
            fclose(af);
            printf("  audio.raw   %zu frames  (%.2f s at 48 kHz)\n",
                   g_audio_frames,
                   (double)g_audio_frames / 48000.0);
        }
    } else {
        printf("  audio.raw   (no audio produced)\n");
    }

    /* Save state */
    printf("\nSave state: ");
    if (libra_save_state(ctx, "/tmp/libra_test.state"))
        printf("OK (/tmp/libra_test.state)\n");
    else
        printf("not supported by this core\n");

    /* SRAM */
    printf("SRAM save:  ");
    if (libra_save_sram(ctx, "/tmp/libra_test.srm"))
        printf("OK (/tmp/libra_test.srm)\n");
    else
        printf("none (core has no SRAM or interface missing)\n");

    /* Teardown */
    libra_unload_game(ctx);
    libra_unload_core(ctx);
    libra_destroy(ctx);
    free(g_frame);

    printf("\nAll done.\n");
    printf("  View frame:  eog frame.ppm\n");
    printf("  Play audio:  aplay -r 48000 -f S16_LE -c 2 audio.raw\n");
    return 0;
}
