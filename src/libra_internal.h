// SPDX-License-Identifier: MIT
#ifndef LIBRA_INTERNAL_H
#define LIBRA_INTERNAL_H

#include "libra.h"
#include "core.h"
#include "audio.h"
#include "libretro.h"

#define LIBRA_MAX_OPTIONS 128

struct libra_ctx {
    libra_config_t  config;
    libra_core_t   *core;       /* NULL when no core loaded */
    int             pixel_format;
    bool            game_loaded;
    bool            shutdown_requested;

    char           *system_dir;
    char           *save_dir;
    char           *assets_dir;
    char           *core_path;

    bool            fast_forwarding;

    /* Core options: simple fixed-size key/value store */
    char           *opt_keys[LIBRA_MAX_OPTIONS];
    char           *opt_vals[LIBRA_MAX_OPTIONS];
    char           *opt_val_list[LIBRA_MAX_OPTIONS]; /* "|"-joined possible values */
    bool            opt_visible[LIBRA_MAX_OPTIONS]; /* per-key visibility hint */
    unsigned        opt_count;
    bool            opt_updated;

    /* SRAM dirty-detection shadow copy */
    uint8_t        *sram_shadow;
    size_t          sram_shadow_size;

    /* Audio resampler state */
    libra_audio_t  *audio;

    /* Disk control (populated by SET_DISK_CONTROL_INTERFACE or _EXT_) */
    struct retro_disk_control_callback     disk_cb;
    struct retro_disk_control_ext_callback disk_ext_cb;
    bool has_disk_cb;
    bool has_disk_ext_cb;

    /* Subsystem info (static data owned by the core) */
    const struct retro_subsystem_info *subsystem_info;
    unsigned                           subsystem_count;
};

#endif /* LIBRA_INTERNAL_H */
