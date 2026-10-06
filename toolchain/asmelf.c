// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmelf.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * writing an object file.
 */

#include "asmelf.h"
#include <string.h>
#include <stdio.h>

#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4

#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STT_NOTYPE  0
#define STT_SECTION 3

#define R_X86_64_64   1
#define R_X86_64_PC32 2

struct sym64 {
    uint32_t name;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
};

struct rela64 {
    uint64_t offset;
    uint64_t info;      /* symbol index << 32 | type */
    int64_t  addend;
};

struct shdr64 {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t entsize;
};

/*
 * a growable blob, because every table here is built by appending and
 * the sizes are not known until it is done
 */
struct blob {
    uint8_t *at;
    size_t   len, cap;
};

static bool blob_add(struct blob *b, const void *data, size_t n)
{
    if (b->len + n > b->cap) {
        return false;
    }
    memcpy(b->at + b->len, data, n);
    b->len += n;
    return true;
}

static uint32_t str_add(struct blob *b, const char *s)
{
    uint32_t at = (uint32_t)b->len;
    blob_add(b, s, strlen(s) + 1);
    return at;
}

bool asmelf_write(const struct assembler *a, uint8_t *out, size_t max,
                  size_t *written, char *error, size_t error_max)
{
    /*
     * the working tables. sized rather than allocated: an object file
     * from one assembly is small, and a fixed ceiling that says so
     * beats an allocator that can fail half way through writing
     */
    static uint8_t strtab_mem[16 * 1024];
    static uint8_t shstr_mem[256];
    static struct sym64  syms[ASM_MAX_LABELS + 8];
    static struct rela64 relas[ASM_MAX_PATCHES];
    /*
     * a relocation belongs to the section whose bytes it patches, and
     * there is one relocation section per patched section, `sh_info`
     * on it says which. putting them all in .rela.text was invisible
     * for a while: the comparison against nasm counted relocations
     * and got the same *total*, since two in .rela.text and two in
     * .rela.rodata is four either way. what it costs is a .rodata
     * relocation applied at that offset in .text, which is code
     * overwritten with an address
     */
    static struct rela64 relas_ro[ASM_MAX_PATCHES];
    size_t nrelas_ro = 0;

    struct blob strtab = { strtab_mem, 0, sizeof strtab_mem };
    struct blob shstr  = { shstr_mem, 0, sizeof shstr_mem };
    size_t nsyms = 0, nrelas = 0;

    /*
     * index 0 of a string table is an empty name, by definition, a
     * symbol with name 0 is a symbol with no name
     */
    str_add(&strtab, "");
    str_add(&shstr, "");
    uint32_t n_text   = str_add(&shstr, ".text");
    uint32_t n_symtab = str_add(&shstr, ".symtab");
    uint32_t n_strtab = str_add(&shstr, ".strtab");
    uint32_t n_rela   = str_add(&shstr, ".rela.text");
    uint32_t n_rela_ro = str_add(&shstr, ".rela.rodata");
    uint32_t n_shstr  = str_add(&shstr, ".shstrtab");
    uint32_t n_rodata = str_add(&shstr, ".rodata");

    /* symbol 0 is the null symbol, likewise by definition */
    memset(&syms[nsyms++], 0, sizeof syms[0]);

    /*
     * then a symbol for the section itself, which is what a relocation
     * against a *local* label refers to
     */
    struct sym64 sec = { 0 };
    sec.info  = (STB_LOCAL << 4) | STT_SECTION;
    sec.shndx = 1;      /* .text */
    syms[nsyms++] = sec;
    size_t text_sym = 1;

    sec.shndx = 6;      /* .rodata */
    syms[nsyms++] = sec;
    size_t rodata_sym = 2;

    /* every label defined here, and every name that is not. */
    size_t sym_index[ASM_MAX_LABELS];
    for (size_t i = 0; i < a->labels; i++) {
        sym_index[i] = 0;
    }

    for (size_t pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < a->labels; i++) {
            /*
             * two different reasons to be global, and only one of them
             * is about this file not defining the name. `global foo`
             * says the opposite, it is defined here, and meant for
             * somewhere else
             */
            bool global = !a->label[i].defined || a->label[i].exported;
            bool undefined = !a->label[i].defined;
            if ((pass == 0) == global) {
                continue;   /* locals first, then globals */
            }
            if (nsyms >= sizeof syms / sizeof syms[0]) {
                snprintf(error, error_max, "too many symbols");
                return false;
            }
            struct sym64 s = { 0 };
            s.name  = str_add(&strtab, a->label[i].name);
            s.info  = (uint8_t)((global ? STB_GLOBAL : STB_LOCAL) << 4)
                    | STT_NOTYPE;
            /* which section it is in. */
            s.shndx = undefined ? 0
                    : (a->label[i].section == ASM_TEXT ? 1 : 6);
            s.value = undefined ? 0 : a->label[i].at;
            sym_index[i] = nsyms;
            syms[nsyms++] = s;
        }
    }
    size_t first_global = 0;
    for (size_t i = 0; i < nsyms; i++) {
        if ((syms[i].info >> 4) == STB_GLOBAL) {
            first_global = i;
            break;
        }
        first_global = i + 1;
    }

    /* two kinds, and the second is the one that is easy to get wrong. */
    for (size_t i = 0; i < a->patches; i++) {
        const struct asm_patch *p = &a->patch[i];

        if (!p->unresolved && p->size == 8 && p->next == 0) {
            uint64_t value = 0;
            size_t   in_sec = text_sym;
            for (size_t j = 0; j < a->labels; j++) {
                if (strcmp(a->label[j].name, p->name) == 0
                    && a->label[j].defined) {
                    value  = a->label[j].at;
                    in_sec = (a->label[j].section == ASM_TEXT)
                           ? text_sym : rodata_sym;
                    break;
                }
            }
            struct rela64 r;
            r.offset = p->at;
            r.info   = ((uint64_t)in_sec << 32) | R_X86_64_64;
            r.addend = (int64_t)value;
            if (p->section == ASM_RODATA) {
                relas_ro[nrelas_ro++] = r;
            } else {
                relas[nrelas++] = r;
            }

            /* and the slot itself goes out empty */
            uint8_t *slot = (p->section == ASM_TEXT)
                          ? (uint8_t *)&a->out[ASM_TEXT][p->at]
                          : (uint8_t *)&a->out[ASM_RODATA][p->at];
            memset(slot, 0, 8);
            continue;
        }

        if (!p->unresolved) {
            continue;       /* a relative jump, already measured */
        }
        if (p->size == 2) {
            /* a sixteen-bit reference to a name this file does not define. */
            snprintf(error, error_max,
                     "line %d: %s is 16-bit and defined somewhere else, "
                     "which an object file cannot say", p->line, p->name);
            return false;
        }
        size_t which = 0;
        for (size_t j = 0; j < a->labels; j++) {
            if (strcmp(a->label[j].name, p->name) == 0) {
                which = sym_index[j];
                break;
            }
        }
        if (which == 0) {
            which = text_sym;
        }

        struct rela64 r;
        r.offset = p->at;
        if (p->size == 8) {
            r.info   = ((uint64_t)which << 32) | R_X86_64_64;
            r.addend = 0;
        } else {
            /*
             * the addend is -4 because the processor measures from the
             * end of the instruction and the field is its last four
             * bytes. this is the number that, got wrong, gives a call
             * landing four bytes past its target
             */
            r.info   = ((uint64_t)which << 32) | R_X86_64_PC32;
            r.addend = -4;
        }
        if (p->section == ASM_RODATA) {
            relas_ro[nrelas_ro++] = r;
        } else {
            relas[nrelas++] = r;
        }
    }


    size_t at = 64;                         /* past the ELF header */
    size_t text_off = at;   at += a->len[ASM_TEXT];
    at = (at + 7) & ~(size_t)7;
    size_t symtab_off = at; at += nsyms * sizeof(struct sym64);
    size_t strtab_off = at; at += strtab.len;
    at = (at + 7) & ~(size_t)7;
    size_t rela_off = at;   at += nrelas * sizeof(struct rela64);
    size_t rela_ro_off = at; at += nrelas_ro * sizeof(struct rela64);
    size_t shstr_off = at;  at += shstr.len;
    at = (at + 7) & ~(size_t)7;
    size_t rodata_off = at; at += a->len[ASM_RODATA];
    at = (at + 7) & ~(size_t)7;
    size_t sections = nrelas_ro > 0 ? 8 : 7;
    size_t shdr_off = at;   at += sections * sizeof(struct shdr64);

    if (at > max) {
        snprintf(error, error_max, "the object file is too large");
        return false;
    }
    memset(out, 0, at);

    /*
     * the section symbols moved everything along by one, so a symbol
     * index recorded earlier would now name the wrong thing
     */
    (void)rodata_sym;

    /* the header */
    uint8_t *e = out;
    e[0] = 0x7f; e[1] = 'E'; e[2] = 'L'; e[3] = 'F';
    e[4] = 2;       /* 64-bit */
    e[5] = 1;       /* little-endian */
    e[6] = 1;       /* version */
    *(uint16_t *)(e + 16) = 1;      /* ET_REL: a thing to be linked */
    *(uint16_t *)(e + 18) = 0x3e;   /* x86-64 */
    *(uint32_t *)(e + 20) = 1;
    *(uint64_t *)(e + 40) = shdr_off;
    *(uint16_t *)(e + 52) = 64;
    *(uint16_t *)(e + 58) = sizeof(struct shdr64);
    *(uint16_t *)(e + 60) = (uint16_t)sections;
    *(uint16_t *)(e + 62) = 5;      /* which one holds their names */

    memcpy(out + text_off, a->out[ASM_TEXT], a->len[ASM_TEXT]);
    memcpy(out + symtab_off, syms, nsyms * sizeof syms[0]);
    memcpy(out + strtab_off, strtab.at, strtab.len);
    memcpy(out + rela_off, relas, nrelas * sizeof relas[0]);
    memcpy(out + rela_ro_off, relas_ro, nrelas_ro * sizeof relas_ro[0]);
    memcpy(out + shstr_off, shstr.at, shstr.len);
    memcpy(out + rodata_off, a->out[ASM_RODATA], a->len[ASM_RODATA]);

    struct shdr64 *sh = (struct shdr64 *)(out + shdr_off);
    memset(sh, 0, sections * sizeof *sh);

    sh[1].name = n_text;   sh[1].type = SHT_PROGBITS;
    sh[1].flags = SHF_ALLOC | SHF_EXECINSTR;
    sh[1].offset = text_off; sh[1].size = a->len[ASM_TEXT]; sh[1].align = 16;

    sh[2].name = n_symtab; sh[2].type = SHT_SYMTAB;
    sh[2].offset = symtab_off; sh[2].size = nsyms * sizeof syms[0];
    sh[2].link = 3;                     /* the strings are in section 3 */
    sh[2].info = (uint32_t)first_global; /* where the globals begin */
    sh[2].align = 8; sh[2].entsize = sizeof(struct sym64);

    sh[3].name = n_strtab; sh[3].type = SHT_STRTAB;
    sh[3].offset = strtab_off; sh[3].size = strtab.len; sh[3].align = 1;

    sh[4].name = n_rela;   sh[4].type = SHT_RELA;
    sh[4].offset = rela_off; sh[4].size = nrelas * sizeof relas[0];
    sh[4].link = 2;                     /* the symbols it names */
    sh[4].info = 1;                     /* the section it patches */
    sh[4].align = 8; sh[4].entsize = sizeof(struct rela64);

    sh[5].name = n_shstr;  sh[5].type = SHT_STRTAB;
    sh[5].offset = shstr_off; sh[5].size = shstr.len; sh[5].align = 1;

    /* the read-only data, which for this kernel is a table of 256 addresses. */
    sh[6].name = n_rodata; sh[6].type = SHT_PROGBITS;
    sh[6].flags = SHF_ALLOC;
    sh[6].offset = rodata_off; sh[6].size = a->len[ASM_RODATA];
    sh[6].align = 8;

    /*
     * and .rodata's own relocations, when it has any, a table of
     * labels is nothing else
     */
    if (nrelas_ro > 0) {
        sh[7].name = n_rela_ro; sh[7].type = SHT_RELA;
        sh[7].offset = rela_ro_off;
        sh[7].size = nrelas_ro * sizeof relas_ro[0];
        sh[7].link = 2;
        sh[7].info = 6;                 /* the section it patches */
        sh[7].align = 8; sh[7].entsize = sizeof(struct rela64);
    }

    *written = at;
    return true;
}
