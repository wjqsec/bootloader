Easyboot Plugins
================

By default, [Easyboot](https://gitlab.com/bztsrc/easyboot) boots Multiboot2 compliant kernels in ELF and PE formats from the boot
partition. If your kernel uses a different file format, a different boot protocol, or not located on the boot partition, then
you'll need plugins on the boot partition. You can find these in the [src/plugins](../../src/plugins) directory.

[[_TOC_]]

Installation
------------

To install plugins, simply copy them into the directory specified in the `(indir)` parameter, under the `easyboot` sub-directory
next to the menu.cfg file.

For example:
```
bootpart
|-- easyboot
|   |-- linux_x86.plg
|   |-- minix_x86.plg
|   `-- menu.cfg
|-- EFI
|   `-- BOOT
|-- kernel
`-- initrd
$ easyboot bootpart disk.img
```

Compilation
-----------

*It was obvious from the start that ELF isn't suitable for this task. It is too bloated and too complex. So originally I wanted to
use struct exec (the classic UNIX a.out format), but unfortunately modern toolchains can't create that any more. So I've decided to
create my own format and own linker for the plugins.*

You can compile the plugin's source with any standard ANSI C cross-compiler into an ELF object file, but then you'll have to use
the [plgld](../../src/misc/plgld.c) linker to create the final binary. This is an architecture agnostic cross-linker, will work no
matter what machine code the plugin was compiled for. The final .plg is just a fraction in size of the .o ELF it was generated from.

### Plugin API

The C source of a plugin must include the `src/loader.h` header file, and it must contain an `EASY_PLUGIN` line. This has one
parameter, the type of the plugin, followed by the identifier match specification. This latter is used by the loader to determine
when to use that particular plugin.

For example:

```c
#include "../loader.h"

/* magic bytes that identify a linux kernel */
EASY_PLUGIN(PLG_T_KERNEL) {
   /* offset  size  match type     magic bytes */
    { 0x1fe,     2, PLG_M_CONST, { 0xAA, 0x55, 0, 0 } },
    { 0x202,     4, PLG_M_CONST, { 'H', 'd', 'r', 'S' } }
};

/* entry point, prototype is defined by the plugin's type */
PLG_API void _start(uint8_t *buf, uint64_t size);
{
    /* prepare environment for a linux kernel */
}
```

The plugins can use several variables and functions, these are all defined in the header file and run-time linked.

```c
uint32_t verbose;
```
Verbosity level. A plugin is only allowed to print any output if this is non-zero, except for error messages. The bigger its
value is, the more details should be printed.

```c
uint64_t file_size;
```
The total size of the opened file (see `open` and `loadfile` below).

```c
uint8_t *root_buf;
```
When a file system plugin initializes, this contains the first 128k of the partition (hopefully including the super block).
Later a file system plugin can reuse this 128k buffer for whatever purpose (FAT cache, inode cache etc.)

```c
uint8_t *tags_buf;
```
Contains the Multiboot2 tags. A kernel plugin can parse this to convert the boot manager provided data into whatever format the
kernel expects. This pointer points to the start of the buffer.

```c
uint8_t *tags_ptr;
```
This pointer points to the end of the Multiboot2 tags buffer. Tag plugins might add new tags here and adjust this pointer.

```c
uint8_t *rsdp_ptr;
```
Points to the RSDP ACPI pointer.

```c
uint8_t *dsdt_ptr;
```
Points to the DSDT (or GUDT, FDT) hardware description blob.

```c
efi_system_table_t *ST;
```
On UEFI machines points to the EFI system table, otherwise `NULL`.

```c
void memset(void *dst, uint8_t c, uint32_t n);
void memcpy(void *dst, const void *src, uint32_t n);
int  memcmp(const void *s1, const void *s2, uint32_t n);
```
Mandatory memory functions (the C compiler might generate calls to these, even when there's no direct call).

```c
void *alloc(uint32_t num);
```
Allocates `num` pages (4k) of memory. Plugins must not allocate much, they must aim at minimal memory footprint.

```c
void free(void *buf, uint32_t num);
```
Free previously allocated memory of `num` pages.

```c
void printf(char *fmt, ...);
```
Prints formated string to the boot console.

```c
uint64_t pb_init(uint64_t size);
```
Starts the progress bar, `size` is the total size it represents. Returns how many bytes one pixel worth.

```c
void pb_draw(uint64_t curr);
```
Draws the progressbar for the current value. `curr` must be between 0 and total size.

```c
void pb_fini(void);
```
Closes the prograsses bar, clears its place on screen.

```c
void loadsec(uint64_t sec, void *dst);
```
Used by the file system plugins, loads a sector from disk into memory. `sec` is the sector's number, relative to the root partition.

```c
void sethooks(void *o, void *r, void *c);
```
Used by the file system plugins, sets the open / read / close functions' hooks for the root partition's file system.

```c
int open(char *fn);
```
Open a file on the root (or boot) partition for reading, returns 1 on success. Only one file can be opened at any given time.
When there was no `sethooks` call beforehand then it operates on the boot partition.

```c
uint64_t read(uint64_t offs, uint64_t size, void *buf);
```
Reads data from opened file at seek position `offs` into memory, returns number of bytes actually read.

```c
void close(void);
```
Closes the opened file.

```c
uint8_t *loadfile(char *path);
```
Load a file entirely from root (or boot) partition into a newly allocated memory buffer, and transparently uncompress it if plugin
found. Size returned in `file_size`.

```c
int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
```
Load a segment from the kernel buffer. This checks if the memory `vaddr` is available, and maps the segment if it's higher-half.
The `offs` is the file offset, so relative to the kernel buffer. If `memsz` is bigger than `filesz`, then the difference is filled
up with zeros.

```c
void _start(void);
```
Entry point for file system plugins (`PLG_T_FS`). It should parse the superblock in `root_buf` and call `sethooks`. On error
it should just return without setting its hooks.

```c
void _start(uint8_t *buf, uint64_t size);
```
Entry point for kernel plugins (`PLG_T_KERNEL`). Receives the kernel image in memory, it should relocate its segments, set up
the proper environment and transfer control over. When there's no error, it never returns.

```c
uint8_t *_start(uint8_t *buf);
```
Entry point for decompressor plugins (`PLG_T_DECOMP`). Receives the compressed buffer (and its size in `file_size`), and should
return an allocated new buffer with the uncompressed data (and set new buffer's size also in `file_size`). It must free the old
buffer (watch out, `file_size` is in bytes, but free() expects size in pages). On error, `file_size` must not be changed and it
must return the unmodified original buffer.

```c
void _start(void);
```
Entry point for tag plugins (`PLG_T_TAG`). They might add new tags at `tags_ptr` and adjust that pointer to a new, 8 bytes
aligned position.

### Local Functions

The plugins can use local functions, however because of a CLang bug, those *must* be declared as `static`. (The bug is, CLang
generates PLT records for those functions, even though "-fno-plt" flag is passed on the command line. Using `static` workarounds
this).

Low-level File Format Specification
-----------------------------------

In case someone wants to write a plugin in a non-C language (in Assembly for example), here's the low level description of the
file format.

It is very similar to the a.out format. The file consist of a fixed sized header, followed by varying length sections. There's no
section header, each section's data directly follows the previous section's data in the following order:

```
(header)
(identifier match records)
(relocation records)
(machine code)
(read-only data)
(initialized readable-writable data)
```

For the first real section, machine code, the alignment is included. For all the other sections, padding is added to the previous
section's size.

HINT: if you pass a plugin as a single argument to `plgld`, then it dumps the sections in the file with an output similar to
`readelf -a` or `objdump -xd`.

### Header

All numbers are in little-endian format regardless to the architecture.

| Offset  | Size  | Description                                                    |
|--------:|------:|----------------------------------------------------------------|
|       0 |     4 | magic bytes `EPLG`                                             |
|       4 |     4 | total size of the file                                         |
|       8 |     4 | total memory required when file is loaded                      |
|      12 |     4 | size of the code section                                       |
|      16 |     4 | size of the read-only data section                             |
|      20 |     4 | plugin's entry point                                           |
|      24 |     2 | architecture code (same as ELF's)                              |
|      26 |     2 | number of relocation records                                   |
|      28 |     1 | number of identifier match records                             |
|      29 |     1 | highest referenced GOT entry                                   |
|      30 |     1 | file format revision (0 for now)                               |
|      31 |     1 | plugin's type (1=file system, 2=kernel, 3=decompressor, 4=tag) |

The architecture code is the same as in ELF headers, for example 62 = x86_64, 183 = Aarch64 and 243 = RISC-V.

The plugin's type specifies the entry point's prototype, the ABI is always SysV.

### Identifier Match Section

This section contains as many of the following records as many specified in the header's "number of identifier match records" field.

| Offset  | Size  | Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     2 | offset                                                   |
|       2 |     1 | size                                                     |
|       3 |     1 | type                                                     |
|       4 |     4 | magic bytes to match                                     |

First, the beginning of subject is loaded into a buffer. An accumulator is set up, initially with a 0. Offsets in these records are
always relative to this accumulator, and they address that byte in the buffer.

Type field tells how to interpret the offset. If it's a 1, then the offset plus the accumulator is used as the value. If it's 2,
then a 8-bit byte value is taken at offset, 3 means take a 16-bit word value, and 4 means take a 32-bit dword value. 5 means take
a 8-bit byte value and add the accumulator to it, 6 means take a 16-bit word value and add the accumulator to it, and 7 is the same
but with 32-bit value. 8 will search for the magic bytes from the accumulatorth byte to the end of the buffer in offset steps, and
if found, returns the matching offset as the value.

If size is zero, then the accumulator is set to the value. If the size is not zero, then that many bytes is checked if they match
the given magic bytes.

For example, to check if a PE executable starts with a NOP instruction:
```c
EASY_PLUGIN(PLG_T_KERNEL) {
   /* offset  size  match type     magic bytes */
    { 0,         2, PLG_M_CONST, { 'M', 'Z', 0, 0 } },      /* check magic bytes */
    { 60,        0, PLG_M_DWORD, { 0, 0, 0, 0 } },          /* get the PE header's offset to the accumulator */
    { 0,         4, PLG_M_CONST, { 'P', 'E', 0, 0 } },      /* check magic bytes */
    { 40,        1, PLG_M_DWORD, { 0x90, 0, 0, 0 } }        /* check for NOP instruction at entry point */
};
```

### Relocation Section

This section contains as many of the following records as many specified in the header's "number of relocation records" field.

| Offset  | Size  | Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | offset                                                   |
|       4 |     4 | relocation type                                          |

Meaning of bits in type:

| From    | To    | Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     7 | symbol (0 - 255)                                         |
|       8 |     8 | PC relative addressing                                   |
|       9 |     9 | GOT relative indirect addressing                         |
|      10 |    13 | immediate mask index (0 - 15)                            |
|      14 |    19 | start bit (0 - 63)                                       |
|      20 |    25 | end bit (0 - 63)                                         |
|      26 |    31 | negated address flag bit position (0 - 63)               |

The offset field is relative to the magic in the plugin's header, and it selects an integer in memory where the relocation must be
performed.

The symbol tells which address to use. 0 means the BASE address where the plugin was loaded in memory, aka. the address of the
header's magic in memory. Other values select an external symbol address from the GOT, defined in the loader or in another plugin,
have a look at the `plg_got` array in the plgld.c's source to see which value corresponds to which symbol. If GOT relative bit is
1, then the address of the symbol's GOT entry is used, instead of the symbol's actual address.

If PC relative bit is 1, then the offset is first subtracted from the address (intrustion pointer relative addressing mode).

The immediate mask index tells which bits store the address in the instruction. If this is 0, then address is written as-is to
the offset, regardless to the architecture. For x86_64, only index 0 is allowed. For ARM Aarch64: 0 = as-is, 1 = 0x07ffffe0 (shift
to the left 5 bits), 2 = 0x07fffc00 (shift to the left 10 bits), 3 = 0x60ffffe0 (with ADR/ADRP instructions the immediate is
shifted and splitted into two bitgroups). Future architectures might define more and different immediate bitmasks.

Using the immediate mask, end - start + 1 bits are taken from the memory, and signed extended. This value is added to the address
(addend, and in case of internal references, the internal symbol's address is also encoded here).

If the negated address flag bit is not 0 and the address is positive, then that bit is cleared. If the address is negative, then
that bit is set and the address is negated.

Finally, the start and end bits selects which portion of the address to write to the selected integer. This also defines the size
of the relocation, bits outside of this range and those not being part of the immediate mask are left unchanged.

### Code Section

This section contains machine instructions for the architecture specified in the header, and has as many bytes as the code size
field says.

### Read-only Data Section

This is an optional section, might be missing. It is as long as the read-only section size field in the header says. All constant
variables are placed in this section.

### Initialized Data Section

This is an optional section, might be missing. If there are still bytes in the file after the code section (or the optional
read-only data section), then those bytes are all considered the data section. If a variable is initialized with a non-zero value,
then it is placed in this section.

### BSS Section

This is an optional section, might be missing. This section is never stored in the file. If the in memory size field is bigger
than the file size field in the header, then their difference will be filled with zeros in memory. If a variable isn't initialized,
or initialized as zero, then it is placed in this section.
