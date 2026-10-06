// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/format.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the formatter, with nothing underneath it.
 */

#include "format.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * where the output goes, and how much room is left. a `max` of zero is
 * legal and means "count, do not store", which is how a caller can
 * ask how big a buffer needs to be before allocating one
 */
struct sink {
    char  *out;
    size_t max;
    size_t written;     /* what *would* have been written */
};

static void emit(struct sink *s, char c)
{
    if (s->out != NULL && s->written + 1 < s->max) {
        s->out[s->written] = c;
    }
    s->written++;
}

static void emit_str(struct sink *s, const char *str, int width,
                     bool left, char pad)
{
    size_t len = strlen(str);
    size_t w = (width > 0) ? (size_t)width : 0;

    if (!left) {
        while (w > len) { emit(s, pad); w--; }
    }
    for (size_t i = 0; i < len; i++) {
        emit(s, str[i]);
    }
    if (left) {
        while (w > len) { emit(s, ' '); w--; }
    }
}

/*
 * a number, into a buffer the caller owns. unsigned throughout, with the
 * sign dealt with by whoever called: turning the most negative long
 * positive is undefined, and it is the one input a hand-written itoa
 * always gets wrong
 */
static void number(char *buf, uint64_t v, unsigned base, bool upper)
{
    static const char *lower_digits = "0123456789abcdef";
    static const char *upper_digits = "0123456789ABCDEF";
    const char *digits = upper ? upper_digits : lower_digits;

    char tmp[24];
    int at = 0;
    if (v == 0) {
        tmp[at++] = '0';
    }
    while (v > 0) {
        tmp[at++] = digits[v % base];
        v /= base;
    }
    int w = 0;
    while (at > 0) {
        buf[w++] = tmp[--at];
    }
    buf[w] = '\0';
}

int vsnprintf(char *out, size_t max, const char *fmt, va_list ap)
{
    struct sink s = { .out = out, .max = max, .written = 0 };

    for (; *fmt != '\0'; fmt++) {
        if (*fmt != '%') {
            emit(&s, *fmt);
            continue;
        }
        fmt++;

        bool left = false;
        char pad  = ' ';
        for (;;) {
            if (*fmt == '-')      { left = true; fmt++; }
            else if (*fmt == '0') { pad = '0'; fmt++; }
            else break;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /*
         * length modifiers, and they are *read* rather than skipped.
         *
         * the first version of this discarded them and always fetched a
         * `long`, on the grounds that everything here is 64-bit anyway.
         * that is wrong for the commonest case in C: `printf("%d", n)`
         * passes an `int`, and a vararg `int` occupies the low half of
         * its slot with the top half undefined, so reading it as a
         * long gets whatever happened to be in the high bits. it works
         * until it does not, which is the worst way for it to work
         */
        bool is_long = false;
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
            if (*fmt == 'l' || *fmt == 'z') {
                is_long = true;
            }
            fmt++;
        }

        char buf[32];
        switch (*fmt) {
        case 'd':
        case 'i': {
            long v = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
            uint64_t mag = (v < 0) ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
            char *at = buf;
            if (v < 0) {
                /*
                 * the sign goes before any zero padding, which is the
                 * difference between "-0042" and "000-42"
                 */
                if (pad == '0' && !left && width > 0) {
                    emit(&s, '-');
                    width--;
                } else {
                    *at++ = '-';
                }
            }
            number(at, mag, 10, false);
            emit_str(&s, buf, width, left, pad);
            break;
        }
        case 'u':
            number(buf, is_long ? va_arg(ap, unsigned long)
                                : (unsigned long)va_arg(ap, unsigned int),
                   10, false);
            emit_str(&s, buf, width, left, pad);
            break;
        case 'x':
            number(buf, is_long ? va_arg(ap, unsigned long)
                                : (unsigned long)va_arg(ap, unsigned int),
                   16, false);
            emit_str(&s, buf, width, left, pad);
            break;
        case 'X':
            number(buf, is_long ? va_arg(ap, unsigned long)
                                : (unsigned long)va_arg(ap, unsigned int),
                   16, true);
            emit_str(&s, buf, width, left, pad);
            break;
        case 'p':
            emit(&s, '0');
            emit(&s, 'x');
            number(buf, (uint64_t)(uintptr_t)va_arg(ap, void *), 16, false);
            emit_str(&s, buf, 0, false, ' ');
            break;
        case 'c':
            buf[0] = (char)va_arg(ap, int);
            buf[1] = '\0';
            emit_str(&s, buf, width, left, ' ');
            break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            /*
             * a null pointer prints as "(null)" rather than faulting.
             * it is somebody's bug either way, and one that prints is
             * one that can be found
             */
            emit_str(&s, (str != NULL) ? str : "(null)", width, left, ' ');
            break;
        }
        case '%':
            emit(&s, '%');
            break;
        case '\0':
            /*
             * a format ending in a bare %, stop rather than reading
             * past the end of the string looking for a specifier
             */
            emit(&s, '%');
            fmt--;
            break;
        default:
            emit(&s, '%');
            emit(&s, *fmt);
            break;
        }
    }

    if (out != NULL && max > 0) {
        size_t at = (s.written < max - 1) ? s.written : max - 1;
        out[at] = '\0';
    }
    return (int)s.written;
}

int snprintf(char *out, size_t max, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, max, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *out, const char *fmt, ...)
{
    /*
     * unbounded, and here only because ported code uses it. everything
     * else in this file goes through the bounded path
     */
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, (size_t)-1, fmt, ap);
    va_end(ap);
    return n;
}

