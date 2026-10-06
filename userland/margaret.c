// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/margaret.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * margaret, the attendant who keeps the books.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"
#include "textbuf.h"



#define BUFFERS 4

struct slot {
    struct textbuf buf;
    char   name[128];
    size_t cy, cx;      /* where the cursor is */
    size_t top;         /* the first line on the screen */
    bool   used;
};

static struct slot slot[BUFFERS];
static int current;

#define B  (&slot[current].buf)
#define CY (slot[current].cy)
#define CX (slot[current].cx)
#define TOP (slot[current].top)



static unsigned cols = 80, rows = 25;
static char message[160];

/* which lines on the screen are no longer what is on the screen. */
static size_t dirty_from, dirty_to;
static bool   all_dirty;
static size_t drawn_top;        /* what `top` was when it was last drawn */

static void mark(size_t line)
{
    if (dirty_from > dirty_to) {
        dirty_from = line;
        dirty_to   = line + 1;
        return;
    }
    if (line < dirty_from) { dirty_from = line; }
    if (line + 1 > dirty_to) { dirty_to = line + 1; }
}

static void mark_from(size_t line)
{
    mark(line);
    dirty_to = B->count + 1;    /* everything below it moved */
}

static void mark_all(void)
{
    all_dirty = true;
}

/* one buffer, flushed once per keystroke. */
static char out[16 * 1024];
static size_t out_len;

static void emit(const char *s, size_t n)
{
    if (out_len + n > sizeof out) {
        write_fd(STDOUT, out, (long)out_len);
        out_len = 0;
        if (n > sizeof out) {
            write_fd(STDOUT, s, (long)n);
            return;
        }
    }
    memcpy(out + out_len, s, n);
    out_len += n;
}

static void emit_str(const char *s)
{
    emit(s, strlen(s));
}

static void flush(void)
{
    if (out_len > 0) {
        write_fd(STDOUT, out, (long)out_len);
        out_len = 0;
    }
}

static void move_to(unsigned row, unsigned col)
{
    char b[32];
    snprintf(b, sizeof b, "\033[%u;%uH", row + 1, col + 1);
    emit_str(b);
}

static void clear_to_end_of_line(void)
{
    emit_str("\033[K");
}



/* the text area: everything above the status line and the two rows of shortcuts. */
#define FOOTER 3

static unsigned text_rows(void)
{
    return (rows > FOOTER + 1) ? rows - FOOTER : 1;
}

static void draw_line(size_t n)
{
    if (n < TOP || n >= TOP + text_rows()) {
        return;                          /* not on the screen */
    }
    move_to((unsigned)(n - TOP), 0);
    clear_to_end_of_line();

    if (n < B->count) {
        size_t len = B->line[n].len;
        if (len > cols) {
            len = cols;                  /* no horizontal scrolling yet */
        }
        emit(B->line[n].text, len);
    }
    /* and past the end of the file, nothing at all. */
}

/* the two rows of shortcuts, nano's arrangement. */
static void draw_help(void)
{
    static const char *row1 =
        "  ^S Save   ^O Open   ^W Find   ^N Next   ^Z Undo   ^R Redo";
    static const char *row2 =
        "  ^X Exit   ^T Name   ^B Buffer ^A Start  ^E End    ^G Help";

    move_to(rows - 2, 0);
    clear_to_end_of_line();
    emit_str(row1);
    move_to(rows - 1, 0);
    clear_to_end_of_line();
    emit_str(row2);
}

static void draw_status(void)
{
    char bar[256];
    const struct slot *s = &slot[current];

    snprintf(bar, sizeof bar,
             "  %s%s  line %lu of %lu, col %lu   [%d/%d]  %s",
             s->name[0] ? s->name : "(no name)",
             s->buf.dirty ? " *" : "",
             (unsigned long)(CY + 1), (unsigned long)B->count,
             (unsigned long)(CX + 1),
             current + 1, BUFFERS,
             message);

    move_to(rows - FOOTER, 0);
    clear_to_end_of_line();
    bar[cols < sizeof bar ? cols : sizeof bar - 1] = '\0';
    emit_str(bar);
}

static void draw(void)
{
    /*
     * the whole screen only when it really is the whole screen: the
     * first draw, a scroll, or a change of buffer. everything else
     * touches the lines that changed and the status bar.
     *
     * this is the difference the version is about. typing one character
     * used to write every line on the console
     */
    if (all_dirty || TOP != drawn_top) {
        emit_str("\033[2J");
        for (size_t i = 0; i < text_rows(); i++) {
            draw_line(TOP + i);
        }
        all_dirty = false;
        drawn_top = TOP;
    } else {
        for (size_t i = dirty_from; i < dirty_to && i < TOP + text_rows();
             i++) {
            draw_line(i);
        }
    }
    dirty_from = 1;
    dirty_to   = 0;             /* from > to: nothing waiting */

    draw_status();
    draw_help();

    /*
     * and the cursor last, so it ends where the person is typing rather
     * than wherever the last line was written
     */
    move_to((unsigned)(CY - TOP), (unsigned)(CX < cols ? CX : cols - 1));
    flush();
}



static void scroll_to_cursor(void)
{
    unsigned h = text_rows();
    if (CY < TOP) {
        TOP = CY;
    } else if (CY >= TOP + h)
{
        TOP = CY - h + 1;
    }
}

static void say(const char *s)
{
    snprintf(message, sizeof message, "%s", s);
}



static bool load(const char *path)
{
    long fd = open(path);
    if (fd < 0) {
        return false;
    }

    struct textbuf *b = B;
    char chunk[1024];
    size_t line = 0;
    long n;

    while ((n = read_fd(fd, chunk, sizeof chunk)) > 0) {
        for (long i = 0; i < n; i++) {
            if (chunk[i] == '\n') {
                textbuf_insert_line(b, ++line, NULL, 0);
            } else if (chunk[i] != '\r') {
                textbuf_insert(b, line, b->line[line].len, &chunk[i], 1);
            }
        }
    }
    close(fd);

    /* a file just opened is not a pile of edits. */
    textbuf_forget_history(b);
    return true;
}

static bool save(void)
{
    struct slot *s = &slot[current];
    if (s->name[0] == '\0') {
        say("no name, ^T gives it one");
        return false;
    }

    /*
     * FIXME: create does not empty what is already there, so saving a
     * file that has got shorter leaves the tail of the old one on the
     * end of it. delete a line, ^S, and the file on disk is longer than
     * the one on the screen, and this says "written". cp.c unlinks first
     * for exactly this reason and writes the reason down; this is the
     * caller that did not. unlink here as well, or fix it once in
     * vfs_create, where every caller would get it rather than each one
     * remembering.
     */
    long fd = create(s->name);
    if (fd < 0) {
        say("cannot write that");
        return false;
    }
    for (size_t i = 0; i < B->count; i++) {
        if (B->line[i].len > 0) {
            write_fd(fd, B->line[i].text, (long)B->line[i].len);
        }
        write_fd(fd, "\n", 1);
    }
    close(fd);
    s->buf.dirty = false;
    say("written");
    return true;
}

/* on the status bar, in raw mode, because that is where the terminal is left. */
static bool prompt(const char *what, char *into, size_t max)
{
    size_t at = strlen(into);

    for (;;) {
        char bar[256];
        snprintf(bar, sizeof bar, "  %s%s", what, into);
        move_to(rows - FOOTER, 0);
        clear_to_end_of_line();
        emit_str(bar);
        move_to(rows - FOOTER, (unsigned)strlen(bar));
        flush();

        long k = getkey();
        if (k == '\n' || k == '\r') {
            return true;
        }
        if (k == 0x1b || k == 0x07) {       /* escape, ^G */
            return false;
        }
        if (k == '\b' || k == 0x7f) {
            if (at > 0) { into[--at] = '\0'; }
            continue;
        }
        if (k >= ' ' && k < 0x7f && at + 1 < max) {
            into[at++] = (char)k;
            into[at] = '\0';
        }
    }
}



static void insert_char(char c)
{
    if (textbuf_insert(B, CY, CX, &c, 1)) {
        CX++;
        mark(CY);
    }
}

static void newline(void)
{
    if (textbuf_split(B, CY, CX)) {
        CY++;
        CX = 0;
        mark_from(CY - 1);      /* everything below moved down */
    }
}

static void backspace(void)
{
    if (CX > 0) {
        if (textbuf_delete(B, CY, CX - 1, 1)) {
            CX--;
            mark(CY);
        }
    } else if (CY > 0)
{
        size_t was = B->line[CY - 1].len;
        if (textbuf_join(B, CY - 1)) {
            CY--;
            CX = was;
            mark_from(CY);
        }
    }
}

static void delete_forward(void)
{
    if (CX < B->line[CY].len) {
        if (textbuf_delete(B, CY, CX, 1)) {
            mark(CY);
        }
    } else if (CY + 1 < B->count)
{
        if (textbuf_join(B, CY)) {
            mark_from(CY);
        }
    }
}



static void clamp(void)
{
    if (CY >= B->count) { CY = B->count - 1; }
    if (CX > B->line[CY].len) { CX = B->line[CY].len; }
}

static char needle[128];

static void find_again(bool from_here)
{
    if (needle[0] == '\0') {
        say("nothing to look for, ^W first");
        return;
    }
    size_t l, c;
    size_t start_l = from_here ? CY : 0;
    size_t start_c = from_here ? CX + 1 : 0;

    if (!textbuf_find(B, needle, start_l, start_c, &l, &c)) {
        /* not between here and the end. */
        if (!textbuf_find(B, needle, 0, 0, &l, &c)) {
            say("not found");
            return;
        }
        say("wrapped to the top");
    } else {
        say("");
    }
    CY = l;
    CX = c;
    textbuf_break_run(B);
    scroll_to_cursor();
}

int main(int argc, char **argv)
{
    unsigned c = 0, r = 0;
    if (winsize(&c, &r) == 0 && c > 20 && r > 4) {
        cols = c;
        rows = r;
    }

    for (int i = 0; i < BUFFERS; i++) {
        if (!textbuf_init(&slot[i].buf)) {
            printf("margaret: out of memory\n");
            return 1;
        }
    }
    slot[0].used = true;

    if (argc >= 2) {
        snprintf(slot[0].name, sizeof slot[0].name, "%s", argv[1]);
        if (!load(argv[1])) {
            say("new file");
        }
    }

    /*
     * raw, with signals still on: ctrl+c should not kill an editor
     * holding an unsaved file, and ^X is the way out, but a truly
     * wedged one must still be killable from another console
     */
    long was = tty_mode(TTY_RAW);

    mark_all();
    draw();

    bool running = true;
    while (running) {
        long k = getkey();
        if (k < 0) {
            continue;
        }
        message[0] = '\0';

        switch (k) {
        case KEY_LEFT:
            if (CX > 0) { CX--; }
            else if (CY > 0) { CY--; CX = B->line[CY].len; }
            textbuf_break_run(B);
            break;
        case KEY_RIGHT:
            if (CX < B->line[CY].len) { CX++; }
            else if (CY + 1 < B->count) { CY++; CX = 0; }
            textbuf_break_run(B);
            break;
        case KEY_UP:
            if (CY > 0) { CY--; }
            textbuf_break_run(B);
            break;
        case KEY_DOWN:
            if (CY + 1 < B->count) { CY++; }
            textbuf_break_run(B);
            break;
        case KEY_HOME: case 0x01:  CX = 0; break;
        case KEY_END:  case 0x05:  CX = B->line[CY].len; break;

        case KEY_PGUP:
            CY = (CY > text_rows()) ? CY - text_rows() : 0;
            textbuf_break_run(B);
            break;
        case KEY_PGDN:
            CY += text_rows();
            if (CY >= B->count) { CY = B->count - 1; }
            textbuf_break_run(B);
            break;

        case '\n': case '\r':   newline(); break;
        case '\b': case 0x7f:   backspace(); break;
        case KEY_DELETE: case 0x04: delete_forward(); break;

        case 0x1a:              /* ^Z: undo */
            if (textbuf_undo(B, &CY, &CX)) {
                mark_all();
                say("undone");
            } else {
                say("nothing to undo");
            }
            break;
        case 0x12:              /* ^R: redo */
            if (textbuf_redo(B, &CY, &CX)) {
                mark_all();
                say("redone");
            } else {
                say("nothing to redo");
            }
            break;

        case 0x17:              /* ^W: find */
            if (prompt("find: ", needle, sizeof needle)) {
                find_again(false);
            }
            mark_all();
            break;
        case 0x0e:              /* ^N: find again, from here */
            find_again(true);
            break;

        case 0x02:              /* ^B: the next buffer */
            current = (current + 1) % BUFFERS;
            slot[current].used = true;
            mark_all();
            say("buffer");
            break;

        case 0x0f:              /* ^O: open into this buffer */
            {
                char path[128] = { 0 };
                if (prompt("open: ", path, sizeof path)) {
                    textbuf_free(B);
                    textbuf_init(B);
                    CY = CX = TOP = 0;
                    snprintf(slot[current].name, sizeof slot[current].name,
                             "%s", path);
                    if (!load(path)) {
                        say("new file");
                    }
                }
                mark_all();
            }
            break;

        case 0x14:              /* ^T: name it */
            if (prompt("name: ", slot[current].name,
                       sizeof slot[current].name)) {
                say("named");
            }
            mark_all();
            break;

        case 0x13:              /* ^S: save */
            save();
            break;

        case 0x18:              /* ^X: leave */
            if (B->dirty) {
                say("unsaved, ^S to write, ^X again to abandon it");
                mark_all();
                draw();
                if (getkey() != 0x18) {
                    break;
                }
            }
            running = false;
            break;

        case 0x03:              /* ^C */
            say("^C does nothing here, ^X to leave");
            break;

        case 0x07:              /* ^G */
            say("arrows move, ^A/^E line ends, ^K cut is not here yet");
            break;

        default:
            if (k >= ' ' && k < 0x7f) {
                insert_char((char)k);
            } else if (k == '\t') {
                for (int i = 0; i < 4; i++) { insert_char(' '); }
            }
            break;
        }

        clamp();
        scroll_to_cursor();
        draw();
    }

    /* and put the terminal back exactly as it was found. */
    tty_mode((unsigned long)was);

    /* and the screen back to empty. */
    emit_str("\033[2J");
    move_to(0, 0);
    flush();

    for (int i = 0; i < BUFFERS; i++) {
        textbuf_free(&slot[i].buf);
    }
    return 0;
}
