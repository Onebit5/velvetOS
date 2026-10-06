// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmpre.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the macro pass, in front of the assembler.
 */

#ifndef USER_ASMPRE_H
#define USER_ASMPRE_H

#include <stddef.h>
#include <stdbool.h>

/* the macro pass, in front of the assembler. */

#define ASMPRE_LINE 512

bool asmpre_run(char (*in)[ASMPRE_LINE], size_t count,
                char (*out)[ASMPRE_LINE], size_t out_max, size_t *out_len,
                char *error, size_t error_max, int *error_line);

#endif
