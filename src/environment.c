// SPDX-License-Identifier: MIT
#include "environment.h"
#include "libra_internal.h"
#include "audio.h"
#include "vfs.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* Thread-local context pointer set before retro_run() and init calls */
static libra_ctx_t *s_ctx = NULL;

void libra_environment_set_ctx(libra_ctx_t *ctx)
{
    s_ctx = ctx;
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Insert key/default only if key not already present.
 * val_list is a "|"-joined string of possible values (may be NULL).
 * Marks the option as visible by default. */
static void insert_option(libra_ctx_t *ctx, const char *key,
                           const char *default_value, const char *val_list)
{
    if (!key)
        return;
    for (unsigned i = 0; i < ctx->opt_count; i++) {
        if (strcmp(ctx->opt_keys[i], key) == 0)
            return; /* already exists — preserve current value */
    }
    if (ctx->opt_count >= LIBRA_MAX_OPTIONS)
        return;

    unsigned idx = ctx->opt_count;
    ctx->opt_keys[idx]     = strdup(key);
    ctx->opt_vals[idx]     = strdup(default_value ? default_value : "");
    ctx->opt_val_list[idx] = val_list ? strdup(val_list) : NULL;
    ctx->opt_visible[idx]  = true;
    ctx->opt_count++;
}

/* Build a "|"-joined value list from a retro_core_option_value array
 * (v1/v2 format).  Returns a malloc'd string; caller must free. */
static char *build_val_list(const struct retro_core_option_value *values)
{
    char buf[2048];
    size_t pos = 0;
    for (int i = 0;
         i < RETRO_NUM_CORE_OPTION_VALUES_MAX && values[i].value != NULL;
         i++) {
        if (i > 0 && pos < sizeof(buf) - 1)
            buf[pos++] = '|';
        const char *v = values[i].value;
        size_t len = strlen(v);
        if (pos + len >= sizeof(buf))
            break;
        memcpy(buf + pos, v, len);
        pos += len;
    }
    buf[pos] = '\0';
    return pos > 0 ? strdup(buf) : NULL;
}

/* Log callback forwarded to stderr */
static void log_printf(enum retro_log_level level, const char *fmt, ...)
{
    const char *prefix;
    va_list args;

    switch (level) {
        case RETRO_LOG_DEBUG: prefix = "[DEBUG]"; break;
        case RETRO_LOG_INFO:  prefix = "[INFO] "; break;
        case RETRO_LOG_WARN:  prefix = "[WARN] "; break;
        case RETRO_LOG_ERROR: prefix = "[ERROR]"; break;
        default:              prefix = "[?]    "; break;
    }

    fprintf(stderr, "libra %s ", prefix);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/* Rumble callback — forwards to host */
static bool rumble_set_state(unsigned port,
                              enum retro_rumble_effect effect,
                              uint16_t strength)
{
    if (s_ctx && s_ctx->config.rumble)
        s_ctx->config.rumble(s_ctx->config.userdata,
                             port, (unsigned)effect, strength);
    return true;
}

/* -------------------------------------------------------------------------
 * Main environment callback
 * ---------------------------------------------------------------------- */

bool libra_environment_cb(unsigned cmd, void *data)
{
    libra_ctx_t *ctx = s_ctx;
    if (!ctx)
        return false;

    switch (cmd) {

        /* ---- Core status ------------------------------------------------- */

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (data) *(bool *)data = true;
            return true;

        case RETRO_ENVIRONMENT_SHUTDOWN:
            ctx->shutdown_requested = true;
            return true;

        /* ---- Video -------------------------------------------------------- */

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            ctx->pixel_format = *(const enum retro_pixel_format *)data;
            return true;

        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            if (ctx->core) {
                const struct retro_game_geometry *g =
                    (const struct retro_game_geometry *)data;
                ctx->core->av_info.geometry = *g;
            }
            return true;

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
            if (ctx->core) {
                const struct retro_system_av_info *av =
                    (const struct retro_system_av_info *)data;
                ctx->core->av_info = *av;
                if (ctx->audio)
                    libra_audio_set_src_rate(ctx->audio, av->timing.sample_rate);
            }
            return true;

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            if (data) *(int *)data = 3;
            return true;

        /* ---- Directories / identity -------------------------------------- */

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *(const char **)data = ctx->system_dir;
            return true;

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char **)data = ctx->save_dir;
            return true;

        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
            *(const char **)data = ctx->core_path;
            return true;

        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
            *(const char **)data = ctx->assets_dir;
            return true;

        case RETRO_ENVIRONMENT_GET_USERNAME:
            *(const char **)data = NULL;
            return true;

        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            if (data) *(unsigned *)data = RETRO_LANGUAGE_ENGLISH;
            return true;

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
            if (data) *(bool *)data = ctx->fast_forwarding;
            return true;

        /* ---- Logging / messages ------------------------------------------ */

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback *cb = (struct retro_log_callback *)data;
            cb->log = log_printf;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const struct retro_message *msg = (const struct retro_message *)data;
            fprintf(stderr, "libra [MSG] %s\n", msg->msg);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
            const struct retro_message_ext *msg =
                (const struct retro_message_ext *)data;
            /* Route to log or OSD — we just forward to stderr */
            const char *lvl;
            switch (msg->level) {
                case RETRO_LOG_DEBUG: lvl = "[DEBUG]"; break;
                case RETRO_LOG_WARN:  lvl = "[WARN] "; break;
                case RETRO_LOG_ERROR: lvl = "[ERROR]"; break;
                default:              lvl = "[INFO] "; break;
            }
            fprintf(stderr, "libra %s %s\n", lvl, msg->msg);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            /* data may be NULL — return value alone signals support */
            if (data) *(bool *)data = true;
            return true;

        /* ---- Input -------------------------------------------------------- */

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true;

        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            return true;

        /* ---- Rumble ------------------------------------------------------- */

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            struct retro_rumble_interface *ri =
                (struct retro_rumble_interface *)data;
            ri->set_rumble_state = rumble_set_state;
            return true;
        }

        /* ---- VFS ---------------------------------------------------------- */

        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            struct retro_vfs_interface_info *info =
                (struct retro_vfs_interface_info *)data;
            if (info->required_interface_version > LIBRA_VFS_VERSION)
                return false;
            info->required_interface_version = LIBRA_VFS_VERSION;
            info->iface = libra_vfs_get_interface();
            return true;
        }

        /* ---- Misc no-ops -------------------------------------------------- */

        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
            return true;

        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
            return true;

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true;

        /* ---- Core options ------------------------------------------------- */

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            if (data) *(unsigned *)data = 2;
            return true;

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            /* Legacy format: "description; val1|val2" — extract first value as
             * default and the whole "val1|val2|..." portion as val_list. */
            const struct retro_variable *vars =
                (const struct retro_variable *)data;
            if (!vars)
                return true;
            for (; vars->key != NULL; vars++) {
                const char *def      = "";
                const char *val_list = NULL;
                const char *semi = vars->value ? strchr(vars->value, ';') : NULL;
                if (semi) {
                    const char *vals = semi + 1;
                    while (*vals == ' ') vals++;
                    val_list = vals; /* "|"-joined list already */
                    const char *pipe = strchr(vals, '|');
                    char tmp[256] = "";
                    if (pipe) {
                        size_t len = (size_t)(pipe - vals);
                        if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
                        memcpy(tmp, vals, len);
                        tmp[len] = '\0';
                        def = tmp;
                    } else {
                        def = vals;
                    }
                }
                insert_option(ctx, vars->key, def, val_list);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            const struct retro_core_option_definition *defs =
                (const struct retro_core_option_definition *)data;
            if (!defs) return true;
            for (; defs->key != NULL; defs++) {
                char *vl = build_val_list(defs->values);
                insert_option(ctx, defs->key, defs->default_value, vl);
                free(vl);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            const struct retro_core_options_intl *intl =
                (const struct retro_core_options_intl *)data;
            if (!intl || !intl->us) return true;
            for (const struct retro_core_option_definition *d = intl->us;
                 d->key != NULL; d++) {
                char *vl = build_val_list(d->values);
                insert_option(ctx, d->key, d->default_value, vl);
                free(vl);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
            const struct retro_core_options_v2 *opts =
                (const struct retro_core_options_v2 *)data;
            if (!opts || !opts->definitions) return true;
            for (const struct retro_core_option_v2_definition *d = opts->definitions;
                 d->key != NULL; d++) {
                char *vl = build_val_list(d->values);
                insert_option(ctx, d->key, d->default_value, vl);
                free(vl);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
            const struct retro_core_options_v2_intl *intl =
                (const struct retro_core_options_v2_intl *)data;
            if (!intl || !intl->us || !intl->us->definitions) return true;
            for (const struct retro_core_option_v2_definition *d =
                     intl->us->definitions; d->key != NULL; d++) {
                char *vl = build_val_list(d->values);
                insert_option(ctx, d->key, d->default_value, vl);
                free(vl);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *var = (struct retro_variable *)data;
            if (!var->key) { var->value = NULL; return false; }
            var->value = NULL;
            for (unsigned i = 0; i < ctx->opt_count; i++) {
                if (strcmp(ctx->opt_keys[i], var->key) == 0) {
                    var->value = ctx->opt_vals[i];
                    return true;
                }
            }
            return false;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (data) {
                *(bool *)data = ctx->opt_updated;
                ctx->opt_updated = false;
            }
            return true;

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
            const struct retro_core_option_display *d =
                (const struct retro_core_option_display *)data;
            if (!d || !d->key)
                return false;
            for (unsigned i = 0; i < ctx->opt_count; i++) {
                if (strcmp(ctx->opt_keys[i], d->key) == 0) {
                    ctx->opt_visible[i] = d->visible;
                    return true;
                }
            }
            return false;
        }

        /* ---- Disk control ------------------------------------------------- */

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: {
            const struct retro_disk_control_callback *cb =
                (const struct retro_disk_control_callback *)data;
            if (cb) { ctx->disk_cb = *cb; ctx->has_disk_cb = true; }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
            const struct retro_disk_control_ext_callback *cb =
                (const struct retro_disk_control_ext_callback *)data;
            if (cb) {
                ctx->disk_ext_cb     = *cb;
                ctx->has_disk_ext_cb = true;
                /* Mirror the 7 shared fields into the basic struct */
                ctx->disk_cb.set_eject_state     = cb->set_eject_state;
                ctx->disk_cb.get_eject_state     = cb->get_eject_state;
                ctx->disk_cb.get_image_index     = cb->get_image_index;
                ctx->disk_cb.set_image_index     = cb->set_image_index;
                ctx->disk_cb.get_num_images      = cb->get_num_images;
                ctx->disk_cb.replace_image_index = cb->replace_image_index;
                ctx->disk_cb.add_image_index     = cb->add_image_index;
                ctx->has_disk_cb = true;
            }
            return true;
        }

        /* ---- Subsystem ---------------------------------------------------- */

        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: {
            const struct retro_subsystem_info *info =
                (const struct retro_subsystem_info *)data;
            ctx->subsystem_info = info;
            ctx->subsystem_count = 0;
            if (info) {
                while (info[ctx->subsystem_count].ident != NULL)
                    ctx->subsystem_count++;
            }
            return true;
        }

        /* ---- Identity / locale ------------------------------------------- */

        default:
            return false;
    }
}
