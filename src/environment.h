// SPDX-License-Identifier: MIT
#ifndef LIBRA_ENVIRONMENT_H
#define LIBRA_ENVIRONMENT_H

#include <stdbool.h>
#include "libretro.h"

struct libra_ctx; /* forward declaration */

bool libra_environment_cb(unsigned cmd, void *data);
void libra_environment_set_ctx(struct libra_ctx *ctx);

#endif /* LIBRA_ENVIRONMENT_H */
