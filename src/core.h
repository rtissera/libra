// SPDX-License-Identifier: MIT
#ifndef LIBRA_CORE_H
#define LIBRA_CORE_H

#include <stdbool.h>
#include "libretro.h"

typedef struct {
    void *dl_handle;

    /* Mandatory libretro symbols */
    void     (*retro_init)(void);
    void     (*retro_deinit)(void);
    unsigned (*retro_api_version)(void);
    void     (*retro_get_system_info)(struct retro_system_info *info);
    void     (*retro_get_system_av_info)(struct retro_system_av_info *info);
    bool     (*retro_load_game)(const struct retro_game_info *game);
    void     (*retro_unload_game)(void);
    void     (*retro_run)(void);
    void     (*retro_reset)(void);
    void     (*retro_set_environment)(retro_environment_t cb);
    void     (*retro_set_video_refresh)(retro_video_refresh_t cb);
    void     (*retro_set_audio_sample)(retro_audio_sample_t cb);
    void     (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb);
    void     (*retro_set_input_poll)(retro_input_poll_t cb);
    void     (*retro_set_input_state)(retro_input_state_t cb);

    /* Optional — may be NULL if core does not implement them */
    size_t (*retro_serialize_size)(void);
    bool   (*retro_serialize)(void *data, size_t size);
    bool   (*retro_unserialize)(const void *data, size_t size);
    void  *(*retro_get_memory_data)(unsigned id);
    size_t (*retro_get_memory_size)(unsigned id);
    void   (*retro_set_controller_port_device)(unsigned port, unsigned device);
    void   (*retro_cheat_reset)(void);
    void   (*retro_cheat_set)(unsigned index, bool enabled, const char *code);
    bool   (*retro_load_game_special)(unsigned game_type,
               const struct retro_game_info *info, size_t num_info);

    struct retro_system_info    sys_info;
    struct retro_system_av_info av_info;
} libra_core_t;

libra_core_t *libra_core_load(const char *path);
void          libra_core_unload(libra_core_t *core);

#endif /* LIBRA_CORE_H */
