booting: philemon, or a loader of your own
==========================================

velvetOS boots with philemon. he is a project of his own, like the kernel,
and this tree builds the release ``philemon-release.mk`` names, cloned
into ``obj/philemon/``; his own documentation is in
``obj/philemon/docs/philemon.rst``. the image ``make`` writes is his
format: his first sector, the rest of him, his table at block 32, then the
kernel, the ramdisk and this system's source.

why it is philemon or nothing
-----------------------------

velvet is started one way. its boot protocol, described in the kernel's
``docs/subsystems/boot.rst``, says what the machine has to look like when
the kernel's first instruction runs: long mode, the kernel's segments
where they were linked, all of memory mapped again at
``0xffff800000000000``, and a pointer in ``rdi`` to one struct describing
the memory map, the ramdisk, the acpi tables and the screen. philemon is
the only loader that does that. grub, limine, syslinux and the rest
speak multiboot, their own protocols, or linux's, and velvet has no entry
point for any of them. a drive with one of them on it and velvetOS behind
it does not boot.

the protocol has been the kernel's own since velvet 0.4.4, declared in its
``include/velvet/boot.h``. the release this tree pins today is older, and
declares the same bytes in ``include/velvet/philemon.h``, under philemon's
names; the steps below read the same against either.

choosing a loader when installing
---------------------------------

the installer comes back as a program in 0.4.2, and when it does it asks
which loader the drive gets:

``philemon``
   the default, and the only choice that boots. the drive is written the
   way the installer always wrote it: philemon in the first sectors, his
   table at block 32, the kernel and the ramdisk after it, and the
   partitions behind those.

``another loader``
   the installer writes the system and no loader at all. before it writes
   anything it shows this, and waits for an answer:

   ::

      velvetOS will not start from this drive.

      velvet, its kernel, is started one way, and philemon is the only
      loader that starts it that way. this drive gets the system, the
      kernel and the ramdisk, and no loader. the machine will not boot
      from it until a loader that speaks velvet's boot protocol is put
      on it, and none but philemon does today.

      choose this to put one there yourself. otherwise go back and
      choose philemon.

      write the drive without a loader? [y/N]

   anything but ``y`` goes back to the question.

what a loaderless drive holds
-----------------------------

everything a loader needs, where one can find it without philemon's help:

- the partition table in the mbr, with the boot code in front of it left
  as zeroes;
- the boot table at block 32, with its magic and nothing else in it, so the
  kernel still knows which drive it was started from;
- the kernel at ``/kernel/velvet`` and the ramdisk at
  ``/kernel/ramdisk.tar`` on the system partition, as files, for a loader
  that reads ext4. not under ``/boot``, because once the kernel runs that
  is where it mounts the ramdisk, over whatever the disk has there.

putting a loader on it by hand
------------------------------

this is for someone who wants to try. the machine boots at the end of it
only if the loader does what velvet's boot protocol says, and today that
means writing that part of it yourself.

#. install the loader the way its own documentation says, from another
   machine or a live system. if it fills the gap between the first sector
   and the first partition, the way grub's core image does on an mbr drive,
   it writes over the boot table at block 32, which costs the kernel
   nothing but the line saying which drive it was booted from.
#. have it load ``/kernel/velvet``, an elf64 executable, by its program
   headers, all segments moved the same distance, and
   ``/kernel/ramdisk.tar`` whole, anywhere.
#. have it build the memory map, find the rsdp, and set a linear
   framebuffer mode if it can, then build the page tables the protocol
   asks for: the kernel's segments at their linked addresses in the top
   two gigabytes, and physical memory again at ``0xffff800000000000``.
#. fill in ``struct boot_handoff`` from the kernel's
   ``include/velvet/boot.h``, magic ``0x50484c4d4e30`` and revision 1, and
   jump to the kernel's entry point in long mode, interrupts off, with the
   struct's address through the direct map in ``rdi`` and a stack of its
   own in ``rsp``.

grub can do the loading and the mode setting and nothing else: it hands a
kernel either a multiboot struct, in 32-bit protected mode, or linux's boot
parameters, and velvet takes neither. a shim that grub loads as a multiboot kernel, which
then does the last two steps and jumps, is the shortest way there. it is
not written.

and to go back, run the installer again and choose philemon, or write an
image ``make`` built straight onto the drive.
