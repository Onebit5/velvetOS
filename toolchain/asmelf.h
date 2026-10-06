// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmelf.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * an object file: the bytes, the names, and the holes.
 */

#ifndef USER_ASMELF_H
#define USER_ASMELF_H

#include "asmlib.h"

/*
 * an object file: the bytes, the names, and the holes.
 *
 * an assembler's answer to a name it does not know is not an error,
 * it is a *relocation*, a note saying which bytes are a reference to
 * which name, for whoever ends up providing it. writing that note is
 * the whole of the contract with a linker, and it is what lets
 * `isr.asm` and `syscall.asm` be assembled at all: both call a C
 * function that is not in the file.
 */
bool asmelf_write(const struct assembler *a, uint8_t *out, size_t max,
                  size_t *written, char *error, size_t error_max);

#endif
