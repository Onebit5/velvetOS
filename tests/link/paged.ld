/* the other kind of script: one that page-aligns its sections, the way
 * the kernel's does and for the same reason -- permissions come at page
 * granularity, so a .text sharing its last page with .rodata is either
 * writable code or unreadable constants.
 *
 * simple.ld is the opposite case and is kept deliberately: with nothing
 * aligned, ld packs every section into one page and gives the whole
 * thing one set of permissions. a linker that always started a new
 * segment per section matched ld on this script and not on that one,
 * which is exactly why both are here. */
ENTRY(_start)
SECTIONS
{
    . = 0x400000;

    .text : { *(.text*) }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .rodata : { *(.rodata*) }

    . = ALIGN(0x1000);
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
