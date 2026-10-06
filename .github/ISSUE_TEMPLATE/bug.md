---
name: bug report
about: something the machine gets wrong
title: ''
labels: bug
assignees: ''
---

<!-- one bug per issue, please. if you have found two, open two. -->

## what happens

<!-- what the machine did, and what it should have done instead. -->

## how to reproduce

<!-- the smallest sequence that shows it: exact commands, in order. -->

```sh
make
make run
# and then, at the shell:
```

## where it lives

<!-- the subsystem, as best you can tell: arch, boot, mm, sched, fs, drivers,
     net, lib, shell, user, toolchain, tools. a guess is fine and saves a
     round trip. -->

## the machine

- version: the banner prints `velvetOS v...`, and the number lives in
  `kernel/version.h`
- commit: `git rev-parse --short HEAD`
- host: `uname -a`, and the versions of gcc and nasm
- built with the machine's tools (`make`), or with the project's own
  (`make selfboot`)?
- qemu settings, if they are not the defaults. `make run` uses `CPUS=4 MEM=2G`

## the output

<!-- the serial log, or what is on the screen. bin/*-boottest.log if the boot
     test produced it. if it panicked, the whole panic: the backtrace is the
     useful part, and the registers above it. -->

## does the suite see it?

- [ ] `make test` fails
- [ ] `make boottest` fails
- [ ] it only happens by hand
