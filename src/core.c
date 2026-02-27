// SPDX-License-Identifier: MIT
#include "core.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX-compliant way to assign dlsym result to function pointer */
#define LOAD_SYM(core, name) \
    do { \
        void *_sym = dlsym((core)->dl_handle, #name); \
        if (!_sym) { \
            fprintf(stderr, "libra: missing symbol '%s': %s\n", #name, dlerror()); \
            libra_core_unload(core); \
            return NULL; \
        } \
        __extension__ (*(void **)(&(core)->name) = _sym); \
    } while (0)

/* Optional symbols: silently leave NULL if not found */
#define LOAD_SYM_OPT(core, name) \
    do { \
        dlerror(); \
        void *_sym = dlsym((core)->dl_handle, #name); \
        if (_sym) \
            __extension__ (*(void **)(&(core)->name) = _sym); \
    } while (0)

libra_core_t *libra_core_load(const char *path)
{
    libra_core_t *core = calloc(1, sizeof(*core));
    if (!core)
        return NULL;

    dlerror(); /* clear */
    core->dl_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!core->dl_handle) {
        fprintf(stderr, "libra: dlopen '%s' failed: %s\n", path, dlerror());
        free(core);
        return NULL;
    }

    LOAD_SYM(core, retro_init);
    LOAD_SYM(core, retro_deinit);
    LOAD_SYM(core, retro_api_version);
    LOAD_SYM(core, retro_get_system_info);
    LOAD_SYM(core, retro_get_system_av_info);
    LOAD_SYM(core, retro_load_game);
    LOAD_SYM(core, retro_unload_game);
    LOAD_SYM(core, retro_run);
    LOAD_SYM(core, retro_reset);
    LOAD_SYM(core, retro_set_environment);
    LOAD_SYM(core, retro_set_video_refresh);
    LOAD_SYM(core, retro_set_audio_sample);
    LOAD_SYM(core, retro_set_audio_sample_batch);
    LOAD_SYM(core, retro_set_input_poll);
    LOAD_SYM(core, retro_set_input_state);

    /* Optional symbols */
    LOAD_SYM_OPT(core, retro_serialize_size);
    LOAD_SYM_OPT(core, retro_serialize);
    LOAD_SYM_OPT(core, retro_unserialize);
    LOAD_SYM_OPT(core, retro_get_memory_data);
    LOAD_SYM_OPT(core, retro_get_memory_size);
    LOAD_SYM_OPT(core, retro_set_controller_port_device);
    LOAD_SYM_OPT(core, retro_cheat_reset);
    LOAD_SYM_OPT(core, retro_cheat_set);
    LOAD_SYM_OPT(core, retro_load_game_special);

    if (core->retro_api_version() != RETRO_API_VERSION) {
        fprintf(stderr, "libra: core API version mismatch (expected %d)\n",
                RETRO_API_VERSION);
        libra_core_unload(core);
        return NULL;
    }

    core->retro_get_system_info(&core->sys_info);

    return core;
}

void libra_core_unload(libra_core_t *core)
{
    if (!core)
        return;
    if (core->dl_handle)
        dlclose(core->dl_handle);
    free(core);
}
