// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_asm.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the assembler, as a program: read a file, write the bytes to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../toolchain/asmlib.h"
#include "../toolchain/asmpre.h"
#include "../toolchain/asmelf.h"

static struct assembler a;

int main(int argc, char **argv)
{
    /* `-o file.o` writes an object; without it, flat bytes to stdout. */
    const char *object = NULL;
    const char *source = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            object = argv[++i];
        } else {
            source = argv[i];
        }
    }
    if (source == NULL) {
        fprintf(stderr, "usage: asm [-o out.o] <file.asm>\n");
        return 2;
    }
    FILE *f = fopen(source, "r");
    if (f == NULL) {
        fprintf(stderr, "asm: cannot read %s\n", source);
        return 2;
    }

    /* the whole file, then the macro pass over it, and only then the assembler. */
    static char raw[16384][ASMPRE_LINE];
    static char expanded[65536][ASMPRE_LINE];
    size_t raw_n = 0;

    while (raw_n < 16384 && fgets(raw[raw_n], ASMPRE_LINE, f) != NULL) {
        raw_n++;
    }
    fclose(f);
    f = NULL;

    size_t exp_n = 0;
    char pre_error[160] = { 0 };
    int  pre_line = 0;
    if (!asmpre_run(raw, raw_n, expanded, 65536, &exp_n,
                    pre_error, sizeof pre_error, &pre_line)) {
        fprintf(stderr, "asm: line %d: %s\n", pre_line + 1, pre_error);
        return 1;
    }

    asm_init(&a);

    /* passes, until nothing changes. */
    for (int pass = 0; pass < 16; pass++) {
        asm_restart(&a);

        bool bad = false;
        for (size_t n = 0; n < exp_n; n++) {
            if (!asm_line(&a, expanded[n], (int)n + 1)) {
                bad = true;
                break;
            }
        }
        if (bad || !asm_finish(&a)) {
            fprintf(stderr, "asm: line %d: %s\n", a.error_line, a.error);
            return 1;
        }
        if (!a.grew) {
            break;
        }
    }

    if (object != NULL) {
        static uint8_t obj[512 * 1024];
        size_t n = 0;
        char err[160] = { 0 };
        if (!asmelf_write(&a, obj, sizeof obj, &n, err, sizeof err)) {
            fprintf(stderr, "asm: %s\n", err);
            return 1;
        }
        FILE *o = fopen(object, "wb");
        if (o == NULL) {
            fprintf(stderr, "asm: cannot write %s\n", object);
            return 1;
        }
        fwrite(obj, 1, n, o);
        fclose(o);
        return 0;
    }

    /*
     * flat output is every section in order, which is what a file with
     * `org` and no linker means by them
     */
    for (int i = 0; i < ASM_SECTIONS; i++) {
        fwrite(a.out[i], 1, a.len[i], stdout);
    }
    return 0;
}
