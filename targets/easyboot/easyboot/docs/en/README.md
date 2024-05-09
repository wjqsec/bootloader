Booting kernels with Easyboot
=============================

[Easyboot](https://gitlab.com/bztsrc/easyboot) is an all-in-one boot manager and bootable disk image creator that can load
various OS kernels and Multiboot2 compliant kernels in various formats.

[[_TOC_]]

Installation
------------

```
 easyboot [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e] [-c] <indir> <outfile>

  -v, -vv         increase verbosity / validation
  -s <mb>         set the disk image size in Megabytes (defaults to 35M)
  -b <mb>         set the boot partition size in Megabytes (defaults to 33M)
  -u <guid>       set the boot partition's unique identifier (defaults to random)
  -p <t> <u> <i>  add a root partition (type guid, unique guid, imagefile)
  -e              add El Torito Boot Catalog (EFI CDROM boot support)
  -c              always create a new image file even if it exists
  <indir>         use the contents of this directory for the boot partition
  <outfile>       output image or device file name
```

The **Easyboot** tool creates a bootable disk image named `(outfile)` using GUID Partitioning Table with a single partition
formatted as FAT32 and named as "EFI System Partition" (ESP in short). The contents of that partition are taken from the `(indir)`
you provide. You must place a simple plain text configuration file in that directory called `easyboot/menu.cfg`. With either NL or
CRLF line endings, you can easily edit this with any text editor. Depending on your configuration, you might also need some
[plugins](plugins.md) in this directory called `easyboot/*.plg`.

The image can be booted on Raspberry Pi too, and it will work in qemu; but to boot on a real machine, you'll need further firmware
files `bootcode.bin`, `fixup.dat`, `start.elf` and a .dtb file in the `(indir)` directory, these can be downloaded from the
Raspberry Pi's [official repository](https://github.com/raspberrypi/firmware/tree/master/boot).

The tool also has some optional command line flags: `-s (mb)` sets the overall size of the generated disk image in Megabytes, while
`-b (mb)` sets the size of the boot partition in Megabytes. Obviously the former must be bigger than the latter. If not specified,
then partition size is calculated from the size of the given directory (33 Mb at a minimum, the smallest FAT32 there can be) and
disk size defaults to 2 Mb bigger than that (due to alignment and space needed for the partitioning table). If there's a more than
2 Mb gap between these two size values, then you can use 3rd party tools like `fdisk` to add more partitions to the image to your
liking (or see `-p` below). If you want a predictable layout, you can also specify the boot partition's unique identifier
(UniquePartitionGUID) with the `-u <guid>` flag.

Optionally you can also add extra partition(s) with the `-p` flag. This requires 3 arguments: (PartitionTypeGUID),
(UniquePartitionGUID) and the name of the image file that contains the contents of the partition.

The `-e` flag adds El Torito Boot Catalog to the generated image, so that it can be booted not just as an USB stick but as an EFI
CDROM too.

If `(outfile)` is a device file (eg. `/dev/sda` on Linux, `/dev/disk0` on BSDs, and `\\.\PhysicalDrive0` on Windows), then it does
not create GPT nor ESP, instead it locates the already existing ones on the device. It still copies all files in `(indir)` to the
boot partition and installs the loaders. This also works if `(outfile)` is an image file that already exists (in this case use the
`-c` flag to always create a new, empty image file first).

Configuration
-------------

The `easyboot/menu.cfg` file may contain the following lines (very similar to grub.cfg's syntax, you can find an example
configuration file [here](menu.cfg)):

### Comments

All lines starting with a `#` are considered comments and skipped till the end of the line.

### Verbosity level

You can set the verbosity level using a line starting with `verbose`.

```
verbose (0-3)
```

This tells the loader how much information to print to the boot console. Verbose 0 means totally quiet (default) and verbose 3
will dump the loaded kernel segments and the machine code at the entry point.

### Framebuffer

You can request a specific screen resolution with the line starting with `framebuffer`. The format is as follows:

```
framebuffer (width) (height) (bits per pixel) [#(fgcolor)] [#(bgcolor)] [#(pbarbg)]
```

**Easyboot** will set up a framebuffer for you, even if this line doesn't exists (800 x 600 x 32bpp by default). But if this line
does exist, then it will try to set the specified resolution. Paletted modes not supported, so bits per pixel has to be 15 at least.

If the fourth optional color parameter is given, then it has to be in HTML notation starting with a hashmark followed by 6
hexadecimal digits, RRGGBB. For example `#ff0000` is full bright red and `#007f7f` is a darker cyan. It sets the foreground or with
other words the font's color. Likewise if the fifth optional color parameter is given, it also has to be in HTML notation, and it
sets the background color. The last optional color parameter is likewise, and it sets the progress bar's background color.

When colors not given, then it defaults to white letters on black background, and the progress bar's background is dark gray.

### Default boot option

Line starting with `default` sets which menu entry should be booted without user interaction after the specified timeout.

```
default (menuentry index) (timeout msec)
```

Menuentry index is a number between 1 to 9. Timeout is in milliseconds (one thousandth of a second), so 1000 gives one second.

### Menu alignment

Lines starting with `menualign` alter how the boot options are displayed.

```
menualign ("vertical"|"horizontal")
```

By default the menu is horizontal, which you can change to vertical.

### Boot options

You can specify up to 9 menuentries with lines starting with `menuentry`. The format is as follows:

```
menuentry (icon) [label]
```

For each icon, an `easyboot/(icon).tga` file on the boot partition must exists. The image must be in a run length encoded,
color-mapped [Targa format](TGA.txt), because that's the most compressed variant (first three bytes of the file must be
`0`, `1` and `9` in this order, see Data Type 9 in the specification). Its dimensions must be exactly 64 pixels tall and 64
pixels wide.

To save in this format from GIMP, first select "Image > Mode > Indexed...", in the pop-up window set "Maximum number of colors"
to 256. Then select "File > Export As...", enter a filename which ends in `.tga`, and in the pop-up window check "RLE compression".
For a command line conversion tool, you can use ImageMagick, `convert (any image file) -colors 256 -compress RLE icon.tga`.

The optional label parameter is for displaying ASCII version or release information on the menu, and not for arbitrary strings,
therefore to save space, UTF-8 not supported unless you also provide `easyboot/font.sfn`. (A UNICODE font requires lots of storage,
even though the [Scalable Screen Font](https://gitlab.com/bztsrc/scalable-font2) is very efficient, it's still about 820K. In
contrast, GRUB's unicode.pf2 is much bigger, about 2392K although both fonts contain ca. 55600 glyphs in 8x16 and 16x16 bitmaps.
The embedded ASCII font has 8x16 bitmaps and 96 glyphs only.)

All lines that come after a `menuentry` line will belong to that menuentry, except when that line is another menuentry line.
For convenience, you can use blocks like in GRUB, but these are just syntactic sugar. Curly braces are handled as whitespace
characters. You can omit them and use tabs instead just like in a Python script or Makefile if that's your preference.

For example
```
menuentry FreeBSD backup
{
    kernel bsd.old/boot
}
```

### Select partition

Line starting with `partition` selects a GPT partition. Must be preceded by a `menuentry` line.

```
partition (partition unique UUID)
```

This partition will be used as the root file system for the boot option. The kernel as well as the modules will be loaded from
this partition, and depending on the boot protocol, it will also be passed to the kernel. The specified partition might reside
on a different disk than the boot disk, **Easyboot** will iterate on all GPT partitioned disks during boot to locate it.

For convenience, the partition is also looked for on the `kernel` line (see below). If the given boot command line contains a
`root=(UUID)` or `root=*=(UUID)` string, then there's no need for a separate `partition` line.

When the partition is not explicitly specified then the kernel as well as the modules are loaded from the boot partition.

### Specify a kernel

The line starting with `kernel` tells what file should be booted, and with what parameters. Must be preceded by a `menuentry`
line.

```
kernel (path to your kernel file) (optional boot command line arguments)
```

The path must point to an existing file, an executable kernel binary, and it has to be an absolute UTF-8 path on the root (or boot)
partition. If the kernel isn't in the root directory of the partition, then the directory separator is always `/`, even on UEFI
systems. If the name contains a space, then that must be escaped with `\`. The path might be followed by command line arguments,
separated by a space. For Multiboot2 compliant kernels, these command line arguments will be passed in the *Boot command line*
(type 1) tag. They won't be altered by **Easyboot**, neither parsed except searched for the partition specifier.

By default **Easyboot** can boot Multiboot2 compliant kernels in ELF64 and PE32+/COFF formats (and on UEFI systems, UEFI
applications as well). Normally that protocol does not allow higher-half kernels, but **Easyboot** violates the protocol a little
bit in a way that does not break normal, non-higher-half kernels. If you want to boot any other kernel, then you'll need a kernel
loader [plugin](plugins.md).

NOTE: unlike GRUB, where you have to use special commands like "linux" or "multiboot" to select the boot protocol, here there's
just one command and the protocol is autodetected from your kernel in run-time.

### Loading further modules

You can load arbitrary files (initial ramdisks, kernel drivers, etc.) along with the kernel using lines starting with `module`.
Must be preceded by a `menuentry` line. Note that this line can be repeated multiple times within each menuentry. If the
boot protocol supports an initrd, then the very first `module` line is considered as the initrd.

```
module (path to a file) (optional module command line arguments)
```

The path must point to an existing file, and it has to be an absolute UTF-8 path on the root (or boot) partition. It might be
followed by command line arguments, separated by a space. If the file is compressed and there's a uncompression [plugin](plugins.md)
for it, then the module will be transparently uncompressed. Information about these loaded (and uncompressed) modules will be passed
to a Multiboot2 compliant kernel in the *Modules* (type 3) tags.

The special case is if the module starts with the bytes `DSDT`, `GUDT` or `0xD00DFEED`. In these cases the file won't be added to
the *Modules* tags, rather the ACPI table will be patched so that its DSDT pointers will point to the contents of this file. With
this you can easily replace a buggy BIOS' ACPI table with a user provided one.

### Boot splash logo

You can also display a logo at the center of the screen when the boot option is selected if you place a line starting with
`bootsplash`. Must be preceded by a `menuentry` line.

```
bootsplash [#(bgcolor)] (path to a tga file)
```

The background color is optional, and has to be in HTML notation starting with a hashmark followed by 6 hexadecimal digits, RRGGBB.
If the first argument does not start with `#`, then a path argument is assumed.

The path must point to an existing file, and it has to be an absolute UTF-8 path on the boot (NOT root) partition. The image must
be in a run length encoded, color-mapped Targa format, just like the menu icons. Dimensions could be anything that fits on screen.

### Multicore Support

To start the kernel on all processor cores at once, specify the `multicore` directive. Must be preceded by a `menuentry` line
and also requires the [smp](../../src/plugins/smp.c) plugin to be installed.

```
multicore
```

Troubleshooting
---------------

If you encounter any problems, just run with `easyboot -vv` flag. This will perform validation and will output the results
verbosely at image creation time. Otherwise add `verbose 3` to `easyboot/menu.cfg` to get detailed boot time messages.

If you see `PMBR-ERR` string on the top left corner with red background, then that means your CPU is very old and does not
support 64-bit long mode or the boot sector was unable to bootstrap the loader. Might occur only on BIOS machines, this can
never happen with UEFI or on RaspberryPi.

| Message                             | Description                                                                       |
|-------------------------------------|-----------------------------------------------------------------------------------|
| `Booting [X]...`                    | indicates that boot option (menuentry index) X was choosen                        |
| `Loading 'X' (Y bytes)...`          | file X of Y length is being loaded                                                |
| `Parsing kernel...`                 | kernel was found, now detecting it's format                                       |
| `Starting X boot...`                | shows that the boot loader of X system was detected                               |
| `Starting X kernel...`              | shows that the kernel of X system was detected                                    |
| `Transfering X control to Y`        | indicates that X mode entry point at Y address is about to be called              |

If you encounter any problems after you see that last message, then that means the problem has happened in the operating system's
boot procedure, and not in the **Easyboot** loader, so you'll have to consult the given operating system's documentation for an
answer. Otherwise please feel free to open an [issue](https://gitlab.com/bztsrc/easyboot/-/issues) on gitlab.

### Multiboot1

Required plugins: [grubmb1](../../src/plugins/grubmb1.c)

### Multiboot2

This is the most flexible, with multiple variation supported via plugins:

- ELF64 or PE32+ with simplified Multiboot2: no plugins required
- ELF32 with simplified Multiboot2 and 32-bit entry point: [elf32](../../src/plugins/elf32.c)
- a.out (struct exec) with simplified Multiboot2 and 32-bit entry point: [aout](../../src/plugins/aout.c)
- GRUB-compatible Multiboot2 with 32-bit entry point: [grubmb2](../../src/plugins/grubmb2.c)

Note the difference: [simplified Multiboot2](ABI.md) requires no embedded tags, supports higher-half kernels, clean 64-bit entry
point with parameters passed according to the Multiboot2, SysV and fastcall ABI as well.

On the other hand [GRUB-compatible Multiboot2](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) needs embedded
tags, does not support higher-half nor 64-bit kernels, the entry point is always 32-bit and parameters passed in `eax`, `ebx` only.

### Windows

No plugins required, but you must set up [SecureBoot](secureboot.md).

```
menuentry win {
  kernel EFI/Microsoft/BOOT/BOOTMGRW.EFI
}
```

### Linux

Required plugins: [linux](../../src/plugins/linux.c), [ext234](../../src/plugins/ext234.c)

If the kernel is not placed on the boot partition then you can use the command line to specify the root partition.

```
menuentry linux {
  kernel vmlinuz-linux root=PARTUUID=01020304-0506-0708-0a0b0c0d0e0f1011
}
```

### OpenBSD

Required plugins: [obsdboot](../../src/plugins/obsdboot.c), [ufs44](../../src/plugins/ufs44.c)

```
menuentry openbsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot
}
```

WARNING! Do not use the [elf32](../../src/plugins/elf32.c) plugin if you're booting OpenBSD! Its `boot` incorrectly claims to be
an ELF with 32-bit SysV ABI entry point, but actually it has a 16-bit real mode entry.

### FreeBSD

Required plugins: [fbsdboot](../../src/plugins/fbsdboot.c), [ufs2](../../src/plugins/ufs2.c)

On legacy BIOS systems, specify the loader `boot`.

```
menuentry freebsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot/boot
}
```

On UEFI machines, use `loader.efi` on the boot partition (no plugins necessary).

```
menuentry freebsd {
  kernel boot/loader.efi
}
```

If your root file system is ZFS then copy this one file (`boot` on BIOS, `loader.efi` on UEFI) to `(indir)`, and DO NOT specify
a root partition.

### FreeDOS

Required plugins: [fdos](../../src/plugins/fdos.c)

Move FreeDOS' files to `(indir)` (FreeDOS will use boot partition as root partition).

```
menuentry freedos {
  kernel KERNEL.SYS
}
```

If booting stops after printing copyright and `- InitDisk`, then the FreeDOS kernel was compiled without FAT32 support. Download
a different kernel, with `f32` in its name.

### ReactOS

Required plugins: [reactos](../../src/plugins/reactos.c)

```
menuentry reactos {
  kernel FREELDR.SYS
}
```

### MenuetOS

Required plugins: [menuet](../../src/plugins/menuet.c)

```
menuentry menuetos {
  kernel KERNEL.MNT
  module CONFIG.MNT
  module RAMDISK.MNT
}
```

To turn auto configuration off, add `noauto` to the command line.

### KolibriOS

Required plugins: [kolibri](../../src/plugins/kolibri.c)

```
menuentry kolibrios {
  kernel KERNEL.MNT syspath=/rd/1/ launcher_start=1
  module KOLIBRI.IMG
  module DEVICES.DAT
}
```

The plugin works on UEFI machines too, but you can also use `uefi4kos.efi` on the boot partition (and no plugin required).

### SerenityOS

Required plugins: [grubmb1](../../src/plugins/grubmb1.c)

```
menuentry serenityos {
  kernel boot/Prekernel
  module boot/Kernel
}
```

### Haiku

Required plugins: [grubmb1](../../src/plugins/grubmb1.c), [befs](../../src/plugins/befs.c)

```
menuentry haiku {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel system/packages/haiku_loader-r1~beta4_hrev56578_59-1-x86_64.hpkg
}
```

On UEFI machines, use `haiku_loader.efi` on the boot partition (no plugins necessary).
