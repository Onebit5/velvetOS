// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmlib.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the assembler's instruction encoding, and the helpers around it.
 */

#include "asmlib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* the number is what goes in the encoding, and the *fifth bit* of it goes in REX. */
static const char *reg64[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8","r9","r10","r11","r12","r13","r14","r15"
};
static const char *reg32[16] = {
    "eax","ecx","edx","ebx","esp","ebp","esi","edi",
    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"
};
static const char *reg16[16] = {
    "ax","cx","dx","bx","sp","bp","si","di",
    "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"
};
static const char *reg8[16] = {
    "al","cl","dl","bl","spl","bpl","sil","dil",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"
};

/* which table a name is in, and therefore how wide the operation is */
static int find_reg(const char *s, int *width)
{
    for (int i = 0; i < 16; i++) {
        if (strcmp(s, reg64[i]) == 0) { *width = 64; return i; }
        if (strcmp(s, reg32[i]) == 0) { *width = 32; return i; }
        if (strcmp(s, reg16[i]) == 0) { *width = 16; return i; }
        if (strcmp(s, reg8[i])  == 0) { *width = 8;  return i; }
    }
    return -1;
}



static void put8(struct assembler *a, uint8_t b)
{
    if (a->len[a->sec] < ASM_MAX_OUTPUT) {
        a->out[a->sec][a->len[a->sec]++] = b;
    }
}
static void put16(struct assembler *a, uint16_t v)
{
    put8(a, (uint8_t)v); put8(a, (uint8_t)(v >> 8));
}
static void put32(struct assembler *a, uint32_t v)
{
    put16(a, (uint16_t)v); put16(a, (uint16_t)(v >> 16));
}
static void put64(struct assembler *a, uint64_t v)
{
    put32(a, (uint32_t)v); put32(a, (uint32_t)(v >> 32));
}

static bool fail(struct assembler *a, int line, const char *what,
                 const char *detail)
{
    if (a->error[0] == '\0') {
        snprintf(a->error, sizeof a->error, "%s%s%s", what,
                 detail ? ": " : "", detail ? detail : "");
        a->error_line = line;
    }
    return false;
}

/* REX, emitted only when it is needed. */
static void rex(struct assembler *a, bool w, int r, int x, int b,
                bool force)
{
    uint8_t v = 0x40;
    if (w) { v |= 0x08; }
    if (r & 8) { v |= 0x04; }
    if (x & 8) { v |= 0x02; }
    if (b & 8) { v |= 0x01; }
    if (v != 0x40 || force) {
        put8(a, v);
    }
}

/*
 * the operand-size prefix, when the width asked for is not the one the
 * current mode gives for free
 */
static void osize(struct assembler *a, int width)
{
    int natural = (a->bits == 16) ? 16 : 32;
    if ((width == 16 || width == 32) && width != natural) {
        put8(a, 0x66);
    }
}

static void modrm(struct assembler *a, int mod, int reg, int rm)
{
    put8(a, (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7)));
}



struct parts {
    char op[64];
    char arg[4][64];
    int  count;
};

/*
 * whitespace, including the line ending, which `fgets` leaves on and
 * which cost the first run of this: the mnemonic came out as "ret\n"
 * and matched nothing in any table
 */
static bool blank(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool split(const char *text, struct parts *p)
{
    memset(p, 0, sizeof *p);

    while (blank(*text)) { text++; }

    /* a comment runs to the end, and a line may be nothing else */
    size_t n = 0;
    while (*text && !blank(*text) && *text != ';'
           && n + 1 < sizeof p->op) {
        p->op[n++] = *text++;
    }
    p->op[n] = '\0';
    if (p->op[0] == '\0' || p->op[0] == ';') {
        return true;        /* a blank line, which is not an error */
    }

    while (*text) {
        while (blank(*text) || *text == ',') { text++; }
        if (*text == '\0' || *text == ';') { break; }

        if (p->count >= 4) { break; }
        char *into = p->arg[p->count];
        size_t at = 0;

        /* a bracketed operand keeps its brackets and its spaces out */
        while (*text && *text != ',' && *text != ';'
               && at + 1 < sizeof p->arg[0]) {
            if (!blank(*text)) {
                into[at++] = *text;
            }
            text++;
        }
        into[at] = '\0';
        if (at > 0) { p->count++; }
    }
    return true;
}

/* assembly is case-insensitive, but what is inside quotes is *data*. */
static void lower(char *s)
{
    char quote = 0;
    for (; *s; s++) {
        if (quote != 0) {
            if (*s == quote) { quote = 0; }
            continue;
        }
        if (*s == '\'' || *s == '"') { quote = *s; continue; }
        if (*s >= 'A' && *s <= 'Z') { *s = (char)(*s + 32); }
    }
}

/* `(1<<31)|1`, `TRAMP_BASE+0x0f00`, `gdt_end-gdt`. */
struct vscan {
    const char        *at;
    struct assembler  *a;
    bool               ok;
    bool               known;
};

static uint64_t v_or(struct vscan *s);

static void v_blank(struct vscan *s)
{
    while (*s->at == ' ' || *s->at == '\t') { s->at++; }
}

static uint64_t v_atom(struct vscan *s)
{
    v_blank(s);

    if (*s->at == '(') {
        s->at++;
        uint64_t v = v_or(s);
        v_blank(s);
        if (*s->at == ')') { s->at++; } else { s->ok = false; }
        return v;
    }
    if (*s->at == '-') {
        s->at++;
        return (uint64_t)(-(int64_t)v_atom(s));
    }
    if (*s->at == '~') {
        s->at++;
        return ~v_atom(s);
    }

    /*
     * a character, which in an assembly file is just a number written
     * so a person can read it: `db 'A'` and `db 0x41` are the same byte
     */
    if (*s->at == '\'' || *s->at == '"') {
        char quote = *s->at++;
        uint64_t v = 0;
        int shift = 0;
        while (*s->at && *s->at != quote && shift < 64) {
            v |= (uint64_t)(unsigned char)*s->at++ << shift;
            shift += 8;
        }
        if (*s->at == quote) { s->at++; } else { s->ok = false; }
        return v;
    }

    if (*s->at >= '0' && *s->at <= '9') {
        char *end = NULL;
        uint64_t v;
        if (s->at[0] == '0' && (s->at[1] == 'x' || s->at[1] == 'X')) {
            v = (uint64_t)strtoull(s->at + 2, &end, 16);
        } else {
            v = (uint64_t)strtoull(s->at, &end, 0);
        }
        s->at = end;
        return v;
    }

    char name[80];
    size_t n = 0;
    while ((*s->at >= 'a' && *s->at <= 'z') || (*s->at >= 'A' && *s->at <= 'Z')
        || (*s->at >= '0' && *s->at <= '9') || *s->at == '_' || *s->at == '.') {
        if (n + 1 < sizeof name) { name[n++] = *s->at; }
        s->at++;
    }
    name[n] = '\0';
    if (n == 0) {
        s->ok = false;
        return 0;
    }

    for (size_t i = 0; i < s->a->labels; i++) {
        if (strcmp(s->a->label[i].name, name) == 0 && s->a->label[i].defined) {
            return s->a->label[i].at;
        }
    }
    s->known = false;   /* a label defined later, or somebody else's */
    return 0;
}

static uint64_t v_mul(struct vscan *s)
{
    uint64_t v = v_atom(s);
    for (;;) {
        v_blank(s);
        if (*s->at == '*') { s->at++; v *= v_atom(s); }
        else if (*s->at == '/') {
            s->at++;
            uint64_t d = v_atom(s);
            v = (d != 0) ? v / d : 0;
        } else { return v; }
    }
}

static uint64_t v_add(struct vscan *s)
{
    uint64_t v = v_mul(s);
    for (;;) {
        v_blank(s);
        if (*s->at == '+') { s->at++; v += v_mul(s); }
        else if (*s->at == '-') { s->at++; v -= v_mul(s); }
        else { return v; }
    }
}

static uint64_t v_shift(struct vscan *s)
{
    uint64_t v = v_add(s);
    for (;;) {
        v_blank(s);
        if (s->at[0] == '<' && s->at[1] == '<') { s->at += 2; v <<= v_add(s); }
        else if (s->at[0] == '>' && s->at[1] == '>') { s->at += 2; v >>= v_add(s); }
        else { return v; }
    }
}

static uint64_t v_and(struct vscan *s)
{
    uint64_t v = v_shift(s);
    for (;;) {
        v_blank(s);
        if (*s->at == '&' && s->at[1] != '&') { s->at++; v &= v_shift(s); }
        else { return v; }
    }
}

static uint64_t v_or(struct vscan *s)
{
    uint64_t v = v_and(s);
    for (;;) {
        v_blank(s);
        if (*s->at == '|' && s->at[1] != '|') { s->at++; v |= v_and(s); }
        else if (*s->at == '^') { s->at++; v ^= v_and(s); }
        else { return v; }
    }
}

static bool value_of(struct assembler *a, const char *s, uint64_t *out,
                     bool *known)
{
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    struct vscan sc = { s, a, true, true };
    uint64_t v = v_or(&sc);
    v_blank(&sc);

    if (!sc.ok || *sc.at != '\0') {
        return false;   /* something in it was not part of a value */
    }
    *out = v;
    *known = sc.known;
    return true;
}

static bool define_label(struct assembler *a, const char *name, int line)
{
    for (size_t i = 0; i < a->labels; i++) {
        if (strcmp(a->label[i].name, name) == 0) {
            if (a->label[i].defined) {
                return fail(a, line, "that label is already defined", name);
            }
            a->label[i].at = a->origin + a->len[a->sec];
            a->label[i].defined = true;
            a->label[i].section = a->sec;
            return true;
        }
    }
    if (a->labels >= ASM_MAX_LABELS) {
        return fail(a, line, "too many labels", NULL);
    }
    struct asm_label *l = &a->label[a->labels++];
    memset(l, 0, sizeof *l);
    snprintf(l->name, sizeof l->name, "%s", name);
    l->at = a->origin + a->len[a->sec];
    l->defined = true;
    l->section = a->sec;
    return true;
}

static void want_patch(struct assembler *a, const char *name, uint8_t size,
                       int line)
{
    /* not a branch unless the caller says so */
    if (a->patches >= ASM_MAX_PATCHES) {
        return;
    }
    struct asm_patch *p = &a->patch[a->patches++];
    p->at   = a->len[a->sec];
    p->section = a->sec;
    p->size = size;
    p->name[0] = '\0';
    snprintf(p->name, sizeof p->name, "%s", name);
    p->line = line;
    p->jump = -1;
    p->unresolved = false;
    /*
     * `next` is filled in by the caller once it knows where the
     * instruction ends, a relative jump is measured from the byte
     * after it, not from the field
     */
}

/* the splitter takes the spaces out of an operand, so that `[rsp + 8]` arrives as `[rsp+8]`. */
static const char *drop_size(const char *s, int *width)
{
    static const struct { const char *word; int w; } sizes[] = {
        { "qword", 64 }, { "dword", 32 }, { "word", 16 }, { "byte", 8 },
    };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        size_t n = strlen(sizes[i].word);
        if (strncmp(s, sizes[i].word, n) == 0) {
            if (width != NULL) { *width = sizes[i].w; }
            return s + n;
        }
    }
    return s;
}

/* `[rdi]`, `[rsp+8]`, `[gs:8]`, `[label]`. */
struct mem {
    bool     is_mem;
    int      base;      /* register number, or -1 for none */
    int      index;     /* the second register, or -1: sixteen-bit only */
    int64_t  disp;
    bool     has_disp;
    uint8_t  segment;   /* prefix byte, or 0 */
    char     label[80]; /* if the displacement is a name */
};

/* the register a term names, if the mode allows a register there at all. */
static int addr_reg(const struct assembler *a, const char *s)
{
    int w = 0;
    int r = find_reg(s, &w);
    if (r < 0) {
        return -1;
    }
    if (a->bits == 16) {
        return (w == 16 && (r == 3 || r == 5 || r == 6 || r == 7)) ? r : -2;
    }
    return (w == 64) ? r : -2;
}

static bool parse_mem(struct assembler *a, const char *s, struct mem *m)
{
    memset(m, 0, sizeof *m);
    m->base  = -1;
    m->index = -1;
    s = drop_size(s, NULL);

    size_t n = strlen(s);
    if (n < 3 || s[0] != '[' || s[n - 1] != ']') {
        return false;
    }

    char body[80];
    size_t len = n - 2;
    if (len >= sizeof body) {
        return false;
    }
    memcpy(body, s + 1, len);
    body[len] = '\0';

    /*
     * a segment override is a prefix byte and nothing else: the rest of
     * the operand is encoded exactly as it would have been
     */
    char *colon = strchr(body, ':');
    if (colon != NULL) {
        *colon = '\0';
        if (strcmp(body, "gs") == 0)      { m->segment = 0x65; }
        else if (strcmp(body, "fs") == 0) { m->segment = 0x64; }
        else if (strcmp(body, "es") == 0) { m->segment = 0x26; }
        else if (strcmp(body, "ds") == 0) { m->segment = 0x3e; }
        else if (strcmp(body, "cs") == 0) { m->segment = 0x2e; }
        else if (strcmp(body, "ss") == 0) { m->segment = 0x36; }
        else { return false; }
        memmove(body, colon + 1, strlen(colon + 1) + 1);
    }

    /* the terms, split at the + and - between them. */
    bool any_reg = false;
    for (char *term = body; term != NULL && *term != '\0'; ) {
        char *next = NULL;
        for (char *p = term + 1; *p; p++) {
            if (*p == '+' || *p == '-') { next = p; break; }
        }
        char one[80];
        const char *at = (*term == '+' || *term == '-') ? term + 1 : term;
        snprintf(one, sizeof one, "%.*s",
                 next ? (int)(next - at) : (int)strlen(at), at);
        int r = addr_reg(a, one);
        if (r == -2) {
            return false;   /* a register this mode cannot address through */
        }
        if (r >= 0) {
            any_reg = true;
        }
        term = next;
    }

    if (!any_reg) {
        uint64_t v;
        bool known;
        if (!value_of(a, body, &v, &known)) {
            return false;
        }
        if (!known) {
            snprintf(m->label, sizeof m->label, "%s", body);
        }
        m->disp = (int64_t)v;
        m->has_disp = true;
        m->is_mem = true;
        return true;
    }

    for (char *term = body; term != NULL && *term != '\0'; ) {
        bool negative = (*term == '-');
        if (*term == '+' || *term == '-') { term++; }

        char *next = NULL;
        for (char *p = term + 1; *p; p++) {
            if (*p == '+' || *p == '-') { next = p; break; }
        }

        char one[80];
        snprintf(one, sizeof one, "%.*s",
                 next ? (int)(next - term) : (int)strlen(term), term);

        int r = addr_reg(a, one);
        if (r >= 0) {
            if (negative) {
                return false;   /* a register subtracted is not an address */
            }
            if (m->base < 0) {
                m->base = r;
            } else if (m->index < 0 && a->bits == 16) {
                m->index = r;
            } else {
                return false;
            }
        } else {
            if (m->has_disp) {
                return false;   /* two displacements: not something to guess at */
            }
            uint64_t v;
            bool known;
            if (!value_of(a, one, &v, &known)) {
                return false;
            }
            if (!known) {
                snprintf(m->label, sizeof m->label, "%s", one);
            }
            m->disp = negative ? -(int64_t)v : (int64_t)v;
            m->has_disp = true;
        }

        term = next;
    }

    m->is_mem = true;
    return true;
}

/*
 * real mode does not encode addresses the same way, and it is not a
 * subset of the way that follows it: there is no SIB byte at all, only
 * four registers can appear in an address, and they appear in *pairs*
 * chosen from a table of eight rows rather than in any combination.
 *
 *   000 bx+si   001 bx+di   010 bp+si   011 bp+di
 *   100 si      101 di      110 bp      111 bx
 *
 * and the trap that gives this its own function: rm 110 with mod 00 is
 * not `[bp]`, it is a bare sixteen-bit address, which is the one this
 * kernel's trampoline uses on nearly every line, and the reason
 * `[bp]` has to be written with a displacement of zero.
 *
 * returns the rm field, or -1 if the pair is not one the table has
 */
static int rm16(const struct mem *m)
{
    static const struct { int base, index, rm; } table[] = {
        { 3, 6, 0 }, { 3, 7, 1 }, { 5, 6, 2 }, { 5, 7, 3 },
        { 6, -1, 4 }, { 7, -1, 5 }, { 5, -1, 6 }, { 3, -1, 7 },
        /* written either way round, since `[si+bx]` is the same address */
        { 6, 3, 0 }, { 7, 3, 1 }, { 6, 5, 2 }, { 7, 5, 3 },
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (table[i].base == m->base && table[i].index == m->index) {
            return table[i].rm;
        }
    }
    return -1;
}

static void mem_modrm16(struct assembler *a, const struct mem *m, int reg,
                        int line)
{
    if (m->base < 0) {
        modrm(a, 0, reg, 6);            /* the bare address, not [bp] */
        if (m->label[0] != '\0') {
            want_patch(a, m->label, 2, line);
        }
        put16(a, (uint16_t)m->disp);
        return;
    }

    int rm = rm16(m);
    if (rm < 0) {
        fail(a, line, "that pair of registers is not an address in real mode",
             NULL);
        return;
    }

    int mod;
    if ((!m->has_disp || m->disp == 0) && rm != 6) {
        mod = 0;
    } else if (m->disp >= -128 && m->disp <= 127 && m->label[0] == '\0') {
        mod = 1;
    } else {
        mod = 2;
    }

    modrm(a, mod, reg, rm);
    if (mod == 1) {
        put8(a, (uint8_t)m->disp);
    } else if (mod == 2) {
        if (m->label[0] != '\0') {
            want_patch(a, m->label, 2, line);
        }
        put16(a, (uint16_t)m->disp);
    }
}

/* write the ModRM (and SIB, and displacement) for a memory operand */
static void mem_modrm(struct assembler *a, const struct mem *m, int reg,
                      int line)
{
    if (a->bits == 16) {
        mem_modrm16(a, m, reg, line);
        return;
    }
    if (m->base < 0) {
        /*
         * no base at all: mod 00, rm 100, and a SIB that says "no index,
         * no base", which is the only way to write a bare address
         */
        modrm(a, 0, reg, 4);
        put8(a, 0x25);
        if (m->label[0] != '\0') {
            want_patch(a, m->label, 4, line);
        }
        put32(a, (uint32_t)m->disp);
        return;
    }

    int mod;
    if (!m->has_disp || m->disp == 0) {
        /* except for rbp, where mod 00 means something else entirely */
        mod = ((m->base & 7) == 5) ? 1 : 0;
    } else if (m->disp >= -128 && m->disp <= 127) {
        mod = 1;
    } else {
        mod = 2;
    }

    modrm(a, mod, reg, m->base);
    if ((m->base & 7) == 4) {
        /* rm 100 said "a SIB follows", so one must. */
        put8(a, (uint8_t)(0x20 | (m->base & 7)));
    }
    if (mod == 1) {
        put8(a, (uint8_t)m->disp);
    } else if (mod == 2) {
        put32(a, (uint32_t)m->disp);
    }
}

/* the accumulator's own way of naming an address. */
static bool moffs(struct assembler *a, const struct mem *m, int reg,
                  int width, bool storing, int line)
{
    if (a->bits != 16 || reg != 0 || m->base >= 0 || m->index >= 0) {
        return false;
    }
    if (width != 8 && width != 16 && width != 32) {
        return false;
    }
    if (m->segment) { put8(a, m->segment); }
    osize(a, width);
    put8(a, (uint8_t)((width == 8 ? 0xa0 : 0xa1) + (storing ? 2 : 0)));
    if (m->label[0] != '\0') {
        want_patch(a, m->label, 2, line);
    }
    put16(a, (uint16_t)m->disp);
    return true;
}

/* the hole a branch leaves behind, of the width the mode gives it. */
static void rel_patch(struct assembler *a, const char *name, int line)
{
    want_patch(a, name, (a->bits == 16) ? 2 : 4, line);
    if (a->bits == 16) {
        put16(a, 0);
    } else {
        put32(a, 0);
    }
}



static bool one_byte(struct assembler *a, const char *op)
{
    static const struct { const char *name; uint8_t b; } simple[] = {
        { "ret", 0xc3 }, { "nop", 0x90 }, { "cli", 0xfa }, { "sti", 0xfb },
        { "hlt", 0xf4 }, { "cld", 0xfc }, { "std", 0xfd }, { "leave", 0xc9 },
        { "pushfq", 0x9c }, { "popfq", 0x9d }, { "int3", 0xcc },
    };
    for (size_t i = 0; i < sizeof simple / sizeof simple[0]; i++) {
        if (strcmp(op, simple[i].name) == 0) {
            put8(a, simple[i].b);
            return true;
        }
    }
    static const struct { const char *name; uint8_t a, b; } two[] = {
        { "syscall", 0x0f, 0x05 }, { "sysretq", 0x0f, 0x07 },
        { "sysret", 0x0f, 0x07 },  { "iret", 0xcf, 0x90 },
        { "rdmsr", 0x0f, 0x32 },   { "wrmsr", 0x0f, 0x30 },
        { "cpuid", 0x0f, 0xa2 },
    };
    for (size_t i = 0; i < sizeof two / sizeof two[0]; i++) {
        if (strcmp(op, two[i].name) == 0) {
            put8(a, two[i].a);
            put8(a, two[i].b);
            return true;
        }
    }
    if (strcmp(op, "swapgs") == 0) {
        /*
         * three bytes, not two: it lives in the 0f 01 group with a
         * ModRM byte that is part of the opcode rather than naming any
         * operand. the two-byte guess assembles to something else
         * entirely, which is exactly the class of mistake the byte
         * comparison exists to catch
         */
        put8(a, 0x0f);
        put8(a, 0x01);
        put8(a, 0xf8);
        return true;
    }
    if (strcmp(op, "iretq") == 0) {
        /*
         * the only one that needs REX.W to mean what it says: without
         * it this is a 32-bit return, which in long mode pops the wrong
         * number of things off the stack
         */
        put8(a, 0x48);
        put8(a, 0xcf);
        return true;
    }
    return false;
}

/*
 * the group of two-operand instructions that share an encoding shape,
 * differing only in which opcode and which /digit they use
 */
struct alu {
    const char *name;
    uint8_t rm_r;       /* op r/m, r */
    uint8_t digit;      /* for the immediate form */
};
static const struct alu alu[] = {
    { "add", 0x01, 0 }, { "or",  0x09, 1 }, { "adc", 0x11, 2 },
    { "sbb", 0x19, 3 }, { "and", 0x21, 4 }, { "sub", 0x29, 5 },
    { "xor", 0x31, 6 }, { "cmp", 0x39, 7 },
};

static bool encode(struct assembler *a, struct parts *p, int line);

static bool encode(struct assembler *a, struct parts *p, int line)
{
    const char *op = p->op;

    /*
     * `o64 sysret` is `sysret` with a REX.W in front, and nasm writes
     * it that way because the mnemonic alone means the 32-bit form. it
     * is a prefix rather than an instruction, so the rest of the line
     * is encoded after it.
     *
     * `o32` is the same idea one size down, and it is not decoration
     * either: `o32 lgdt` in real mode loads a 32-bit base and plain
     * `lgdt` loads twenty-four bits of one, so a trampoline whose gdt
     * lives above 16MB and whose assembler dropped the prefix loads a
     * descriptor table from an address that is a quarter of the one it
     * wrote down
     */
    if (strcmp(op, "o64") == 0 || strcmp(op, "o32") == 0
     || strcmp(op, "o16") == 0) {
        if (p->count < 1) {
            return fail(a, line, "a prefix with nothing after it", op);
        }
        if (op[1] == '6' && op[2] == '4') {
            put8(a, 0x48);
        } else {
            osize(a, (op[1] == '3') ? 32 : 16);
        }
        struct parts rest;
        memset(&rest, 0, sizeof rest);
        for (int i = 1; i < p->count; i++) {
            memcpy(rest.arg[i - 1], p->arg[i], sizeof rest.arg[0]);
        }
        rest.count = p->count - 1;

        /*
         * what follows the prefix may have arrived glued together:
         * `o32 lgdt [gdt_ptr]` has no comma in it, so the splitter took
         * `lgdt [gdt_ptr]` for one operand and removed the space. the
         * bracket is where the mnemonic ends
         */
        const char *bracket = strchr(p->arg[0], '[');
        if (bracket != NULL && rest.count == 0) {
            size_t n = (size_t)(bracket - p->arg[0]);
            if (n >= sizeof rest.op) { n = sizeof rest.op - 1; }
            memcpy(rest.op, p->arg[0], n);
            rest.op[n] = '\0';
            snprintf(rest.arg[0], sizeof rest.arg[0], "%s", bracket);
            rest.count = 1;
        } else {
            snprintf(rest.op, sizeof rest.op, "%s", p->arg[0]);
        }
        return encode(a, &rest, line);
    }

    if (one_byte(a, op)) {
        return true;
    }

    int w1 = 0, w2 = 0;
    int r1 = (p->count > 0) ? find_reg(p->arg[0], &w1) : -1;
    int r2 = (p->count > 1) ? find_reg(p->arg[1], &w2) : -1;


    if (strcmp(op, "push") == 0 || strcmp(op, "pop") == 0) {
        bool pushing = (op[1] == 'u');
        if (r1 >= 0 && (w1 == 64 || w1 == 16)) {
            /*
             * no REX.W: push and pop are 64-bit in long mode and cannot
             * be anything else, so the W bit would be a byte that says
             * nothing, and nasm does not emit it. sixteen bits *is*
             * expressible, with the operand-size prefix, and the
             * trampoline uses it in real mode
             */
            osize(a, w1);
            if (r1 & 8) { put8(a, 0x41); }
            put8(a, (uint8_t)((pushing ? 0x50 : 0x58) + (r1 & 7)));
            return true;
        }
        if (pushing && p->count == 1) {
            uint64_t v; bool known;
            /* `push qword 0` arrives as `qword0`, the splitter having taken the space out. */
            const char *what = drop_size(p->arg[0], NULL);
            if (value_of(a, what, &v, &known) && known) {
                if ((int64_t)v >= -128 && (int64_t)v <= 127) {
                    put8(a, 0x6a);
                    put8(a, (uint8_t)v);
                } else {
                    put8(a, 0x68);
                    put32(a, (uint32_t)v);
                }
                return true;
            }
        }
        struct mem m;
        if (parse_mem(a, p->arg[0], &m)) {
            /*
             * the odd pair: push is ff /6 and pop is 8f /0, and neither
             * takes a REX.W, they are 64-bit in long mode and cannot
             * be told to be anything else
             */
            if (m.segment) { put8(a, m.segment); }
            if (m.base >= 0 && (m.base & 8)) { put8(a, 0x41); }
            put8(a, pushing ? 0xff : 0x8f);
            mem_modrm(a, &m, pushing ? 6 : 0, line);
            return true;
        }
        return fail(a, line, "cannot push or pop that", p->arg[0]);
    }


    if (strcmp(op, "mov") == 0 && p->count == 2) {
        /* the control and segment registers are looked at *first*, and that is not tidiness. */
        {
            static const char *crs[9] = { "cr0", "cr1", "cr2", "cr3", "cr4",
                                          "cr5", "cr6", "cr7", "cr8" };
            for (int i = 0; i < 9; i++) {
                /*
                 * no REX.W and no 0x66 whatever the register named: a
                 * move to or from a control register is 64-bit in long
                 * mode and 32-bit below it, and there is no encoding
                 * that says otherwise
                 */
                if (strcmp(p->arg[0], crs[i]) == 0 && r2 >= 0) {
                    if (r2 & 8) { put8(a, 0x41); }
                    put8(a, 0x0f);
                    put8(a, 0x22);
                    modrm(a, 3, i, r2);
                    return true;
                }
                if (strcmp(p->arg[1], crs[i]) == 0 && r1 >= 0) {
                    if (r1 & 8) { put8(a, 0x41); }
                    put8(a, 0x0f);
                    put8(a, 0x20);
                    modrm(a, 3, i, r1);
                    return true;
                }
            }
        }

        /*
         * `mov ds, ax`. they are not general registers and have an
         * opcode of their own, 8e to load one, 8c to read it, with
         * the *segment* number in ModRM.reg. a real-mode trampoline
         * sets all of them, so this is not optional for the file this
         * assembler exists to build
         */
        {
            static const char *segs[6] = { "es", "cs", "ss", "ds", "fs", "gs" };
            for (int i = 0; i < 6; i++) {
                if (strcmp(p->arg[0], segs[i]) == 0 && r2 >= 0) {
                    put8(a, 0x8e);
                    modrm(a, 3, i, r2);
                    return true;
                }
                if (strcmp(p->arg[1], segs[i]) == 0 && r1 >= 0) {
                    put8(a, 0x8c);
                    modrm(a, 3, i, r1);
                    return true;
                }
            }
        }

        if (r1 >= 0 && r2 >= 0) {
            if (w1 != w2) {
                return fail(a, line, "those registers are different sizes",
                            p->arg[0]);
            }
            osize(a, w1);
            rex(a, w1 == 64, r2, 0, r1, w1 == 8 && (r1 >= 4 || r2 >= 4));
            put8(a, (w1 == 8) ? 0x88 : 0x89);
            modrm(a, 3, r2, r1);
            return true;
        }
        /*
         * memory before immediates: `[rdi]` is not a number, so the
         * immediate path would take it for a label defined later and
         * quietly assemble a `mov rax, <address>` with a hole in it
         */
        struct mem mm;
        if (r1 >= 0 && parse_mem(a, p->arg[1], &mm)) {
            if (moffs(a, &mm, r1, w1, false, line)) { return true; }
            if (mm.segment) { put8(a, mm.segment); }
            osize(a, w1);
            rex(a, w1 == 64, r1, 0, (mm.base >= 0) ? mm.base : 0, false);
            put8(a, (w1 == 8) ? 0x8a : 0x8b);
            mem_modrm(a, &mm, r1, line);
            return true;
        }
        if (r2 >= 0 && parse_mem(a, p->arg[0], &mm)) {
            if (moffs(a, &mm, r2, w2, true, line)) { return true; }
            if (mm.segment) { put8(a, mm.segment); }
            osize(a, w2);
            rex(a, w2 == 64, r2, 0, (mm.base >= 0) ? mm.base : 0, false);
            put8(a, (w2 == 8) ? 0x88 : 0x89);
            mem_modrm(a, &mm, r2, line);
            return true;
        }

        /*
         * an immediate straight into memory: c6 for a byte and c7 for
         * the rest, with the width taken from the size word, because
         * `[dap_lba+4]` says nothing about how much of it is being
         * written and a guess here is three bytes of somebody else's
         * data zeroed
         */
        {
            int mw = 0;
            const char *sized = drop_size(p->arg[0], &mw);
            (void)sized;
            if (mw != 0 && parse_mem(a, p->arg[0], &mm)) {
                uint64_t v; bool known;
                if (!value_of(a, p->arg[1], &v, &known) || !known) {
                    return fail(a, line, "not a value", p->arg[1]);
                }
                if (mm.segment) { put8(a, mm.segment); }
                osize(a, mw);
                rex(a, mw == 64, 0, 0, (mm.base >= 0) ? mm.base : 0, false);
                put8(a, (mw == 8) ? 0xc6 : 0xc7);
                mem_modrm(a, &mm, 0, line);
                if (mw == 8) {
                    put8(a, (uint8_t)v);
                } else if (mw == 16) {
                    put16(a, (uint16_t)v);
                } else {
                    put32(a, (uint32_t)v);
                }
                return true;
            }
        }

        if (r1 >= 0) {
            uint64_t v; bool known;
            if (!value_of(a, p->arg[1], &v, &known)) {
                return fail(a, line, "not a value", p->arg[1]);
            }

            /*
             * nasm's choice, and the reason matching it is the test:
             * writing a 32-bit register zeroes the upper half, so
             * `mov rax,1` is assembled as `mov eax,1`, five bytes
             * instead of ten. emitting the long form is *correct* and
             * does not match, which is exactly the class of difference
             * reading the code would never reveal
             */
            if (w1 == 64 && known && v <= 0xffffffffull) {
                put8(a, (uint8_t)(0xb8 + (r1 & 7)));
                if (r1 & 8) {
                    /*
                     * except that r8-r15 still need the REX bit, which
                     * has to come *before* the opcode
                     */
                    a->len[a->sec]--;
                    put8(a, 0x41);
                    put8(a, (uint8_t)(0xb8 + (r1 & 7)));
                }
                put32(a, (uint32_t)v);
                return true;
            }
            if (w1 == 64) {
                rex(a, true, 0, 0, r1, false);
                put8(a, (uint8_t)(0xb8 + (r1 & 7)));
                if (!known) {
                    want_patch(a, p->arg[1], 8, line);
                    a->patch[a->patches - 1].next = 0;   /* absolute */
                }
                put64(a, v);
                return true;
            }
            if (w1 == 32) {
                /*
                 * the prefix is the mode's, not the register's: in real
                 * mode a 32-bit immediate into a 32-bit register is the
                 * *unnatural* width and needs 0x66 in front of it. this
                 * is `mov ecx, 0xc0000080` in the trampoline, which
                 * without the prefix loads cx and leaves rdmsr reading
                 * whichever msr the top half happened to name
                 */
                osize(a, 32);
                if (r1 & 8) { put8(a, 0x41); }
                put8(a, (uint8_t)(0xb8 + (r1 & 7)));
                put32(a, (uint32_t)v);
                return true;
            }
            if (w1 == 16) {
                /* a sixteen-bit immediate. */
                osize(a, 16);
                if (r1 & 8) { put8(a, 0x41); }
                put8(a, (uint8_t)(0xb8 + (r1 & 7)));
                put16(a, (uint16_t)v);
                return true;
            }
            if (w1 == 8) {
                if (r1 >= 4) { rex(a, false, 0, 0, r1, true); }
                put8(a, (uint8_t)(0xb0 + (r1 & 7)));
                put8(a, (uint8_t)v);
                return true;
            }
        }
        return fail(a, line, "cannot move that", p->arg[0]);
    }


    for (size_t i = 0; i < sizeof alu / sizeof alu[0]; i++) {
        if (strcmp(op, alu[i].name) != 0) {
            continue;
        }
        if (r1 >= 0 && r2 >= 0 && w1 == w2) {
            osize(a, w1);
            rex(a, w1 == 64, r2, 0, r1, false);
            put8(a, (uint8_t)(alu[i].rm_r - (w1 == 8 ? 1 : 0)));
            modrm(a, 3, r2, r1);
            return true;
        }
        /*
         * against memory. the same opcode plus two reverses the
         * direction, `add r/m, r` is 01 and `add r, r/m` is 03, so
         * the whole group gets both forms from one table
         */
        struct mem am;
        if (r1 >= 0 && p->count == 2 && parse_mem(a, p->arg[1], &am)) {
            if (am.segment) { put8(a, am.segment); }
            osize(a, w1);
            rex(a, w1 == 64, r1, 0, (am.base >= 0) ? am.base : 0, false);
            put8(a, (uint8_t)(alu[i].rm_r + 2 - (w1 == 8 ? 1 : 0)));
            mem_modrm(a, &am, r1, line);
            return true;
        }
        if (r2 >= 0 && p->count == 2 && parse_mem(a, p->arg[0], &am)) {
            if (am.segment) { put8(a, am.segment); }
            osize(a, w2);
            rex(a, w2 == 64, r2, 0, (am.base >= 0) ? am.base : 0, false);
            put8(a, (uint8_t)(alu[i].rm_r - (w2 == 8 ? 1 : 0)));
            mem_modrm(a, &am, r2, line);
            return true;
        }

        if (r1 >= 0 && p->count == 2) {
            uint64_t v; bool known;
            if (!value_of(a, p->arg[1], &v, &known) || !known) {
                return fail(a, line, "not a value", p->arg[1]);
            }
            osize(a, w1);
            rex(a, w1 == 64, 0, 0, r1, false);

            /*
             * the eight-bit-immediate form, sign-extended, which nasm
             * prefers whenever the value fits, three bytes instead of
             * six
             */
            bool small = ((int64_t)v >= -128 && (int64_t)v <= 127);
            if (small && w1 != 8) {
                put8(a, 0x83);
                modrm(a, 3, alu[i].digit, r1);
                put8(a, (uint8_t)v);
                return true;
            }
            /*
             * and the accumulator's form, which is the same trade one
             * step further along: `or eax, imm32` against the
             * accumulator needs no ModRM byte, so it is five bytes
             * rather than six. nasm takes it whenever the register is
             * the accumulator and the immediate is too big for 0x83
             */
            if (r1 == 0) {
                put8(a, (uint8_t)(alu[i].rm_r + 4 - (w1 == 8 ? 1 : 0)));
            } else {
                put8(a, (w1 == 8) ? 0x80 : 0x81);
                modrm(a, 3, alu[i].digit, r1);
            }
            if (w1 == 8) {
                put8(a, (uint8_t)v);
            } else if (w1 == 16) {
                put16(a, (uint16_t)v);
            } else {
                put32(a, (uint32_t)v);
            }
            return true;
        }
        /* against memory, with an immediate. */
        if (p->count == 2) {
            int mw = 64;
            struct mem im;
            const char *sized = drop_size(p->arg[0], &mw);
            (void)sized;
            if (parse_mem(a, p->arg[0], &im)) {
                uint64_t v; bool known;
                if (!value_of(a, p->arg[1], &v, &known) || !known) {
                    return fail(a, line, "not a value", p->arg[1]);
                }
                if (im.segment) { put8(a, im.segment); }
                osize(a, mw);
                rex(a, mw == 64, 0, 0, (im.base >= 0) ? im.base : 0, false);

                if (mw == 8) {
                    put8(a, 0x80);
                    mem_modrm(a, &im, alu[i].digit, line);
                    put8(a, (uint8_t)v);
                } else if ((int64_t)v >= -128 && (int64_t)v <= 127) {
                    put8(a, 0x83);
                    mem_modrm(a, &im, alu[i].digit, line);
                    put8(a, (uint8_t)v);
                } else {
                    put8(a, 0x81);
                    mem_modrm(a, &im, alu[i].digit, line);
                    put32(a, (uint32_t)v);
                }
                return true;
            }
        }
        return fail(a, line, "cannot do that with those", op);
    }


    if (strcmp(op, "test") == 0 && r1 >= 0 && r2 < 0 && p->count == 2) {
        /*
         * against an immediate: f6 /0 for a byte and f7 /0 above it,
         * except for the accumulator, which has a shorter form of its
         * own that nasm uses, a8 for al and a9 for the rest
         */
        uint64_t v; bool known;
        if (!value_of(a, p->arg[1], &v, &known) || !known) {
            return fail(a, line, "not a value", p->arg[1]);
        }
        osize(a, w1);
        if (r1 == 0) {
            rex(a, w1 == 64, 0, 0, 0, false);
            put8(a, (w1 == 8) ? 0xa8 : 0xa9);
        } else {
            rex(a, w1 == 64, 0, 0, r1, w1 == 8 && r1 >= 4);
            put8(a, (w1 == 8) ? 0xf6 : 0xf7);
            modrm(a, 3, 0, r1);
        }
        if (w1 == 8) { put8(a, (uint8_t)v); }
        else if (w1 == 16) { put16(a, (uint16_t)v); }
        else { put32(a, (uint32_t)v); }
        return true;
    }
    if (strcmp(op, "test") == 0 && r1 >= 0 && r2 >= 0 && w1 == w2) {
        osize(a, w1);
        rex(a, w1 == 64, r2, 0, r1, false);
        put8(a, (w1 == 8) ? 0x84 : 0x85);
        modrm(a, 3, r2, r1);
        return true;
    }

    /* always the near form, four bytes of displacement. */
    static const struct { const char *name; uint8_t two; } jcc[] = {
        { "jz", 0x84 }, { "je", 0x84 }, { "jnz", 0x85 }, { "jne", 0x85 },
        { "jl", 0x8c }, { "jge", 0x8d }, { "jle", 0x8e }, { "jg", 0x8f },
        { "jb", 0x82 }, { "jae", 0x83 }, { "jbe", 0x86 }, { "ja", 0x87 },
        { "js", 0x88 }, { "jns", 0x89 },
    };
    for (size_t i = 0; i < sizeof jcc / sizeof jcc[0]; i++) {
        if (strcmp(op, jcc[i].name) == 0 && p->count == 1) {
            int which = (int)a->jumps++;
            bool near = a->near_form[which];

            if (near) {
                put8(a, 0x0f);
                put8(a, jcc[i].two);
                rel_patch(a, p->arg[0], line);
            } else {
                /*
                 * 0x70+cc is the short form of the same condition: the
                 * two tables are one table with 0x10 between them
                 */
                put8(a, (uint8_t)(jcc[i].two - 0x10));
                want_patch(a, p->arg[0], 1, line);
                put8(a, 0);
            }
            a->patch[a->patches - 1].jump = which;
            a->patch[a->patches - 1].next = a->origin + a->len[a->sec];
            return true;
        }
    }
    if ((strcmp(op, "jmp") == 0 || strcmp(op, "call") == 0)
        && p->count == 1) {
        /* a *far* jump: `jmp 0x0000:start`. */
        int fw = (a->bits == 16) ? 16 : 32;
        const char *far = drop_size(p->arg[0], &fw);
        const char *colon = strchr(far, ':');
        if (colon != NULL) {
            char segtext[64];
            size_t n = (size_t)(colon - far);
            if (n >= sizeof segtext) {
                return fail(a, line, "not a far address", p->arg[0]);
            }
            memcpy(segtext, far, n);
            segtext[n] = '\0';

            uint64_t seg, off;
            bool sk, ok2;
            if (!value_of(a, segtext, &seg, &sk)) {
                return fail(a, line, "not a segment", segtext);
            }
            if (!value_of(a, colon + 1, &off, &ok2)) {
                return fail(a, line, "not an offset", colon + 1);
            }

            osize(a, fw);
            put8(a, (op[0] == 'j') ? 0xea : 0x9a);
            if (fw == 16) {
                if (!ok2) { want_patch(a, colon + 1, 4, line); }
                put16(a, (uint16_t)off);
            } else {
                if (!ok2) { want_patch(a, colon + 1, 4, line); }
                put32(a, (uint32_t)off);
            }
            if (!ok2) { a->patch[a->patches - 1].next = 0; }
            put16(a, (uint16_t)seg);
            return true;
        }

        if (r1 >= 0 && w1 == 64) {
            /* through a register: ff /4 for jmp, ff /2 for call */
            rex(a, false, 0, 0, r1, false);
            put8(a, 0xff);
            modrm(a, 3, (op[0] == 'j') ? 4 : 2, r1);
            return true;
        }
        if (op[0] == 'c') {
            /*
             * call has no short form: it is always the near one, which
             * is one fewer decision to make
             */
            put8(a, 0xe8);
            rel_patch(a, p->arg[0], line);
            a->patch[a->patches - 1].next = a->origin + a->len[a->sec];
            return true;
        }
        int which = (int)a->jumps++;
        if (a->near_form[which]) {
            put8(a, 0xe9);
            rel_patch(a, p->arg[0], line);
        } else {
            put8(a, 0xeb);
            want_patch(a, p->arg[0], 1, line);
            put8(a, 0);
        }
        a->patch[a->patches - 1].jump = which;
        a->patch[a->patches - 1].next = a->origin + a->len[a->sec];
        return true;
    }

    /* `lgdt [gdt_ptr]` loads the register that says where the global descriptor table is. */
    if (strcmp(op, "lgdt") == 0 || strcmp(op, "lidt") == 0) {
        struct mem m;
        if (p->count != 1 || !parse_mem(a, p->arg[0], &m)) {
            return fail(a, line, "lgdt and lidt take a memory operand",
                        p->count ? p->arg[0] : op);
        }
        if (m.segment) { put8(a, m.segment); }
        if (m.base >= 0 && (m.base & 8)) { put8(a, 0x41); }
        put8(a, 0x0f);
        put8(a, 0x01);
        mem_modrm(a, &m, (op[1] == 'g') ? 2 : 3, line);
        return true;
    }


    if (strcmp(op, "in") == 0 || strcmp(op, "out") == 0) {
        bool reading = (op[0] == 'i');
        const char *port = reading ? p->arg[1] : p->arg[0];
        const char *data = reading ? p->arg[0] : p->arg[1];
        int dw = 0;
        int dr = find_reg(data, &dw);

        if (dr != 0) {
            return fail(a, line, "in and out only use the accumulator", data);
        }
        if (strcmp(port, "dx") == 0) {
            osize(a, dw);
            put8(a, (uint8_t)((reading ? 0xec : 0xee) + (dw == 8 ? 0 : 1)));
            return true;
        }
        uint64_t v; bool known;
        if (!value_of(a, port, &v, &known) || !known || v > 255) {
            return fail(a, line, "not a port", port);
        }
        osize(a, dw);
        put8(a, (uint8_t)((reading ? 0xe4 : 0xe6) + (dw == 8 ? 0 : 1)));
        put8(a, (uint8_t)v);
        return true;
    }

    return fail(a, line, "i do not know that instruction", op);
}



void asm_init(struct assembler *a)
{
    memset(a, 0, sizeof *a);
    a->bits = 64;
    a->sec  = ASM_TEXT;
}

bool asm_line(struct assembler *a, const char *text, int line)
{
    struct parts p;
    if (!split(text, &p)) {
        return false;
    }
    if (p.op[0] == '\0') {
        return true;
    }

    /*
     * a label ends in a colon and may be alone on its line or in front
     * of an instruction
     */
    size_t n = strlen(p.op);
    if (n > 1 && p.op[n - 1] == ':') {
        p.op[n - 1] = '\0';
        if (!define_label(a, p.op, line)) {
            return false;
        }
        if (p.count == 0) {
            return true;
        }
        /* what followed the label is the instruction */
        memmove(p.op, p.arg[0], sizeof p.op < sizeof p.arg[0]
                                ? sizeof p.op : sizeof p.arg[0]);
        p.op[sizeof p.op - 1] = '\0';
        for (int i = 0; i + 1 < p.count; i++) {
            memcpy(p.arg[i], p.arg[i + 1], sizeof p.arg[0]);
        }
        p.count--;
    }

    /*
     * everything is folded to lower case, operands included, and
     * *before* the directives are looked at.
     *
     * doing the operands later meant `TRAMP_BASE equ 0x8000` stored a
     * label called `tramp_base` while `TRAMP_BASE+0x0f00` looked one up
     * called `TRAMP_BASE`, and the two never met. assembly is
     * case-insensitive, so the only safe moment to decide that is
     * before anything reads a name
     */
    lower(p.op);
    for (int i = 0; i < p.count; i++) {
        lower(p.arg[i]);
    }

    /* directives */
    if (strcmp(p.op, "bits") == 0 && p.count == 1) {
        uint64_t v; bool known;
        if (value_of(a, p.arg[0], &v, &known) && known
            && (v == 16 || v == 32 || v == 64)) {
            a->bits = (int)v;
            return true;
        }
        return fail(a, line, "bits takes 16, 32 or 64", p.arg[0]);
    }
    /* `section .rodata` and the rest. */
    if (strcmp(p.op, "section") == 0 && p.count >= 1) {
        if (strncmp(p.arg[0], ".rodata", 7) == 0) {
            a->sec = ASM_RODATA;
        } else if (strncmp(p.arg[0], ".data", 5) == 0) {
            a->sec = ASM_DATA;
        } else if (strncmp(p.arg[0], ".note", 5) == 0) {
            /*
             * a note section carries no bytes anybody executes and is
             * ignored, the linker adds its own
             */
            a->sec = ASM_RODATA;
        } else {
            a->sec = ASM_TEXT;
        }
        return true;
    }

    /* `global name`: this file defines it and somebody else may use it. */
    if (strcmp(p.op, "global") == 0) {
        for (int i = 0; i < p.count; i++) {
            char name[64];
            snprintf(name, sizeof name, "%s", p.arg[i]);
            char *cut = strpbrk(name, ":,");    /* `global main:function` */
            if (cut != NULL) {
                *cut = '\0';
            }
            if (name[0] == '\0') {
                continue;
            }
            bool known = false;
            for (size_t k = 0; k < a->labels; k++) {
                if (strcmp(a->label[k].name, name) == 0) {
                    a->label[k].exported = true;
                    known = true;
                }
            }
            if (!known) {
                /*
                 * said before the label it names, which is the usual
                 * way round, so the entry is made now and defined
                 * when the line carrying it is reached
                 */
                if (a->labels >= ASM_MAX_LABELS) {
                    return fail(a, line, "too many labels", NULL);
                }
                struct asm_label *l = &a->label[a->labels++];
                memset(l, 0, sizeof *l);
                snprintf(l->name, sizeof l->name, "%s", name);
                l->exported = true;
            }
        }
        return true;
    }

    if (strcmp(p.op, "default") == 0
     || strcmp(p.op, "extern") == 0) {
        return true;        /* accepted and ignored: this emits flat bytes */
    }
    /*
     * NAME equ VALUE. a label whose address is a number somebody chose
     * rather than wherever the assembler happened to be
     */
    if (p.count >= 1 && strncmp(p.arg[0], "equ", 3) == 0) {
        /*
         * the splitter has taken the space out, so the value is either
         * glued to the word or is the next argument
         */
        const char *val = p.arg[0] + 3;
        if (*val == '\0' && p.count >= 2) {
            val = p.arg[1];
        }
        uint64_t v; bool known;
        if (!value_of(a, val, &v, &known) || !known) {
            return fail(a, line, "equ needs a value that is known here", val);
        }
        for (size_t i = 0; i < a->labels; i++) {
            if (strcmp(a->label[i].name, p.op) == 0) {
                a->label[i].at = v;
                a->label[i].defined = true;
                return true;
            }
        }
        if (a->labels >= ASM_MAX_LABELS) {
            return fail(a, line, "too many labels", NULL);
        }
        struct asm_label *l = &a->label[a->labels++];
        snprintf(l->name, sizeof l->name, "%s", p.op);
        l->at = v;
        l->defined = true;
        return true;
    }

    /* pad forward to a boundary. */
    if (strcmp(p.op, "align") == 0 && p.count >= 1) {
        uint64_t to; bool known;
        if (!value_of(a, p.arg[0], &to, &known) || !known || to == 0) {
            return fail(a, line, "align needs a number", p.arg[0]);
        }
        while (((a->origin + a->len[a->sec]) % to) != 0) {
            put8(a, 0x90);
        }
        return true;
    }
    if (strcmp(p.op, "times") == 0 && p.count >= 2) {
        uint64_t n; bool known;
        if (!value_of(a, p.arg[0], &n, &known) || !known) {
            return fail(a, line, "times needs a count", p.arg[0]);
        }
        /* the rest of the line, repeated. */
        for (uint64_t k = 0; k < n; k++) {
            struct parts rest;
            memset(&rest, 0, sizeof rest);
            snprintf(rest.op, sizeof rest.op, "%s", p.arg[1]);
            for (int i = 2; i < p.count; i++) {
                memcpy(rest.arg[i - 2], p.arg[i], sizeof rest.arg[0]);
            }
            rest.count = p.count - 2;

            if (rest.op[0] == 'd' && rest.op[2] == '\0' && rest.count >= 1) {
                uint64_t v; bool k2;
                if (!value_of(a, rest.arg[0], &v, &k2)) {
                    return fail(a, line, "not a value", rest.arg[0]);
                }
                switch (rest.op[1]) {
                case 'b': put8(a, (uint8_t)v); break;
                case 'w': put16(a, (uint16_t)v); break;
                case 'd': put32(a, (uint32_t)v); break;
                default:  put64(a, v); break;
                }
            } else if (!encode(a, &rest, line)) {
                return false;
            }
        }
        return true;
    }

    if (strcmp(p.op, "org") == 0 && p.count == 1) {
        uint64_t v; bool known;
        if (!value_of(a, p.arg[0], &v, &known) || !known) {
            return fail(a, line, "org needs a number", p.arg[0]);
        }
        a->origin = v;
        return true;
    }
    if (strcmp(p.op, "db") == 0 || strcmp(p.op, "dw") == 0
     || strcmp(p.op, "dd") == 0 || strcmp(p.op, "dq") == 0) {
        for (int i = 0; i < p.count; i++) {
            uint64_t v; bool known;
            if (!value_of(a, p.arg[i], &v, &known)) {
                return fail(a, line, "not a value", p.arg[i]);
            }
            /* a *name* always leaves a record, even once its value is known. */
            bool is_name = !((p.arg[i][0] >= '0' && p.arg[i][0] <= '9')
                          || p.arg[i][0] == '-' || p.arg[i][0] == '\''
                          || p.arg[i][0] == '"' || p.arg[i][0] == '(');
            if (!known || is_name) {
                want_patch(a, p.arg[i], (p.op[1] == 'q') ? 8 : 4, line);
                a->patch[a->patches - 1].next = 0;
                a->patch[a->patches - 1].unresolved = !known;
            }
            switch (p.op[1]) {
            case 'b': put8(a, (uint8_t)v); break;
            case 'w': put16(a, (uint16_t)v); break;
            case 'd': put32(a, (uint32_t)v); break;
            default:  put64(a, v); break;
            }
        }
        return true;
    }

    return encode(a, &p, line);
}

/*
 * a name nothing defines still needs a symbol, so the relocation has
 * something to point at
 */
static bool remember_extern(struct assembler *a, const char *name)
{
    for (size_t i = 0; i < a->labels; i++) {
        if (strcmp(a->label[i].name, name) == 0) {
            return true;
        }
    }
    if (a->labels >= ASM_MAX_LABELS) {
        return false;
    }
    struct asm_label *l = &a->label[a->labels++];
    snprintf(l->name, sizeof l->name, "%s", name);
    l->at = 0;
    l->defined = false;
    return true;
}

bool asm_finish(struct assembler *a)
{
    for (size_t i = 0; i < a->patches; i++) {
        struct asm_patch *p = &a->patch[i];

        uint64_t value = 0;
        bool found = false;
        for (size_t j = 0; j < a->labels; j++) {
            if (strcmp(a->label[j].name, p->name) == 0
                && a->label[j].defined) {
                value = a->label[j].at;
                found = true;
                /* it is defined after all. */
                p->unresolved = false;
                break;
            }
        }
        if (!found) {
            /*
             * not an error. a name this file does not define is one
             * somebody else provides, and the answer is a relocation
             * rather than a refusal, which is exactly what `extern`
             * means and why an assembler can finish a file that calls
             * into C
             */
            p->unresolved = true;
            if (!remember_extern(a, p->name)) {
                return fail(a, p->line, "too many names", p->name);
            }
            continue;
        }

        /*
         * a relative jump is measured from the byte *after* the
         * instruction, which is what `next` records. an absolute
         * reference, a `dq` of a label, has `next` of zero and takes
         * the address itself
         */
        uint64_t v = (p->next != 0) ? (value - p->next) : value;

        /*
         * a branch that guessed short and does not reach has to be
         * assembled again as the long form, and lengthening it moves
         * everything after it, so the whole pass is worth nothing and
         * another one follows
         */
        if (p->jump >= 0 && p->size == 1) {
            int64_t rel = (int64_t)v;
            if (rel < -128 || rel > 127) {
                a->near_form[p->jump] = true;
                a->grew = true;
                continue;
            }
        }

        for (uint8_t b = 0; b < p->size; b++) {
            a->out[p->section][p->at + b] = (uint8_t)(v >> (8 * b));
        }
    }
    return true;
}

void asm_restart(struct assembler *a)
{
    for (int i = 0; i < ASM_SECTIONS; i++) { a->len[i] = 0; }
    a->sec     = ASM_TEXT;
    a->labels  = 0;
    a->patches = 0;
    a->jumps   = 0;
    a->grew    = false;
    a->origin  = 0;
    a->error[0] = '\0';
    a->error_line = 0;
    a->bits = 64;
    /*
     * near_form is deliberately kept: it is the whole state that makes
     * the next pass different from this one
     */
}
