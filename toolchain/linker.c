// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/linker.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a linker.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * a kernel is a hundred and forty files rather than the two a fixture
 * has, and every one of these numbers was one that stopped it. they are
 * written large rather than made dynamic because a linker that runs out
 * of room says so, and one that grows without limit finds out on a
 * machine with 512MB in it
 */
#define MAX_OBJECTS  512
#define MAX_SECTIONS 64
#define MAX_SYMBOLS  16384
#define MAX_OUTPUT   (8 * 1024 * 1024)
#define MAX_PLACED   32
#define MAX_STEPS    64
#define MAX_PATTERNS 16

#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8
#define SHF_ALLOC    0x2
#define SHF_EXECINSTR 0x4
#define SHF_WRITE    0x1

#define R_X86_64_64   1
#define R_X86_64_PC32 2
#define R_X86_64_32   10
#define R_X86_64_32S  11
#define R_X86_64_PLT32 4

struct shdr {
    uint32_t name; uint32_t type; uint64_t flags; uint64_t addr;
    uint64_t offset; uint64_t size; uint32_t link; uint32_t info;
    uint64_t align; uint64_t entsize;
};

struct sym {
    uint32_t name; uint8_t info; uint8_t other; uint16_t shndx;
    uint64_t value; uint64_t size;
};

struct rela { uint64_t offset; uint64_t info; int64_t addend; };

/*
 * one input file, kept whole in memory: an object is small and reading
 * it twice would be more code than holding it
 */
struct object {
    uint8_t *data;
    size_t   size;
    const char *path;

    /* where it stood on the link line. */
    long     order;

    struct shdr *shdr;
    size_t       shnum;
    const char  *shstr;

    /* where each of its sections ended up in the output. */
    uint64_t placed_at[MAX_SECTIONS];
    bool     placed[MAX_SECTIONS];
    /* `/DISCARD/` named it. */
    bool     dropped[MAX_SECTIONS];
    /*
     * which output section it went into, so a symbol can say which one
     * it belongs to, which is how `nm` and gensyms tell a function
     * from a global variable: by the section, not by the symbol
     */
    int      placed_in[MAX_SECTIONS];
};

static struct object obj[MAX_OBJECTS];
static int objects;

/* every global name, and what it turned out to mean */
struct global {
    char     name[128];
    uint64_t value;
    bool     defined;
    const char *from;
    /* the script said `__text_start = .` rather than any object defining it. */
    bool     absolute;
    int      section;
};
static struct global global[MAX_SYMBOLS];
static int globals;

/* the output, one buffer per placed section */
struct out_section {
    char     name[32];
    char     phdr[32];      /* the segment the script put it in */
    uint64_t addr;
    uint64_t flags;
    uint8_t *data;
    size_t   len, cap;
    /*
     * nothing in it occupies the file: `.bss` is a size and an address
     * and no bytes, and a linker that writes it out anyway turns the
     * kernel's several megabytes of zeroed globals into several
     * megabytes of file
     */
    bool     nobits;
};
static struct out_section out[MAX_PLACED];
static int outs;

static uint64_t entry_addr;
static char     entry_name[128] = "_start";

static void die(const char *what, const char *detail)
{
    fprintf(stderr, "link: %s%s%s\n", what, detail ? ": " : "",
            detail ? detail : "");
    exit(1);
}



/*
 * an object, once its bytes are in memory: the section headers and the
 * names of them, which is everything else here reads through
 */
static void take_apart(struct object *o)
{
    if (o->size < 64 || memcmp(o->data, "\177ELF", 4) != 0) {
        die("not an object file", o->path);
    }
    uint64_t shoff = *(uint64_t *)(o->data + 40);
    uint16_t shnum = *(uint16_t *)(o->data + 60);
    uint16_t shstrndx = *(uint16_t *)(o->data + 62);

    if (shnum > MAX_SECTIONS) {
        die("more sections than i can place", o->path);
    }
    o->shdr  = (struct shdr *)(o->data + shoff);
    o->shnum = shnum;
    o->shstr = (const char *)(o->data + o->shdr[shstrndx].offset);
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        die("cannot read", path);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc((size_t)n + 1);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        die("could not read all of", path);
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

/*
 * a `.a` is not a format so much as a list: `!<arch>\n` and then, for
 * each member, sixty bytes of header with the name and the size in
 * *decimal text*, the member itself, and a pad byte if the size was
 * odd. two of the members are apparatus rather than objects, `/` is
 * the symbol index and `//` is where names too long for the sixteen
 * bytes are kept, and both are skipped here, because the index is a
 * summary of what the members say and the members are all in hand.
 *
 * the rule that makes an archive worth having is one this had to grow
 * into: **a member is pulled in only if it resolves
 * something still undefined**. an object named on the line is always
 * linked; a member is linked only if it is wanted. that is the whole
 * difference between `libc.a` and a list of objects, and it is what
 * lets a program that writes its own `_start` not drag `start.o` in and
 * find `_start` defined twice.
 */
struct member {
    struct object o;        /* filled in but not linked, until it is wanted */
    bool     taken;
    char     name[128];
};
static struct member member[MAX_OBJECTS];
static int members;

static void read_archive(const uint8_t *data, size_t size, const char *path,
                         long order)
{
    /*
     * the long-name table, if this archive has one: names past fifteen
     * characters live there and the header carries `/<offset>` instead
     */
    const char *longnames = NULL;
    size_t at = 8;
    long which = 0;

    while (at + 60 <= size) {
        const char *h = (const char *)data + at;
        if (memcmp(h + 58, "`\n", 2) != 0) {
            die("this archive is not one i can read", path);
        }
        char sizetext[11];
        memcpy(sizetext, h + 48, 10);
        sizetext[10] = '\0';
        size_t msize = (size_t)strtoul(sizetext, NULL, 10);
        const uint8_t *body = data + at + 60;
        if (at + 60 + msize > size) {
            die("a member runs off the end of", path);
        }

        char name[128] = "";
        if (h[0] == '/' && h[1] == '/') {
            longnames = (const char *)body;
        } else if (h[0] == '/' && (h[1] == ' ' || h[1] == '\n')) {
            /*
             * the symbol index. skipped deliberately: it says which
             * member defines what, which is a claim that can be wrong,
             * and every member's own symbol table says the same thing
             * and cannot be
             */
        } else if (h[0] == '/' && longnames != NULL) {
            size_t off = (size_t)strtoul(h + 1, NULL, 10);
            snprintf(name, sizeof name, "%.*s", (int)sizeof name - 1,
                     longnames + off);
            char *end = strchr(name, '/');
            if (end != NULL) { *end = '\0'; }
        } else {
            size_t n = 0;
            while (n < 16 && h[n] != '/' && h[n] != ' ') {
                name[n] = h[n];
                n++;
            }
            name[n] = '\0';
        }

        if (name[0] != '\0') {
            if (members >= MAX_OBJECTS) {
                die("too many archive members", path);
            }
            struct member *m = &member[members];
            memset(m, 0, sizeof *m);
            snprintf(m->name, sizeof m->name, "%.80s(%.40s)", path, name);
            m->o.data  = (uint8_t *)body;
            m->o.size  = msize;
            m->o.path  = m->name;
            m->o.order = order + which++;
            take_apart(&m->o);
            members++;
        }

        at += 60 + msize + (msize & 1);
    }
}

static void load(const char *path, long order)
{
    size_t size;
    uint8_t *data = slurp(path, &size);

    if (size >= 8 && memcmp(data, "!<arch>\n", 8) == 0) {
        read_archive(data, size, path, order);
        return;
    }

    if (objects >= MAX_OBJECTS) {
        die("too many objects", path);
    }
    struct object *o = &obj[objects++];
    memset(o, 0, sizeof *o);
    o->data  = data;
    o->size  = size;
    o->path  = path;
    o->order = order;
    take_apart(o);
}

static const char *sec_name(struct object *o, size_t i)
{
    return o->shstr + o->shdr[i].name;
}

/*
 * the symbol table and its strings, which every object has exactly one
 * of and which every relocation refers through
 */
static struct sym *symbols_of(struct object *o, size_t *count,
                              const char **strings)
{
    for (size_t i = 0; i < o->shnum; i++) {
        if (o->shdr[i].type == SHT_SYMTAB) {
            *count = o->shdr[i].size / sizeof(struct sym);
            *strings = (const char *)(o->data + o->shdr[o->shdr[i].link].offset);
            return (struct sym *)(o->data + o->shdr[i].offset);
        }
    }
    *count = 0;
    *strings = NULL;
    return NULL;
}



static struct out_section *out_for(const char *name, uint64_t flags)
{
    for (int i = 0; i < outs; i++) {
        if (strcmp(out[i].name, name) == 0) {
            return &out[i];
        }
    }
    if (outs >= MAX_PLACED) {
        die("too many output sections", name);
    }
    struct out_section *s = &out[outs++];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    s->flags = flags;
    s->nobits = true;   /* until something with bytes in it turns up */
    s->cap = 64 * 1024;
    s->data = calloc(1, s->cap);
    if (s->data == NULL) {
        die("out of memory", NULL);
    }
    return s;
}

/* room for `n` bytes in a section. */
static void room(struct out_section *s, size_t n)
{
    if (n <= s->cap) {
        return;
    }
    size_t want = s->cap;
    while (want < n) {
        want *= 2;
    }
    uint8_t *bigger = realloc(s->data, want);
    if (bigger == NULL) {
        die("out of memory", s->name);
    }
    memset(bigger + s->cap, 0, want - s->cap);
    s->data = bigger;
    s->cap  = want;
}

/*
 * a section that asks for sixteen-byte alignment leaves a hole behind the
 * one before it, and something has to go in the hole. zero is the obvious
 * answer and the wrong one: 0x00 0x00 decodes as `add [rax], al`, so a
 * gap of zeros is a stretch of code that faults if anything ever runs off
 * the end of a function into it. so the padding is nops, which is what
 * ld does, and the reason `objdump -d` on any real binary shows those
 * strange long `nopw` instructions between functions.
 *
 * they are long because one nop repeated is slower to fetch than one
 * instruction that does nothing for ten bytes. this is the sequence Intel
 * recommends and the one ld emits, up to ten, and past ten it is tens
 * followed by a remainder, which is not a thing the program worked out from a
 * manual. the program generated every gap size from one to a hundred and twenty
 * seven, linked each with ld, and read back what it put there. the table
 * is what came out.
 */
static const uint8_t nop[11][10] = {
    { 0 },
    { 0x90 },
    { 0x66, 0x90 },
    { 0x0f, 0x1f, 0x00 },
    { 0x0f, 0x1f, 0x40, 0x00 },
    { 0x0f, 0x1f, 0x44, 0x00, 0x00 },
    { 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00 },
    { 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00 },
    { 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

/* fill `n` bytes at `p`. */
static void fill(uint8_t *p, size_t n, bool code)
{
    if (!code) {
        memset(p, 0, n);
        return;
    }
    while (n > 10) {
        memcpy(p, nop[10], 10);
        p += 10;
        n -= 10;
    }
    memcpy(p, nop[n], n);
}

/*
 * a linker script is read here as a *sequence* rather than as a table,
 * and that is the change this made. the reader used to collect the
 * output sections and the alignment demanded between them, which is
 * enough for a script that only places things. it is not enough for the
 * kernel's own, because two of the things in that one happen at a
 * position rather than to a section:
 *
 *   `__text_start = .` is a symbol whose value is wherever the location
 *   counter had got to when the line was read. move it and it means
 *   something else. the vmm maps each section from these, so a linker
 *   that skips them produces a kernel that links and cannot protect
 *   itself.
 *
 *   `. = ALIGN(CONSTANT(MAXPAGESIZE))` is the location counter being
 *   pushed forward, and where it lands depends on what came before.
 *
 * so the script becomes a list of steps in the order they are written,
 * and placement walks it with a location counter in hand. that is what
 * ld does, and once it is written this way `PHDRS`, `KEEP` and
 * `/DISCARD/` are each a few lines rather than a special case.
 */
enum { STEP_SET, STEP_ASSIGN, STEP_SECTION };

struct step {
    int      kind;
    char     name[128];     /* the symbol, or the output section */
    char     expr[192];     /* what to set it to */
    char     phdr[32];      /* `:text`, which segment it goes in */
    char     pat[MAX_PATTERNS][64];
    int      pats;
    bool     discard;       /* this is /DISCARD/ and nothing here is kept */
};
static struct step step[MAX_STEPS];
static int steps;

/* the segments, in the order PHDRS declared them. */
static char phdr_name[MAX_PLACED][32];
static int  phdrs;

/* what the script's `name = .` lines worked out to. */
static struct {
    char     name[128];
    uint64_t value;
    int      in_section;
} assigned[MAX_STEPS];
static int assigns;

/* `.`, numbers, `ALIGN(n)` and `CONSTANT(MAXPAGESIZE)`, with the usual arithmetic between them. */
static const char *ex;
static uint64_t    dot;

static uint64_t ex_sum(void);

static void ex_blank(void)
{
    while (*ex == ' ' || *ex == '\t') {
        ex++;
    }
}

static uint64_t ex_atom(void)
{
    ex_blank();
    if (*ex == '(') {
        ex++;
        uint64_t v = ex_sum();
        ex_blank();
        if (*ex == ')') { ex++; }
        return v;
    }
    if (strncmp(ex, "ALIGN", 5) == 0) {
        ex += 5;
        ex_blank();
        if (*ex == '(') { ex++; }
        uint64_t base = dot, to = ex_sum();
        ex_blank();
        if (*ex == ',') {
            /*
             * the two-argument form aligns what it is given rather than
             * the location counter
             */
            ex++;
            base = to;
            to = ex_sum();
            ex_blank();
        }
        if (*ex == ')') { ex++; }
        return to ? ((base + to - 1) & ~(to - 1)) : base;
    }
    if (strncmp(ex, "CONSTANT", 8) == 0) {
        /* MAXPAGESIZE, which is the only one any of these ask for. */
        while (*ex != '\0' && *ex != ')') { ex++; }
        if (*ex == ')') { ex++; }
        return 0x1000;
    }
    if (*ex == '.' && !(ex[1] >= '0' && ex[1] <= '9')) {
        ex++;
        return dot;
    }
    char *end;
    uint64_t v = strtoull(ex, &end, 0);
    if (end == ex) {
        die("i cannot read this bit of the script", ex);
    }
    ex = end;
    return v;
}

static uint64_t ex_sum(void)
{
    uint64_t v = ex_atom();
    for (;;) {
        ex_blank();
        if (*ex == '+')      { ex++; v += ex_atom(); }
        else if (*ex == '-') { ex++; v -= ex_atom(); }
        else                 { return v; }
    }
}

static uint64_t evaluate(const char *text, uint64_t here)
{
    ex = text;
    dot = here;
    return ex_sum();
}

/*
 * a word at a time, with the punctuation coming back one character at a
 * time so that `*(.text .text.*)` reads as `*`, `(`, two patterns and
 * `)` without any of it needing a special case.
 */
static char *sc;

static void sc_blank(void)
{
    for (;;) {
        while (*sc == ' ' || *sc == '\t' || *sc == '\n' || *sc == '\r') {
            sc++;
        }
        if (sc[0] == '/' && sc[1] == '*') {
            sc += 2;
            while (*sc != '\0' && !(sc[0] == '*' && sc[1] == '/')) {
                sc++;
            }
            if (*sc != '\0') { sc += 2; }
            continue;
        }
        break;
    }
}

static bool sc_word(char *out, size_t max)
{
    sc_blank();
    if (*sc == '\0') {
        return false;
    }
    if (strchr("{}();,=:", *sc) != NULL) {
        out[0] = *sc++;
        out[1] = '\0';
        return true;
    }
    size_t n = 0;
    while (*sc != '\0' && strchr(" \t\n\r{}();,=:", *sc) == NULL) {
        if (n + 1 < max) { out[n++] = *sc; }
        sc++;
    }
    out[n] = '\0';
    return true;
}

/*
 * look at the next word without taking it: the difference between
 * `__text_start = .` and `.text : { ... }` is the word *after* the name
 */
static bool sc_peek(char *out, size_t max)
{
    char *save = sc;
    bool got = sc_word(out, max);
    sc = save;
    return got;
}

static void sc_skip_parens(void)
{
    char w[64];
    int depth = 0;
    while (sc_word(w, sizeof w)) {
        if (strcmp(w, "(") == 0) { depth++; }
        else if (strcmp(w, ")") == 0 && --depth <= 0) { return; }
    }
}

/* the body of an output section: the input patterns, and whether anything says to keep them. */
static void read_body(struct step *s)
{
    char w[64];
    bool keeping = false;

    while (sc_word(w, sizeof w)) {
        if (strcmp(w, "}") == 0) {
            return;
        }
        if (strcmp(w, "KEEP") == 0) {
            keeping = true;
            continue;
        }
        if (strcmp(w, "(") == 0 || strcmp(w, ")") == 0
         || strcmp(w, ";") == 0 || strcmp(w, ",") == 0) {
            continue;
        }
        if (strcmp(w, "*") == 0) {
            /* `*(...)`: from any file. */
            char next[64];
            if (!sc_peek(next, sizeof next) || strcmp(next, "(") != 0) {
                continue;
            }
            sc_word(next, sizeof next);
            while (sc_word(w, sizeof w) && strcmp(w, ")") != 0) {
                if (s->pats < MAX_PATTERNS) {
                    snprintf(s->pat[s->pats++], sizeof s->pat[0], "%s", w);
                }
            }
            continue;
        }
        /*
         * a symbol assignment inside a section body, which none of
         * these scripts has, and which would need the location
         * counter part way through a section to mean anything
         */
    }
    (void)keeping;
}

static void read_sections(void)
{
    char w[64], next[64];

    if (!sc_word(w, sizeof w) || strcmp(w, "{") != 0) {
        die("SECTIONS without a body", NULL);
    }
    while (sc_word(w, sizeof w)) {
        if (strcmp(w, "}") == 0) {
            return;
        }
        if (strcmp(w, ";") == 0) {
            continue;
        }
        if (steps >= MAX_STEPS) {
            die("the script has more in it than i can follow", w);
        }
        if (!sc_peek(next, sizeof next)) {
            return;
        }

        if (strcmp(next, "=") == 0) {
            sc_word(next, sizeof next);
            struct step *s = &step[steps++];
            memset(s, 0, sizeof *s);
            s->kind = (strcmp(w, ".") == 0) ? STEP_SET : STEP_ASSIGN;
            snprintf(s->name, sizeof s->name, "%s", w);

            /*
             * the expression is taken as text and evaluated later,
             * because `.` does not have a value until the walk reaches
             * this line
             */
            size_t n = 0;
            sc_blank();
            while (*sc != '\0' && *sc != ';' && *sc != '\n') {
                if (n + 1 < sizeof s->expr) { s->expr[n++] = *sc; }
                sc++;
            }
            s->expr[n] = '\0';
            if (*sc == ';') { sc++; }
            continue;
        }

        struct step *s = &step[steps++];
        memset(s, 0, sizeof *s);
        s->kind = STEP_SECTION;
        snprintf(s->name, sizeof s->name, "%s", w);
        s->discard = (strcmp(w, "/DISCARD/") == 0);

        /*
         * anything between the name and the colon is an address for the
         * section, which none of these scripts gives
         */
        while (sc_word(w, sizeof w) && strcmp(w, "{") != 0) {
            if (strcmp(w, "(") == 0) { sc_skip_parens(); }
        }
        read_body(s);

        /* `} :text`, which segment the section was assigned to */
        if (sc_peek(next, sizeof next) && strcmp(next, ":") == 0) {
            sc_word(next, sizeof next);
            if (sc_word(next, sizeof next)) {
                snprintf(s->phdr, sizeof s->phdr, "%.31s", next);
            }
        }
        if (sc_peek(next, sizeof next) && strcmp(next, "=") == 0) {
            sc_word(next, sizeof next);     /* a fill pattern */
            sc_word(next, sizeof next);
        }
    }
}

static void read_phdrs(void)
{
    char w[64];
    if (!sc_word(w, sizeof w) || strcmp(w, "{") != 0) {
        die("PHDRS without a body", NULL);
    }
    while (sc_word(w, sizeof w)) {
        if (strcmp(w, "}") == 0) {
            return;
        }
        if (strcmp(w, ";") == 0) {
            continue;
        }
        if (phdrs >= MAX_PLACED) {
            die("too many program headers", w);
        }
        snprintf(phdr_name[phdrs++], sizeof phdr_name[0], "%.31s", w);
        while (sc_word(w, sizeof w) && strcmp(w, ";") != 0) {
            if (strcmp(w, "(") == 0) { sc_skip_parens(); }
        }
    }
}

static void read_script(const char *path)
{
    size_t size;
    char *text = (char *)slurp(path, &size);
    text[size] = '\0';
    sc = text;

    char w[64], next[64];
    while (sc_word(w, sizeof w)) {
        if (strcmp(w, "ENTRY") == 0) {
            sc_word(next, sizeof next);         /* ( */
            if (sc_word(next, sizeof next)) {
                snprintf(entry_name, sizeof entry_name, "%s", next);
            }
            sc_word(next, sizeof next);         /* ) */
            continue;
        }
        if (strcmp(w, "PHDRS") == 0) {
            read_phdrs();
            continue;
        }
        if (strcmp(w, "SECTIONS") == 0) {
            read_sections();
            continue;
        }
        /*
         * OUTPUT_FORMAT, OUTPUT_ARCH and the rest of the preamble: read
         * past whatever they are followed by
         */
        if (sc_peek(next, sizeof next) && strcmp(next, "(") == 0) {
            sc_skip_parens();
        }
    }
}

/* does this input section name match the pattern the script wrote? */
static bool glob(const char *pat, const char *s)
{
    if (*pat == '\0') {
        return *s == '\0';
    }
    if (*pat == '*') {
        for (const char *at = s; ; at++) {
            if (glob(pat + 1, at)) {
                return true;
            }
            if (*at == '\0') {
                return false;
            }
        }
    }
    if (*s == '\0' || (*pat != '?' && *pat != *s)) {
        return false;
    }
    return glob(pat + 1, s + 1);
}

/* every global name each object offers or wants, gathered into one table. */
static bool collect(bool complain)
{
    globals = 0;
    for (int i = 0; i < objects; i++) {
        struct object *o = &obj[i];
        size_t n; const char *str;
        struct sym *syms = symbols_of(o, &n, &str);

        for (size_t s = 0; s < n; s++) {
            if ((syms[s].info >> 4) != 1) {     /* globals only */
                continue;
            }
            const char *name = str + syms[s].name;
            if (name[0] == '\0') {
                continue;
            }

            struct global *g = NULL;
            for (int k = 0; k < globals; k++) {
                if (strcmp(global[k].name, name) == 0) {
                    g = &global[k];
                    break;
                }
            }
            if (g == NULL) {
                if (globals >= MAX_SYMBOLS) {
                    die("too many symbols", name);
                }
                g = &global[globals++];
                memset(g, 0, sizeof *g);
                snprintf(g->name, sizeof g->name, "%s", name);
            }

            if (syms[s].shndx == 0xfff2) {
                /*
                 * a *common* symbol: a tentative definition with a size
                 * and no section, which the linker is supposed to find
                 * room for in `.bss`. every script here says
                 * `*(COMMON)` and nothing in this tree produces one,
                 * because gcc has defaulted to `-fno-common` since its
                 * tenth version. so it is refused by name rather than
                 * skipped, a symbol quietly treated as undefined
                 * would be reported as "nothing defines it", which
                 * names the wrong problem
                 */
                fprintf(stderr, "link: %s is a common symbol in %s, and "
                        "i do not allocate those\n", name, o->path);
                return false;
            }
            if (syms[s].shndx == 0 || syms[s].shndx >= o->shnum) {
                /*
                 * a use, not a definition, or one of the special
                 * indices, which name no section in this file
                 */
                continue;
            }
            if (o->dropped[syms[s].shndx]) {
                continue;       /* the script threw its section away */
            }
            if (g->defined && complain) {
                /* two objects both defining it. */
                fprintf(stderr, "link: %s is defined twice, in %s and %s\n",
                        name, g->from, o->path);
                return false;
            }
            /* defined the moment something defines it, whether or not it has been placed yet. */
            g->defined = true;
            g->from = o->path;
            if (o->placed[syms[s].shndx]) {
                g->value = o->placed_at[syms[s].shndx] + syms[s].value;
            }
        }
    }
    return true;
}

/* a name something wants and nothing yet provides */
static bool still_missing(const char *name)
{
    for (int i = 0; i < globals; i++) {
        if (strcmp(global[i].name, name) == 0) {
            return !global[i].defined;
        }
    }
    return false;
}

/* does this member answer any of them? */
static bool answers_something(struct object *o)
{
    size_t n; const char *str;
    struct sym *syms = symbols_of(o, &n, &str);
    for (size_t s = 0; s < n; s++) {
        if ((syms[s].info >> 4) != 1 || syms[s].shndx == 0) {
            continue;
        }
        const char *name = str + syms[s].name;
        if (name[0] != '\0' && still_missing(name)) {
            return true;
        }
    }
    return false;
}

/*
 * the objects in the order the link line put them, which is the order
 * their sections are laid out in and therefore what decides the address
 * of everything. an archive member arrives late and belongs early
 */
static int by_order(const void *a, const void *b)
{
    const struct object *x = a, *y = b;
    return (x->order < y->order) ? -1 : (x->order > y->order) ? 1 : 0;
}

int main(int argc, char **argv)
{
    const char *script = NULL;
    const char *output = "a.out";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            script = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            snprintf(entry_name, sizeof entry_name, "%s", argv[++i]);
        } else {
            load(argv[i], (long)i * 4096);
        }
    }
    if (objects == 0 && members == 0) {
        fprintf(stderr, "usage: link [-T script.ld] [-o out] <a.o|a.a> ...\n");
        return 2;
    }

    /*
     * round and round until a pass takes nothing: a member pulled in for
     * one name brings its own undefined names with it, and those may be
     * answered by a member that nothing wanted a moment ago. `printf`
     * pulling in the formatter which pulls in the string routines is
     * exactly that, and stopping after one pass links a program with a
     * hole in it
     */
    for (bool again = true; again; ) {
        again = false;
        collect(false);
        for (int i = 0; i < members && !again; i++) {
            if (member[i].taken || !answers_something(&member[i].o)) {
                continue;
            }
            if (objects >= MAX_OBJECTS) {
                die("too many objects", member[i].name);
            }
            member[i].taken = true;
            obj[objects++] = member[i].o;

            /* one at a time, and the table worked out again before the next question is asked. */
            again = true;
        }
    }
    qsort(obj, (size_t)objects, sizeof obj[0], by_order);

    if (script != NULL) {
        read_script(script);
    }
    if (steps == 0) {
        /*
         * no script, or one that said nothing about sections: the four
         * every object has, in the order everybody puts them
         */
        static const char *usual[] = { ".text", ".rodata", ".data", ".bss" };
        struct step *s = &step[steps++];
        memset(s, 0, sizeof *s);
        s->kind = STEP_SET;
        snprintf(s->expr, sizeof s->expr, "0x400000");
        for (size_t i = 0; i < sizeof usual / sizeof usual[0]; i++) {
            s = &step[steps++];
            memset(s, 0, sizeof *s);
            s->kind = STEP_SECTION;
            snprintf(s->name, sizeof s->name, "%s", usual[i]);
            snprintf(s->pat[s->pats++], sizeof s->pat[0], "%s*", usual[i]);
        }
    }


    uint64_t at = 0;

    /* the flags an output section inherits from what went into it. */
    const uint64_t keep_flags = SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR;

    for (int k = 0; k < steps; k++) {
        struct step *st = &step[k];

        if (st->kind == STEP_SET) {
            at = evaluate(st->expr, at);
            continue;
        }
        if (st->kind == STEP_ASSIGN) {
            /* a name with a value and no section: `__text_start = .`. */
            snprintf(assigned[assigns].name, sizeof assigned[0].name,
                     "%s", st->name);
            assigned[assigns].in_section = -1;   /* the next one to be placed */
            assigned[assigns++].value = evaluate(st->expr, at);
            continue;
        }

        /*
         * an output section. the inputs are gathered pattern by pattern
         * in the order the script wrote them, and within a pattern in
         * the order the objects were named, which is what makes the
         * result reproducible and what makes it the same as ld's
         */
        if (st->discard) {
            for (int i = 0; i < objects; i++) {
                for (size_t s = 1; s < obj[i].shnum; s++) {
                    for (int q = 0; q < st->pats; q++) {
                        if (glob(st->pat[q], sec_name(&obj[i], s))) {
                            obj[i].dropped[s] = true;
                        }
                    }
                }
            }
            continue;
        }

        /* what the section is aligned to is the strictest thing in it. */
        uint64_t want = 1;
        bool any = false;
        for (int q = 0; q < st->pats; q++) {
            for (int i = 0; i < objects; i++) {
                for (size_t s = 1; s < obj[i].shnum; s++) {
                    if (!(obj[i].shdr[s].flags & SHF_ALLOC)
                     || obj[i].placed[s] || obj[i].dropped[s]) {
                        continue;
                    }
                    if (!glob(st->pat[q], sec_name(&obj[i], s))) {
                        continue;
                    }
                    if (obj[i].shdr[s].align > want) {
                        want = obj[i].shdr[s].align;
                    }
                    any = true;
                }
            }
        }
        if (!any) {
            /* an output section nothing matched. */
            continue;
        }
        at = (at + want - 1) & ~(want - 1);

        struct out_section *dst = NULL;
        for (int q = 0; q < st->pats; q++) {
            for (int i = 0; i < objects; i++) {
                struct object *o = &obj[i];
                for (size_t s = 1; s < o->shnum; s++) {
                    if (!(o->shdr[s].flags & SHF_ALLOC)
                     || o->placed[s] || o->dropped[s]) {
                        continue;
                    }
                    if (!glob(st->pat[q], sec_name(o, s))) {
                        continue;
                    }

                    if (dst == NULL) {
                        dst = out_for(st->name, o->shdr[s].flags & keep_flags);
                        dst->addr = at;
                        snprintf(dst->phdr, sizeof dst->phdr, "%s", st->phdr);
                        for (int q = 0; q < assigns; q++) {
                            if (assigned[q].in_section < 0) {
                                assigned[q].in_section = (int)(dst - out);
                            }
                        }
                    }
                    dst->flags |= o->shdr[s].flags & keep_flags;

                    uint64_t align = o->shdr[s].align ? o->shdr[s].align : 1;
                    uint64_t pad = (align - (dst->len % align)) % align;
                    room(dst, dst->len + pad + o->shdr[s].size);
                    fill(dst->data + dst->len, pad,
                         (dst->flags & SHF_EXECINSTR) != 0);
                    dst->len += pad;

                    o->placed_at[s] = dst->addr + dst->len;
                    o->placed[s] = true;
                    o->placed_in[s] = (int)(dst - out);

                    if (o->shdr[s].type != SHT_NOBITS) {
                        memcpy(dst->data + dst->len,
                               o->data + o->shdr[s].offset, o->shdr[s].size);
                        dst->nobits = false;
                    }
                    dst->len += o->shdr[s].size;
                }
            }
        }
        if (dst != NULL) {
            at = dst->addr + dst->len;
        }
    }

    /* a section with bytes in it that the script never mentioned. */
    for (int i = 0; i < objects; i++) {
        for (size_t s = 1; s < obj[i].shnum; s++) {
            if ((obj[i].shdr[s].flags & SHF_ALLOC)
             && !obj[i].placed[s] && !obj[i].dropped[s]) {
                fprintf(stderr, "link: the script says nothing about %s, "
                        "which %s has\n", sec_name(&obj[i], s), obj[i].path);
                return 1;
            }
        }
    }


    if (!collect(true)) {
        return 1;
    }

    /*
     * and the script's own names, which belong to no object and are
     * what the vmm maps each section from
     */
    for (int i = 0; i < assigns; i++) {
        if (assigned[i].in_section < 0) {
            assigned[i].in_section = outs - 1;   /* the script ended first */
        }
        struct global *g = NULL;
        for (int q = 0; q < globals; q++) {
            if (strcmp(global[q].name, assigned[i].name) == 0) {
                g = &global[q];
                break;
            }
        }
        if (g == NULL) {
            if (globals >= MAX_SYMBOLS) {
                die("too many symbols", assigned[i].name);
            }
            g = &global[globals++];
            memset(g, 0, sizeof *g);
            snprintf(g->name, sizeof g->name, "%s", assigned[i].name);
        }
        g->value    = assigned[i].value;
        g->defined  = true;
        g->absolute = true;
        g->section  = assigned[i].in_section;
        g->from     = script;
    }


    for (int i = 0; i < objects; i++) {
        struct object *o = &obj[i];
        size_t n; const char *str;
        struct sym *syms = symbols_of(o, &n, &str);

        for (size_t s = 0; s < o->shnum; s++) {
            if (o->shdr[s].type != SHT_RELA) {
                continue;
            }
            size_t target = o->shdr[s].info;
            if (!o->placed[target]) {
                continue;   /* relocations for a section nobody loaded */
            }

            struct rela *r = (struct rela *)(o->data + o->shdr[s].offset);
            size_t count = o->shdr[s].size / sizeof *r;

            for (size_t k = 0; k < count; k++) {
                uint32_t which = (uint32_t)(r[k].info >> 32);
                uint32_t type  = (uint32_t)(r[k].info & 0xffffffff);
                if (which >= n) {
                    die("a relocation names a symbol that is not there",
                        o->path);
                }

                const char *name = str + syms[which].name;
                uint64_t S;

                if (syms[which].shndx != 0) {
                    /*
                     * defined in this object, often the section
                     * symbol, which is how a local label is referred to
                     */
                    if (!o->placed[syms[which].shndx]) {
                        continue;
                    }
                    S = o->placed_at[syms[which].shndx] + syms[which].value;
                } else {
                    struct global *g = NULL;
                    for (int q = 0; q < globals; q++) {
                        if (strcmp(global[q].name, name) == 0) {
                            g = &global[q];
                            break;
                        }
                    }
                    if (g == NULL || !g->defined) {
                        fprintf(stderr,
                                "link: nothing defines %s, wanted by %s\n",
                                name, o->path);
                        return 1;
                    }
                    S = g->value;
                }

                uint64_t P = o->placed_at[target] + r[k].offset;

                /* find the byte in the output that P names */
                uint8_t *field = NULL;
                for (int q = 0; q < outs; q++) {
                    if (P >= out[q].addr && P < out[q].addr + out[q].len) {
                        field = out[q].data + (P - out[q].addr);
                        break;
                    }
                }
                if (field == NULL) {
                    die("a relocation points outside every section",
                        o->path);
                }

                switch (type) {
                case R_X86_64_64:
                    *(uint64_t *)field = S + (uint64_t)r[k].addend;
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32: {
                    /* a distance rather than an address, measured from the field. */
                    int64_t d = (int64_t)S + r[k].addend - (int64_t)P;
                    if (d < -2147483648LL || d > 2147483647LL) {
                        fprintf(stderr, "link: %s is too far from %s to "
                                "reach with a 32-bit jump\n", name, o->path);
                        return 1;
                    }
                    *(int32_t *)field = (int32_t)d;
                    break;
                }
                case R_X86_64_32:
                case R_X86_64_32S:
                    *(uint32_t *)field = (uint32_t)(S + (uint64_t)r[k].addend);
                    break;
                default:
                    fprintf(stderr, "link: relocation kind %u in %s is one "
                            "i do not know\n", type, o->path);
                    return 1;
                }
            }
        }
    }

    /* the entry point, which is just a symbol like any other */
    for (int q = 0; q < globals; q++) {
        if (strcmp(global[q].name, entry_name) == 0 && global[q].defined) {
            entry_addr = global[q].value;
            break;
        }
    }

    /* this is the first time this machine *produces* an ELF rather than only reading one. */
    struct segment {
        uint64_t addr, filesz, memsz, offset;
        uint32_t flags;
    };
    struct segment seg[MAX_PLACED];
    size_t segs = 0;

    /*
     * with PHDRS, none of the guessing below happens: the script names
     * the segments and every section says which one it is in, so a
     * segment is exactly the sections assigned to it, in the order the
     * segments were declared. that is worth having beyond faithfulness
     *, `.rodata` and `.ksyms` end up in one segment here because the
     * script says so, and the second of those is a table generated
     * between two link passes whose permissions nobody should have to
     * infer
     */
    for (int q = 0; q < phdrs; q++) {
        struct segment *sg = &seg[segs++];
        memset(sg, 0, sizeof *sg);
        for (int i = 0; i < outs; i++) {
            if (strcmp(out[i].phdr, phdr_name[q]) != 0) {
                continue;
            }
            uint32_t flags = 4;
            if (out[i].flags & SHF_WRITE)     { flags |= 2; }
            if (out[i].flags & SHF_EXECINSTR) { flags |= 1; }
            if (sg->memsz == 0 && sg->addr == 0) {
                sg->addr = out[i].addr;
            }
            sg->flags |= flags;
            sg->memsz = out[i].addr + out[i].len - sg->addr;
            if (!out[i].nobits) {
                sg->filesz = sg->memsz;
            }
        }
    }

    for (int i = 0; phdrs == 0 && i < outs; i++) {
        uint32_t flags = 4;                              /* read */
        if (out[i].flags & SHF_WRITE)     { flags |= 2; }
        if (out[i].flags & SHF_EXECINSTR) { flags |= 1; }

        /*
         * two sections go in the same segment unless they differ in
         * permissions *and* a page boundary separates them, because
         * permissions are a property of a page, and two sections
         * sharing a page cannot be given different ones however much
         * they differ. ld does exactly this, and it is why a script
         * that forgets its ALIGN(4096) gets a single writable
         * executable segment and a warning rather than the protection
         * it thought it asked for
         */
        bool split = true;
        if (segs > 0) {
            struct segment *prev = &seg[segs - 1];
            uint64_t end = prev->addr + prev->memsz;
            bool shared_page = end == prev->addr ||
                out[i].addr / 0x1000 == (end - 1) / 0x1000;
            split = prev->flags != flags && !shared_page;
        }
        if (split) {
            seg[segs].addr   = out[i].addr;
            seg[segs].flags  = flags;
            seg[segs].filesz = 0;
            seg[segs].memsz  = 0;
            segs++;
        } else {
            seg[segs - 1].flags |= flags;
        }

        struct segment *sg = &seg[segs - 1];
        sg->memsz = out[i].addr + out[i].len - sg->addr;
        if (!out[i].nobits) {
            /* the file ends where the last section with bytes in it does. */
            sg->filesz = sg->memsz;
        }
    }

    static uint8_t elf[MAX_OUTPUT];
    size_t phoff = 64;
    size_t file_at = phoff + segs * 56;

    memset(elf, 0, sizeof elf);
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 1; elf[6] = 1;
    *(uint16_t *)(elf + 16) = 2;        /* ET_EXEC */
    *(uint16_t *)(elf + 18) = 0x3e;
    *(uint32_t *)(elf + 20) = 1;
    *(uint64_t *)(elf + 24) = entry_addr;
    *(uint64_t *)(elf + 32) = phoff;
    *(uint16_t *)(elf + 52) = 64;
    *(uint16_t *)(elf + 54) = 56;
    *(uint16_t *)(elf + 56) = (uint16_t)segs;

    for (size_t i = 0; i < segs; i++) {
        /* where in the file the segment starts. */
        size_t page = 0x1000;
        file_at = (file_at + page - 1) & ~(page - 1);
        file_at += (size_t)(seg[i].addr % page);
        seg[i].offset = file_at;
        file_at += seg[i].filesz;

        uint8_t *ph = elf + phoff + i * 56;
        *(uint32_t *)(ph + 0)  = 1;      /* PT_LOAD */
        *(uint32_t *)(ph + 4)  = seg[i].flags;
        *(uint64_t *)(ph + 8)  = seg[i].offset;
        *(uint64_t *)(ph + 16) = seg[i].addr;
        *(uint64_t *)(ph + 24) = seg[i].addr;
        *(uint64_t *)(ph + 32) = seg[i].filesz;
        *(uint64_t *)(ph + 40) = seg[i].memsz;
        *(uint64_t *)(ph + 48) = page;
    }

    /* the buffer the whole file is built in. */
    if (file_at > sizeof elf) {
        die("this is a larger program than i have room to write out", output);
    }

    /*
     * the sections themselves, each written at the offset its address
     * works out to inside its segment. the gaps between them are left
     * as the zeros the buffer started as, they are not code and
     * nothing reaches them
     */
    for (int i = 0; i < outs; i++) {
        if (out[i].nobits) {
            continue;
        }
        for (size_t s = 0; s < segs; s++) {
            if (out[i].addr >= seg[s].addr &&
                out[i].addr < seg[s].addr + seg[s].memsz) {
                memcpy(elf + seg[s].offset + (out[i].addr - seg[s].addr),
                       out[i].data, out[i].len);
                break;
            }
        }
    }

    /*
     * an executable does not need one: nothing about running a program
     * reads it, and `strip` exists to throw it away.
     *
     * this kernel needs one anyway, and for a reason worth writing
     * down. a panic prints a backtrace, and a backtrace is a list of
     * addresses unless something can turn an address back into a name
     *, so `gensyms` reads the linked kernel's symbol table and builds
     * the table the kernel symbolises its own faults from. that makes
     * this the one part of the output where "nobody reads it" is false:
     * a kernel linked without a symbol table boots, works, and cannot
     * say where it died.
     *
     * locals go before globals because the section header's `info`
     * field is the index of the first global and the loader is entitled
     * to believe it. and the locals are the point rather than an
     * inclusion: every `static` function in this kernel is one, which
     * is most of the functions there are.
     */
    struct outsym { uint64_t value; uint32_t name; uint16_t shndx;
                    uint8_t info; };
    static struct outsym symbol[MAX_SYMBOLS * 4];
    size_t symbols = 1;                 /* the null one first */
    size_t strings = 1;                 /* and the empty name */

    /* the strings, laid out first so the symbols can point into them */
    size_t strtab_at = (file_at + 7) & ~(size_t)7;
    memset(elf + strtab_at, 0, 1);

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < objects; i++) {
            struct object *o = &obj[i];
            size_t n; const char *str;
            struct sym *syms = symbols_of(o, &n, &str);

            for (size_t s = 0; s < n; s++) {
                uint8_t bind = syms[s].info >> 4;
                uint8_t type = syms[s].info & 0xf;
                if (pass == 0 ? bind != 0 : bind == 0) {
                    continue;
                }
                if (type == 3 || type == 4) {
                    continue;       /* a section, or the file's own name */
                }
                if (syms[s].shndx == 0 || syms[s].shndx >= o->shnum
                 || !o->placed[syms[s].shndx]) {
                    continue;
                }
                const char *name = str + syms[s].name;
                if (name[0] == '\0') {
                    continue;
                }
                if (symbols >= sizeof symbol / sizeof symbol[0]
                 || strtab_at + strings + strlen(name) + 1 > sizeof elf) {
                    die("more symbols than i can write out", name);
                }
                struct outsym *y = &symbol[symbols++];
                y->value = o->placed_at[syms[s].shndx] + syms[s].value;
                y->info  = syms[s].info;
                y->shndx = (uint16_t)(o->placed_in[syms[s].shndx] + 1);
                y->name  = (uint32_t)strings;
                memcpy(elf + strtab_at + strings, name, strlen(name) + 1);
                strings += strlen(name) + 1;
            }
        }
        if (pass == 1) {
            /*
             * and the names the script itself defined, which belong to
             * no section at all
             */
            for (int q = 0; q < globals; q++) {
                if (!global[q].absolute) {
                    continue;
                }
                if (symbols >= sizeof symbol / sizeof symbol[0]
                 || strtab_at + strings + strlen(global[q].name) + 1
                        > sizeof elf) {
                    die("more symbols than i can write out", global[q].name);
                }
                struct outsym *y = &symbol[symbols++];
                y->value = global[q].value;
                y->info  = 0x10;            /* global, no type */
                y->shndx = (uint16_t)(global[q].section + 1);
                y->name  = (uint32_t)strings;
                memcpy(elf + strtab_at + strings, global[q].name,
                       strlen(global[q].name) + 1);
                strings += strlen(global[q].name) + 1;
            }
        }
    }

    /*
     * how many of them were local, which is what the section header has
     * to say so that anything reading it knows where the globals start
     */
    size_t first_global = symbols;
    for (size_t i = 1; i < symbols; i++) {
        if ((symbol[i].info >> 4) != 0) {
            first_global = i;
            break;
        }
    }

    size_t symtab_at = (strtab_at + strings + 7) & ~(size_t)7;
    if (symtab_at + symbols * 24 > sizeof elf) {
        die("more symbols than i have room to write out", output);
    }
    memset(elf + symtab_at, 0, 24);
    for (size_t i = 1; i < symbols; i++) {
        uint8_t *e = elf + symtab_at + i * 24;
        *(uint32_t *)(e + 0)  = symbol[i].name;
        *(uint8_t  *)(e + 4)  = symbol[i].info;
        *(uint8_t  *)(e + 5)  = 0;
        *(uint16_t *)(e + 6)  = symbol[i].shndx;
        *(uint64_t *)(e + 8)  = symbol[i].value;
        *(uint64_t *)(e + 16) = 0;
    }
    file_at = symtab_at + symbols * 24;

    /*
     * nothing loads these: a program is run from its program headers and
     * a stripped binary has no section headers at all. they are here so
     * the result can be *looked at*, objdump -d wants to know which
     * bytes are code and where they live, and without them it has
     * nothing to go on and prints nothing. a linker whose output cannot
     * be disassembled is a linker that can only be debugged by running
     * it, which for kernel code means by not booting
     */
    static const char *extra[] = { ".symtab", ".strtab", ".shstrtab" };
    size_t shstr_at = file_at;
    size_t shstr_len = 1;                       /* the empty name first */
    for (int i = 0; i < outs; i++) {
        shstr_len += strlen(out[i].name) + 1;
    }
    size_t extra_name[3];
    for (size_t i = 0; i < 3; i++) {
        extra_name[i] = shstr_len;
        memcpy(elf + shstr_at + shstr_len, extra[i], strlen(extra[i]) + 1);
        shstr_len += strlen(extra[i]) + 1;
    }

    size_t names[MAX_PLACED];
    size_t put = 1;
    for (int i = 0; i < outs; i++) {
        names[i] = put;
        memcpy(elf + shstr_at + put, out[i].name, strlen(out[i].name) + 1);
        put += strlen(out[i].name) + 1;
    }
    file_at = shstr_at + shstr_len;

    size_t shoff = (file_at + 7) & ~(size_t)7;
    size_t shnum = (size_t)outs + 4;    /* null, ours, and the three above */
    if (shoff + shnum * 64 > sizeof elf) {
        die("this is a larger program than i have room to write out", output);
    }
    memset(elf + shoff, 0, shnum * 64);

    for (int i = 0; i < outs; i++) {
        uint8_t *sh = elf + shoff + (size_t)(i + 1) * 64;
        size_t offset = 0;
        for (size_t s = 0; s < segs; s++) {
            if (seg[s].memsz != 0 && out[i].addr >= seg[s].addr &&
                out[i].addr < seg[s].addr + seg[s].memsz) {
                offset = seg[s].offset + (out[i].addr - seg[s].addr);
                break;
            }
        }
        *(uint32_t *)(sh + 0)  = (uint32_t)names[i];
        *(uint32_t *)(sh + 4)  = out[i].nobits ? SHT_NOBITS : SHT_PROGBITS;
        *(uint64_t *)(sh + 8)  = out[i].flags;
        *(uint64_t *)(sh + 16) = out[i].addr;
        *(uint64_t *)(sh + 24) = offset;
        *(uint64_t *)(sh + 32) = out[i].len;
        *(uint64_t *)(sh + 48) = 16;            /* alignment */
    }

    uint8_t *sh = elf + shoff + ((size_t)outs + 1) * 64;
    *(uint32_t *)(sh + 0)  = (uint32_t)extra_name[0];
    *(uint32_t *)(sh + 4)  = SHT_SYMTAB;
    *(uint64_t *)(sh + 24) = symtab_at;
    *(uint64_t *)(sh + 32) = symbols * 24;
    *(uint32_t *)(sh + 40) = (uint32_t)outs + 2;    /* the strings */
    *(uint32_t *)(sh + 44) = (uint32_t)first_global;
    *(uint64_t *)(sh + 48) = 8;
    *(uint64_t *)(sh + 56) = 24;

    sh = elf + shoff + ((size_t)outs + 2) * 64;
    *(uint32_t *)(sh + 0)  = (uint32_t)extra_name[1];
    *(uint32_t *)(sh + 4)  = SHT_STRTAB;
    *(uint64_t *)(sh + 24) = strtab_at;
    *(uint64_t *)(sh + 32) = strings;
    *(uint64_t *)(sh + 48) = 1;

    sh = elf + shoff + ((size_t)outs + 3) * 64;
    *(uint32_t *)(sh + 0)  = (uint32_t)extra_name[2];
    *(uint32_t *)(sh + 4)  = SHT_STRTAB;
    *(uint64_t *)(sh + 24) = shstr_at;
    *(uint64_t *)(sh + 32) = shstr_len;
    *(uint64_t *)(sh + 48) = 1;

    *(uint64_t *)(elf + 40) = shoff;
    *(uint16_t *)(elf + 58) = 64;
    *(uint16_t *)(elf + 60) = (uint16_t)shnum;
    *(uint16_t *)(elf + 62) = (uint16_t)(shnum - 1);
    file_at = shoff + shnum * 64;

    FILE *f = fopen(output, "wb");
    if (f == NULL) {
        die("cannot write", output);
    }
    fwrite(elf, 1, file_at, f);
    fclose(f);
    return 0;
}
