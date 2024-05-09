Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) is an all-in-one boot *manager* and bootable disk image creator that can load
various OS kernels and Multiboot2 compliant kernels in various binary formats.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

NOTE: If you're looking for a boot *loader*, that loads a single kernel only, then take a look at **Easyboot**'s little brother,
[Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Acknowledgement
---------------

This project wouldn't have been possible without the support of [Free Software Foundation Hungary](https://fsf.hu/nevjegy).
The project was created in appliance of FSF.hu's Free Software Tender 2023.

Advantages over GRUB
--------------------

- dependency-free, easy to use, multilingual installer
- not bloated, it's only about 0.5% of GRUB's size
- easy to configure, with syntax checker
- easily patch ACPI tables with user provided DSDT
- shows a very detailed error screen if kernel goes off at early stage
- Multiboot2: clean 64-bit entry point (no need to embed tags nor for 32-bit trampoline code in kernel)
- Multiboot2: support for higher-half kernels
- Multiboot2: firmware independent, consistent memory map on all platforms
- Multiboot2: firmware independent, consistent framebuffer on all platforms
- Multiboot2: the monitor's EDID info also provided
- Multiboot2: if requested, starts the kernel on all processor cores (SMP support)
- there's also a plugin to simulate GRUB booting with all its pitfalls and bugs.

Supported kernels: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/en/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/en/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

Supported file systems: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(When the kernel is located on the boot partition then any file system can be used for root: ZFS, btrfs, etc.)

Rationale
---------

I've created a simple to use boot loader, and users were asking for more and more features. I wanted to keep that boot loader as
simple as possible, but FSF.hu offered support, so I've decided to fork it and add all those requested features in this boot manager
instead.

This is also a [suckless](https://suckless.org) tool like Simpleboot, has no dependencies, and extremely easy to use:

1. create a directory and put your boot files in it, among other things your menu configuration and the optional plugins
2. execute the `easyboot (source directory) (output image file)` command
3. and... that's about it... nothing else left to do! The image *Just Works (TM)*, it will get your kernels booted!

You can install the boot manager and make an existing device or image bootable; or you can create a bootable image anew. You can
boot that image in a VM, or you can write it with `dd` or [USBImager](https://bztsrc.gitlab.io/usbimager/) to a storage and boot
that on a real machine too.

Simplicity is the ultimate sophistication!

Installation
------------

Just download the binary for your OS. These are portable executables, they don't require installation and they don't need any
shared libraries / DLLs.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

Furthermore you can find various packaging solutions in the [distrib](distrib) directory (for Debian, Ubuntu, RaspiOS, Gentoo,
Arch).

When creating an image then (depending on your configuration) you might also need some plugins in your `(source directory)`. You
can find these in the [src/plugins](src/plugins) directory using the extension `plg`. When installed from a package, these will be
placed under `/usr/share/easyboot`.

Documentation
-------------

The detailed [documentation](docs/en) on how to use the bootable disk creator and how a kernel is booted can be found in the docs
directory.

Example kernel
--------------

If you want to write a kernel that can be loaded without any plugins using **Easyboot**'s [simplified Multiboot2](docs/en/ABI.md)
protocol, then take a look at [Simpleboot Example Kernel](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example) directory. Both
loaders use the same boot protocol, that kernel works with **Easyboot** as-is. You'll see that no Assembly nor embedded tags
required, otherwise the source code is 99.9% the same as the example in the Multiboot2 specification (the only difference is, it
prints to the serial console and not on the VGA teletype screen, because that latter doesn't exists on UEFI and on RaspberryPi
machines).

Compilation
-----------

GNU/make needed for orchestration (although it's literally just `cc easyboot.c -o easyboot`). The toolchain doesn't matter,
any ANSI C compiler will do, works on POSIX and WIN32 MINGW too. Just go to the [src](src) directory and run `make`. That's all.
Despite of it's small size, it is self-contained and compilation has exactly zero library dependencies. It's not called **Easyboot**
for nothing :-)

To recompile the loaders, you'll need the [flatassembler](https://flatassembler.net), and LLVM Clang and lld (gcc and GNU ld
won't work I'm afraid). But don't worry, I've added all of them to `src/data.h` as a byte array, so you don't have to compile
these unless you really really want to (for that, just delete data.h before you run make).

On the other hand to compile the plugins, you'll have to have a cross-compiler, either LLVM CLang or GNU gcc (x86_64-elf-gcc,
aarch64-elf-gcc). The repo includes these as binaries too. To do the recompilation, just delete the `src/plugins/*.plg` files
before running make.

License
-------

**Easyboot** is Free and Open Source Software, licensed under the terms of GPL version 3 or (at your opinion) any later version.
See the [LICENSE](LICENSE) file for details.

bzt
