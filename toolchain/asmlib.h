// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmlib.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * an assembler for the x86-64 this kernel actually contains.
 */

#ifndef USER_ASMLIB_H
#define USER_ASMLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* an assembler for the x86-64 this kernel actually contains. */

#define ASM_MAX_OUTPUT  (256 * 1024)
#define ASM_MAX_LABELS  512
#define ASM_MAX_PATCHES 2048

/* which output a thing belongs to. */
enum asm_section { ASM_TEXT, ASM_RODATA, ASM_DATA, ASM_SECTIONS };

struct asm_label {
    char     name[64];
    uint64_t at;
    bool     defined;
    /* `global name` said so: this one is for other files to use. */
    bool     exported;
    enum asm_section section;
};

/* a place where a label's value has to be written once it is known. */
struct asm_patch {
    uint64_t at;        /* where in the output the field starts */
    uint8_t  size;      /* 1, 4 or 8 bytes */
    uint64_t next;      /* the address the displacement is relative to */
    char     name[64];
    int      line;
    int      jump;      /* which branch this is, or -1 */
    bool     unresolved; /* nothing here defines it: a relocation */
    enum asm_section section;
};

struct assembler {
    uint8_t  out[ASM_SECTIONS][ASM_MAX_OUTPUT];
    size_t   len[ASM_SECTIONS];
    enum asm_section sec;   /* where bytes are going now */
    uint64_t origin;    /* what address the first byte will live at */

    /* which mode the processor will be in when it reads these bytes. */
    int bits;

    struct asm_label  label[ASM_MAX_LABELS];
    size_t            labels;
    struct asm_patch  patch[ASM_MAX_PATCHES];
    size_t            patches;

    /*
     * a conditional jump has a two-byte form with a one-byte
     * displacement and a six-byte form with four, and nasm picks the
     * short one whenever the distance fits. matching that is not
     * optional if the output is to be compared byte for byte.
     *
     * the trouble is that the choice *changes the distance*: shortening
     * one jump moves every label after it, which may bring another jump
     * into range, which shortens that one too. so the assembly is run
     * repeatedly with each branch's decision remembered between passes,
     * starting with every one short and lengthening those that do not
     * fit, until a pass changes nothing.
     *
     * it converges because a branch only ever goes from short to long
     * and never back, so there are at most as many passes as there
     * are branches, and in practice two or three.
     */
    bool   near_form[ASM_MAX_PATCHES];
    size_t jumps;       /* how many branches this pass has seen */
    bool   grew;        /* a branch had to lengthen: go round again */

    char error[192];
    int  error_line;
};

/*
 * start another pass. the branch decisions survive; everything else,
 * the output, the labels, the patches, is worked out again
 */
void asm_restart(struct assembler *a);

void asm_init(struct assembler *a);

/*
 * assemble one line. false on anything it does not understand, with
 * `error` saying what and `error_line` saying where, an assembler
 * that fails without naming the line is one nobody can use
 */
bool asm_line(struct assembler *a, const char *text, int line);

/* fill in every forward reference. */
bool asm_finish(struct assembler *a);

#endif
