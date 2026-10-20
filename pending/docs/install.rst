the installer, as the kernel had it
===================================

cut out of velvet's ``docs/subsystems/fs.rst`` at 92087d5, the commit before
0.4.3 took the installer out of the kernel and its formatter out of the
kernel image. ``../README.md`` says what is to become of them.

making a filesystem
-------------------

``mkfs.c`` is mke2fs, in the kernel, in about the smallest form that
produces something ``ext4.c`` will mount and ``tools/readext4.py``. written
from the layout rather than from either of them. agrees is a filesystem.

it makes an *empty* filesystem: a root directory and nothing in it. no
lost+found, no reserved blocks, no sparse superblocks, no directory index.
that is the split that keeps it small: copying a system onto the new
filesystem is done afterwards through the ordinary ext4 write path, because
a formatter that also knew how to lay out files would be a second
implementation of the half that already works, and the two would drift.

the descriptor table is reserved up front (``s_reserved_gdt_blocks``) so
that growing the filesystem never has to move every bitmap and inode table
on the disk. see `resize` in the testing page.

a program asks for a resize with the ``resize`` call, root's alone, naming
the filesystem by where it is mounted and the size in its own blocks, 0
for as far as the partition and that reserved table both allow. how far
the partition allows is ``disk_resize``'s to enforce rather than its
caller's: the filesystem is handed a view that ends where its partition
does and cannot see past it, and a filesystem longer than its partition
writes over whatever is next. what happened comes back in a ``struct
user_resize``, before and after, with what was in the way of a shrink.

the installer
-------------

``install.c`` copies the machine onto a disk it can then boot from on its
own. the boot medium is not a filesystem; it is philemon in the first
sectors, a table at sector 32, and then the kernel and ramdisk as raw runs
of sectors, so installing is: copy that system area verbatim, put a
partition table after it, and format the partition. the copy is verbatim on
purpose; reading the table and rebuilding the image would be a second
implementation of ``tools/mkboot.py``.

the one thing that makes it possible: an mbr partition table lives at offset
446 of the first sector, which is the same sector the bios loads and jumps
to, and philemon's first stage ends at byte 365. that is not luck, it is the
constraint every bootloader that has ever shared a disk with a partition
table works under.

the install interface
-----------------------

copying this machine onto a disk it can then boot from on its own: the
system area verbatim, a partition table after it, and a filesystem on the
partition. what it needs from outside is function pointers for reading,
writing and saying what it is doing, which is what lets the same code be run
against a disk, a file or a fixture.

``bool (*src_read)(void *ctx, uint64_t lba, uint32_t count, void *buf);``
    the medium being installed *from*

``bool (*dst_read)(void *ctx, uint64_t lba, uint32_t count, void *buf);``
    and the one being installed *to*.

``void (*say)(void *ctx, const char *what);``
    said out loud as it goes.

``struct install_result { uint64_t system_sectors;    /* philemon, the table, the kernel, the ramdisk */``
    what it did, for the caller to report and the test to check

``uint64_t work_first_lba;``
    and the second partition, when the disk was big enough for one to be worth having.

``bool install_system(const struct install_io *io, uint32_t now, bool split, struct install_result *out, const char **error);``
    copy the system, partition what is left, and format it.

``uint64_t install_system_end(bool (*read)(void *, uint64_t, uint32_t, void *),``
    where the system area ends on a medium, by reading its table.

the mkfs interface
--------------------

making a filesystem rather than reading one, in the kernel, in the smallest
form that fs/ext4.c will mount and tools/readext4.py agrees is a filesystem.

it makes an *empty* one. copying a system onto it afterwards goes through the
ordinary write path, and a formatter that also laid out files would be a
second implementation of the half that already works.

``typedef bool (*mkfs_io)(void *ctx, uint64_t lba, uint32_t count,``
    the same shape part.c uses, for the same reason: it makes this testable against a buffer instead of a disk

``struct mkfs_result { uint64_t blocks;            /* 1 KiB each */``
    what the geometry came out as.

``bool mkfs_ext4(mkfs_io write, void *ctx, uint64_t sectors, const char *label, uint32_t now, struct mkfs_result *out, const char **error);``
    format `sectors` 512-byte sectors starting at lba 0 of whatever `write` writes to.

``#define MKFS_MIN_SECTORS 512``
    the smallest disk this can format, in sectors.
