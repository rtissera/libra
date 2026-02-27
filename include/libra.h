// SPDX-License-Identifier: MIT
#ifndef LIBRA_H
#define LIBRA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libra_ctx libra_ctx_t;

/* pixel_format values match retro_pixel_format enum:
 * 0 = XRGB1555, 1 = XRGB8888, 2 = RGB565 */
typedef void    (*libra_video_cb_t)(void *ud, const void *data,
                    unsigned w, unsigned h, size_t pitch, int pixel_format);
typedef void    (*libra_audio_cb_t)(void *ud, const int16_t *data, size_t frames);
typedef void    (*libra_input_poll_cb_t)(void *ud);
typedef int16_t (*libra_input_state_cb_t)(void *ud,
                    unsigned port, unsigned device, unsigned index, unsigned id);
/* effect: 0=strong, 1=weak (matches retro_rumble_effect); strength: 0-65535 */
typedef void    (*libra_rumble_cb_t)(void *ud,
                    unsigned port, unsigned effect, uint16_t strength);

typedef struct {
    libra_video_cb_t       video;
    libra_audio_cb_t       audio;
    libra_input_poll_cb_t  input_poll;
    libra_input_state_cb_t input_state;
    libra_rumble_cb_t      rumble;          /* optional; NULL = no rumble */
    void                  *userdata;
    unsigned               audio_output_rate;  /* target rate Hz, e.g. 48000 */
} libra_config_t;

/* Lifecycle */
libra_ctx_t *libra_create(const libra_config_t *config);
void         libra_destroy(libra_ctx_t *ctx);

/* Directories (set before loading core) */
void libra_set_system_directory(libra_ctx_t *ctx, const char *path);
void libra_set_save_directory(libra_ctx_t *ctx, const char *path);
void libra_set_assets_directory(libra_ctx_t *ctx, const char *path);

/* Fast-forward mode — cores may skip non-essential work when true */
void libra_set_fast_forward(libra_ctx_t *ctx, bool enabled);

/* Core */
bool libra_load_core(libra_ctx_t *ctx, const char *path);
void libra_unload_core(libra_ctx_t *ctx);

/* Game */
bool libra_load_game(libra_ctx_t *ctx, const char *path);
void libra_unload_game(libra_ctx_t *ctx);

/* Per-frame loop */
void libra_run(libra_ctx_t *ctx);

/* Soft reset */
void libra_reset(libra_ctx_t *ctx);

/* Queries (valid after libra_load_game) */
double      libra_get_fps(libra_ctx_t *ctx);
double      libra_get_sample_rate(libra_ctx_t *ctx);
void        libra_get_geometry(libra_ctx_t *ctx,
                unsigned *base_w, unsigned *base_h, float *aspect);
const char *libra_core_name(libra_ctx_t *ctx);
const char *libra_core_extensions(libra_ctx_t *ctx);

/* Core options — key/value get/set */
const char *libra_get_option(libra_ctx_t *ctx, const char *key);
void        libra_set_option(libra_ctx_t *ctx, const char *key, const char *value);

/* Core option iteration (for building a quick-menu UI) */
unsigned    libra_option_count(libra_ctx_t *ctx);
const char *libra_option_key  (libra_ctx_t *ctx, unsigned index);
const char *libra_option_value(libra_ctx_t *ctx, unsigned index);
/* Cycle the option at index by direction (+1 = next, -1 = prev), wrapping.
 * Sets opt_updated so the core will see the change next retro_run().
 * Returns false if no value list is available for this option. */
bool        libra_option_cycle(libra_ctx_t *ctx, unsigned index, int direction);

/* Save states */
bool libra_save_state(libra_ctx_t *ctx, const char *path);
bool libra_load_state(libra_ctx_t *ctx, const char *path);

/* SRAM (battery-backed save RAM) */
bool libra_save_sram(libra_ctx_t *ctx, const char *path);
bool libra_load_sram(libra_ctx_t *ctx, const char *path);
/* Write SRAM to path only if contents changed since last save/load.
 * The first call after libra_load_game() establishes the baseline (no write).
 * Returns true if a write was performed. */
bool libra_save_sram_if_dirty(libra_ctx_t *ctx, const char *path);

/* Controller port — call after libra_load_game; device constants from libretro.h */
void libra_set_controller(libra_ctx_t *ctx, unsigned port, unsigned device);

/* Disk control — swap to disk at index (0-based); ejects then re-inserts */
bool libra_get_disk_count(libra_ctx_t *ctx, unsigned *count);
bool libra_swap_disk(libra_ctx_t *ctx, unsigned index);

/* Cheats */
void libra_clear_cheats(libra_ctx_t *ctx);
void libra_set_cheat(libra_ctx_t *ctx, unsigned index, bool enabled, const char *code);

/* Core option visibility (updated dynamically by core during retro_run) */
bool libra_is_option_visible(libra_ctx_t *ctx, const char *key);

/* Subsystem / multi-ROM loading (e.g. Game Boy link, Sufami Turbo)
 * game_type: matches retro_subsystem_info::id supplied by the core
 * paths[]:   one path per required ROM slot, in the order the core declared them */
bool libra_load_game_special(libra_ctx_t *ctx, unsigned game_type,
                              const char **paths, unsigned num_paths);

/* Returns true if the core called RETRO_ENVIRONMENT_SHUTDOWN */
bool libra_is_shutdown_requested(libra_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LIBRA_H */
