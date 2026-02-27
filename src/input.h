// SPDX-License-Identifier: MIT
#ifndef LIBRA_INPUT_H
#define LIBRA_INPUT_H

#include <stdint.h>

struct libra_ctx; /* forward declaration */

void    libra_input_poll(struct libra_ctx *ctx);
int16_t libra_input_state(struct libra_ctx *ctx,
                           unsigned port, unsigned device,
                           unsigned index, unsigned id);

#endif /* LIBRA_INPUT_H */
