#!/usr/bin/env python3
# check every kprintf/panic format string against what kprintf can
# actually do.
#
# this exists because gcc's format(printf) attribute checks my format
# strings against *real* printf, so it happily accepts anything the C
# standard allows, including flags my little formatter never
# implemented. when that happens the specifier gets printed literally,
# every following argument is read into the wrong slot, and you end up
# staring at a page fault a long way from the actual mistake. ask me
# how i know.
#
#   usage: tools/checkfmt.py [kernel]

import re
import sys
import pathlib

SPEC = re.compile(r'%([-+ #0]*)([0-9]*)(\.[0-9]+)?(hh|h|ll|l|z)?([a-zA-Z%])')

SUPPORTED_FLAGS = set('-0')
SUPPORTED_LENGTH = {'', 'l', 'll', 'z'}
SUPPORTED_CONV = set('csdiuxp%')

def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'kernel')
    problems = []

    for path in sorted(root.rglob('*.[ch]')):
        for lineno, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            # inline asm is full of % too, so only look at real calls
            if 'kprintf(' not in line and 'panic(' not in line:
                continue
            for m in SPEC.finditer(line):
                flags, _width, prec, length, conv = m.groups()
                if conv == '%':
                    continue
                why = []
                if set(flags) - SUPPORTED_FLAGS:
                    why.append(f"flag '{flags}'")
                if prec:
                    why.append(f"precision '{prec}'")
                if (length or '') not in SUPPORTED_LENGTH:
                    why.append(f"length '{length}'")
                if conv not in SUPPORTED_CONV:
                    why.append(f"conversion '{conv}'")
                if why:
                    problems.append((path, lineno, m.group(0), ', '.join(why)))

    for path, lineno, spec, why in problems:
        print(f"{path}:{lineno}: kprintf cannot handle '{spec}': unsupported {why}",
              file=sys.stderr)

    if problems:
        print(f"\n{len(problems)} unsupported format specifier(s). either avoid them "
              f"or teach lib/kprintf.c the trick.", file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
