/*
 * src/loader.h
 * https://gitlab.com/bztsrc/easyboot
 *
 * Copyright (C) 2023 bzt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @brief Defines for the Easyboot loader
 */

#include <stdint.h>

#define EASYBOOT_MAGIC      "Easyboot"

/**
 * The longest path we can handle
 */
#define PATH_MAX 256
/**
 * Maximum size of MBI tags in pages (30 * 4096 = 122k)
 */
#define TAGS_MAX 30

#ifndef NULL
#define NULL (void*)0
#endif
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#define FB_COLOR(r,g,b) ((((r) >> (8 - vidmode.framebuffer_red_mask_size)) << vidmode.framebuffer_red_field_position) |\
                         (((g) >> (8 - vidmode.framebuffer_green_mask_size)) << vidmode.framebuffer_green_field_position) |\
                         (((b) >> (8 - vidmode.framebuffer_blue_mask_size)) << vidmode.framebuffer_blue_field_position))

/*** ELF defines and structs ***/

#define ELFMAG      "\177ELF"
#define SELFMAG     4
#define EI_CLASS    4       /* File class byte index */
#define ELFCLASS32  1       /* 32-bit objects */
#define ELFCLASS64  2       /* 64-bit objects */
#define EI_DATA     5       /* Data encoding byte index */
#define ELFDATA2LSB 1       /* 2's complement, little endian */
#define PT_LOAD     1       /* Loadable program segment */
#define PT_DYNAMIC  2       /* Dynamic linking information */
#define PT_INTERP   3       /* Program interpreter */
#define PF_X (1 << 0)       /* Segment is executable */
#define PF_W (1 << 1)       /* Segment is writable */
#define PF_R (1 << 2)       /* Segment is readable */
#define EM_386      3       /* Intel 80386 */
#define EM_X86_64   62      /* AMD x86_64 architecture */
#define EM_AARCH64  183     /* ARM aarch64 architecture */
#define EM_RISCV    243     /* RISC-V riscv64 architecture */

typedef struct {
  unsigned char e_ident[16];/* Magic number and other info */
  uint16_t  e_type;         /* Object file type */
  uint16_t  e_machine;      /* Architecture */
  uint32_t  e_version;      /* Object file version */
  uint32_t  e_entry;        /* Entry point virtual address */
  uint32_t  e_phoff;        /* Program header table file offset */
  uint32_t  e_shoff;        /* Section header table file offset */
  uint32_t  e_flags;        /* Processor-specific flags */
  uint16_t  e_ehsize;       /* ELF header size in bytes */
  uint16_t  e_phentsize;    /* Program header table entry size */
  uint16_t  e_phnum;        /* Program header table entry count */
  uint16_t  e_shentsize;    /* Section header table entry size */
  uint16_t  e_shnum;        /* Section header table entry count */
  uint16_t  e_shstrndx;     /* Section header string table index */
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
  unsigned char e_ident[16];/* Magic number and other info */
  uint16_t  e_type;         /* Object file type */
  uint16_t  e_machine;      /* Architecture */
  uint32_t  e_version;      /* Object file version */
  uint64_t  e_entry;        /* Entry point virtual address */
  uint64_t  e_phoff;        /* Program header table file offset */
  uint64_t  e_shoff;        /* Section header table file offset */
  uint32_t  e_flags;        /* Processor-specific flags */
  uint16_t  e_ehsize;       /* ELF header size in bytes */
  uint16_t  e_phentsize;    /* Program header table entry size */
  uint16_t  e_phnum;        /* Program header table entry count */
  uint16_t  e_shentsize;    /* Section header table entry size */
  uint16_t  e_shnum;        /* Section header table entry count */
  uint16_t  e_shstrndx;     /* Section header string table index */
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
  uint32_t  p_type;         /* Segment type */
  uint32_t  p_offset;       /* Segment file offset */
  uint32_t  p_vaddr;        /* Segment virtual address */
  uint32_t  p_paddr;        /* Segment physical address */
  uint32_t  p_filesz;       /* Segment size in file */
  uint32_t  p_memsz;        /* Segment size in memory */
  uint32_t  p_align;        /* Segment alignment */
} __attribute__((packed)) Elf32_Phdr;

typedef struct {
  uint32_t  p_type;         /* Segment type */
  uint32_t  p_flags;        /* Segment flags */
  uint64_t  p_offset;       /* Segment file offset */
  uint64_t  p_vaddr;        /* Segment virtual address */
  uint64_t  p_paddr;        /* Segment physical address */
  uint64_t  p_filesz;       /* Segment size in file */
  uint64_t  p_memsz;        /* Segment size in memory */
  uint64_t  p_align;        /* Segment alignment */
} __attribute__((packed)) Elf64_Phdr;

/*** PE32+ defines and structs ***/

#define MZ_MAGIC                    0x5a4d      /* "MZ" */
#define PE_MAGIC                    0x00004550  /* "PE\0\0" */
#define IMAGE_FILE_MACHINE_I386     0x014c      /* Intel 386 architecture */
#define IMAGE_FILE_MACHINE_AMD64    0x8664      /* AMD x86_64 architecture */
#define IMAGE_FILE_MACHINE_ARM64    0xaa64      /* ARM aarch64 architecture */
#define IMAGE_FILE_MACHINE_RISCV64  0x5064      /* RISC-V riscv64 architecture */
#define PE_OPT_MAGIC_PE32PLUS       0x020b      /* PE32+ format */
#define IMAGE_SUBSYSTEM_EFI_APPLICATION 0xA     /* EFI Application */

typedef struct {
  uint16_t  magic;        /* MZ magic */
  uint16_t  reserved[29]; /* reserved */
  uint32_t  peaddr;       /* address of pe header */
} __attribute__((packed)) mz_hdr;

typedef struct {
  uint32_t  magic;        /* PE magic */
  uint16_t  machine;      /* machine type */
  uint16_t  sections;     /* number of sections */
  uint32_t  timestamp;    /* time_t */
  uint32_t  sym_table;    /* symbol table offset */
  uint32_t  numsym;       /* number of symbols */
  uint16_t  opt_hdr_size; /* size of optional header */
  uint16_t  flags;        /* flags */
  uint16_t  file_type;    /* file type, PE32PLUS magic */
  uint8_t   ld_major;     /* linker major version */
  uint8_t   ld_minor;     /* linker minor version */
  uint32_t  text_size;    /* size of text section(s) */
  uint32_t  data_size;    /* size of data section(s) */
  uint32_t  bss_size;     /* size of bss section(s) */
  uint32_t  entry_point;  /* file offset of entry point */
  uint32_t  code_base;    /* relative code addr in ram */
  union {
    struct {
      uint32_t data_base; /* the preferred load address */
      uint32_t img_base;  /* the preferred load address */
    } pe32;
    struct {
      uint64_t img_base;  /* the preferred load address */
    } pe64;
  } data;
  uint32_t secalign;
  uint32_t filealign;
  uint32_t osver;
  uint32_t imgver;
  uint32_t subver;
  uint32_t winver;
  uint32_t imgsize;
  uint32_t hdrsize;
  uint32_t chksum;
  uint16_t subsystem;
} __attribute__((packed)) pe_hdr;

typedef struct {
  char      name[8];      /* section name */
  uint32_t  vsiz;         /* virtual size */
  uint32_t  vaddr;        /* virtual address */
  uint32_t  rsiz;         /* size of raw data */
  uint32_t  raddr;        /* pointer to raw data */
  uint32_t  reloc;        /* pointer to relocations */
  uint32_t  ln;           /* pointer to line numbers */
  uint16_t  nreloc;       /* number of relocations */
  uint16_t  nln;          /* number of line numbers */
  uint32_t  chr;          /* characteristics */
} __attribute__((packed)) pe_sec;

/*** EFI ***/

#ifndef EFIAPI
# ifdef _MSC_EXTENSIONS
#  define EFIAPI __cdecl
# else
#  ifdef __x86_64__
#   define EFIAPI __attribute__((ms_abi))
#  else
#   define EFIAPI
#  endif
# endif
#endif
#define EFI_ERROR(a)           (((intn_t) a) < 0)
#define EFI_SUCCESS                            0
#define EFI_LOAD_ERROR        0x8000000000000001
#define EFI_UNSUPPORTED       0x8000000000000003
#define EFI_BUFFER_TOO_SMALL  0x8000000000000005

typedef void     *efi_handle_t;
typedef void     *efi_event_t;
typedef int64_t  intn_t;
typedef uint8_t  boolean_t;
typedef uint64_t uintn_t;
typedef uint64_t efi_status_t;
typedef uint64_t efi_physical_address_t;
typedef uint64_t efi_virtual_address_t;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} efi_allocate_type_t;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
} efi_memory_type_t;

typedef struct {
  uint32_t  Type;
  uint32_t  Pad;
  uint64_t  PhysicalStart;
  uint64_t  VirtualStart;
  uint64_t  NumberOfPages;
  uint64_t  Attribute;
} efi_memory_descriptor_t;
#define NextMemoryDescriptor(Ptr,Size)  ((efi_memory_descriptor_t *) (((uint8_t *) Ptr) + Size))

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} efi_locate_search_type_t;

typedef struct {
  uint32_t  Data1;
  uint16_t  Data2;
  uint16_t  Data3;
  uint8_t   Data4[8];
} __attribute__((packed)) guid_t;

typedef struct {
    uint8_t     Type;
    uint8_t     SubType;
    uint8_t     Length[2];
} efi_device_path_t;

typedef struct {
    uint64_t    Signature;
    uint32_t    Revision;
    uint32_t    HeaderSize;
    uint32_t    CRC32;
    uint32_t    Reserved;
} efi_table_header_t;

typedef struct {
    uint16_t    Year;       /* 1998 - 2XXX */
    uint8_t     Month;      /* 1 - 12 */
    uint8_t     Day;        /* 1 - 31 */
    uint8_t     Hour;       /* 0 - 23 */
    uint8_t     Minute;     /* 0 - 59 */
    uint8_t     Second;     /* 0 - 59 */
    uint8_t     Pad1;
    uint32_t    Nanosecond; /* 0 - 999,999,999 */
    int16_t     TimeZone;   /* -1440 to 1440 or 2047 */
    uint8_t     Daylight;
    uint8_t     Pad2;
} efi_time_t;

/* Simple text input / output interface */
typedef struct {
    uint16_t    ScanCode;
    uint16_t    UnicodeChar;
} efi_input_key_t;

typedef efi_status_t (EFIAPI *efi_input_reset_t)(void *This, boolean_t ExtendedVerification);
typedef efi_status_t (EFIAPI *efi_input_read_key_t)(void *This, efi_input_key_t *Key);

typedef struct {
    efi_input_reset_t           Reset;
    efi_input_read_key_t        ReadKeyStroke;
    efi_event_t                 WaitForKey;
} simple_input_interface_t;

typedef efi_status_t (EFIAPI *efi_text_reset_t)(void *This, boolean_t ExtendedVerification);
typedef efi_status_t (EFIAPI *efi_text_output_string_t)(void *This, uint16_t *String);
typedef efi_status_t (EFIAPI *efi_text_test_string_t)(void *This, uint16_t *String);
typedef efi_status_t (EFIAPI *efi_text_query_mode_t)(void *This, uintn_t ModeNumber, uintn_t *Column, uintn_t *Row);
typedef efi_status_t (EFIAPI *efi_text_set_mode_t)(void *This, uintn_t ModeNumber);
typedef efi_status_t (EFIAPI *efi_text_set_attribute_t)(void *This, uintn_t Attribute);
typedef efi_status_t (EFIAPI *efi_text_clear_screen_t)(void *This);
typedef efi_status_t (EFIAPI *efi_text_set_cursor_t)(void *This, uintn_t Column, uintn_t Row);
typedef efi_status_t (EFIAPI *efi_text_enable_cursor_t)(void *This,  boolean_t Enable);

typedef struct {
    int32_t                     MaxMode;
    int32_t                     Mode;
    int32_t                     Attribute;
    int32_t                     CursorColumn;
    int32_t                     CursorRow;
    boolean_t                   CursorVisible;
} simple_text_output_mode_t;

typedef struct {
    efi_text_reset_t            Reset;
    efi_text_output_string_t    OutputString;
    efi_text_test_string_t      TestString;
    efi_text_query_mode_t       QueryMode;
    efi_text_set_mode_t         SetMode;
    efi_text_set_attribute_t    SetAttribute;
    efi_text_clear_screen_t     ClearScreen;
    efi_text_set_cursor_t       SetCursorPosition;
    efi_text_enable_cursor_t    EnableCursor;
    simple_text_output_mode_t   *Mode;
} simple_text_output_interface_t;

/* EFI services */

typedef struct {
  efi_physical_address_t Memory;
  uintn_t NoPages;
} memalloc_t;

typedef efi_status_t (EFIAPI *efi_allocate_pages_t)(efi_allocate_type_t Type, efi_memory_type_t MemoryType,
    uintn_t NoPages, efi_physical_address_t *Memory);
typedef efi_status_t (EFIAPI *efi_free_pages_t)(efi_physical_address_t Memory, uintn_t NoPages);
typedef efi_status_t (EFIAPI *efi_get_memory_map_t)(uintn_t *MemoryMapSize, efi_memory_descriptor_t *MemoryMap,
    uintn_t *MapKey, uintn_t *DescriptorSize, uint32_t *DescriptorVersion);
typedef efi_status_t (EFIAPI *efi_allocate_pool_t)(efi_memory_type_t PoolType, uintn_t Size, void **Buffer);
typedef efi_status_t (EFIAPI *efi_free_pool_t)(void *Buffer);
typedef efi_status_t (EFIAPI *efi_handle_protocol_t)(efi_handle_t Handle, guid_t *Protocol, void **Interface);
typedef efi_status_t (EFIAPI *efi_locate_handle_t)(efi_locate_search_type_t SearchType, guid_t *Protocol,
    void *SearchKey, uintn_t *BufferSize, efi_handle_t *Buffer);
typedef efi_status_t (EFIAPI *efi_image_load_t)(boolean_t BootPolicy, efi_handle_t ParentImageHandle, efi_device_path_t *FilePath,
    void *SourceBuffer, uintn_t SourceSize, efi_handle_t *ImageHandle);
typedef efi_status_t (EFIAPI *efi_image_start_t)(efi_handle_t ImageHandle, uintn_t *ExitDataSize, void **ExitData);
typedef efi_status_t (EFIAPI *efi_exit_boot_services_t)(efi_handle_t ImageHandle, uintn_t MapKey);
typedef efi_status_t (EFIAPI *efi_stall_t)(uintn_t Microseconds);
typedef efi_status_t (EFIAPI *efi_open_protocol_t)(efi_handle_t Handle, guid_t *Protocol, void **Interface,
    efi_handle_t AgentHandle, efi_handle_t ControllerHandle, uint32_t Attributes);
typedef efi_status_t (EFIAPI *efi_locate_protocol_t)(guid_t *Protocol, void *Registration, void **Interface);

typedef struct {
    efi_table_header_t          Hdr;

    void*                       RaiseTPL;
    void*                       RestoreTPL;

    efi_allocate_pages_t        AllocatePages;
    efi_free_pages_t            FreePages;
    efi_get_memory_map_t        GetMemoryMap;
    efi_allocate_pool_t         AllocatePool;
    efi_free_pool_t             FreePool;

    void*                       CreateEvent;
    void*                       SetTimer;
    void*                       WaitForEvent;
    void*                       SignalEvent;
    void*                       CloseEvent;
    void*                       CheckEvent;

    void*                       InstallProtocolInterface;
    void*                       ReinstallProtocolInterface;
    void*                       UninstallProtocolInterface;
    efi_handle_protocol_t       HandleProtocol;
    efi_handle_protocol_t       PCHandleProtocol;
    void*                       RegisterProtocolNotify;
    efi_locate_handle_t         LocateHandle;
    void*                       LocateDevicePath;
    void*                       InstallConfigurationTable;

    efi_image_load_t            LoadImage;
    efi_image_start_t           StartImage;
    void*                       Exit;
    void*                       UnloadImage;
    efi_exit_boot_services_t    ExitBootServices;

    void*                       GetNextHighMonotonicCount;
    efi_stall_t                 Stall;
    void*                       SetWatchdogTimer;

    void*                       ConnectController;
    void*                       DisconnectController;

    efi_open_protocol_t         OpenProtocol;
    void*                       CloseProtocol;
    void*                       OpenProtocolInformation;

    void*                       ProtocolsPerHandle;
    void*                       LocateHandleBuffer;
    efi_locate_protocol_t       LocateProtocol;
    void*                       InstallMultipleProtocolInterfaces;
    void*                       UninstallMultipleProtocolInterfaces;

    void*                       CalculateCrc32;
} efi_boot_services_t;

/* EFI system table */

#define ACPI_TABLE_GUID                 { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }
#define ACPI_20_TABLE_GUID              { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81} }
#define SMBIOS_TABLE_GUID               { 0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }

typedef struct {
    guid_t      VendorGuid;
    void        *VendorTable;
} efi_configuration_table_t;

typedef struct {
    efi_table_header_t              Hdr;

    uint16_t                        *FirmwareVendor;
    uint32_t                        FirmwareRevision;

    efi_handle_t                    ConsoleInHandle;
    simple_input_interface_t        *ConIn;

    efi_handle_t                    ConsoleOutHandle;
    simple_text_output_interface_t  *ConOut;

    efi_handle_t                    ConsoleErrorHandle;
    simple_text_output_interface_t  *StdErr;

    void                            *RuntimeServices;
    efi_boot_services_t             *BootServices;

    uintn_t                         NumberOfTableEntries;
    efi_configuration_table_t       *ConfigurationTable;
} efi_system_table_t;

/* Device Path Protocol */

#define EFI_DEVICE_PATH_PROTOCOL_GUID  { 0x09576E91, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

typedef struct {
    uint8_t     Type;               /* 1 - Hardware Device Path */
    uint8_t     SubType;            /* 3 - Memory Mapped Path */
    uint8_t     Length[2];
    uint32_t    MemoryType;         /* 2 - EfiLoaderData */
    uint64_t    StartAddress;
    uint64_t    EndAddress;
} efi_memory_mapped_device_path_t;

typedef struct {
    uint8_t     Type;               /* 4 - Media Device Path */
    uint8_t     SubType;            /* 1 - Hard Disk */
    uint8_t     Length[2];
    uint32_t    PartitionNumber;
    uint64_t    PartitionStart;
    uint64_t    PartitionSize;
    guid_t      PartitionSignature; /* UniquePartitionGUID */
    uint8_t     PartitionFormat;    /* 2 - GPT */
    uint8_t     SignatureFormat;    /* 2 - GUID */
} __attribute__((packed)) efi_hard_disk_device_path_t;

/* Loaded image protocol */

#define EFI_LOADED_IMAGE_PROTOCOL_GUID { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

typedef struct {
    uint32_t                Revision;
    efi_handle_t            ParentHandle;
    void                    *SystemTable;
    efi_handle_t            DeviceHandle;
    efi_device_path_t       *FilePath;
    void                    *Reserved;
    uint32_t                LoadOptionsSize;
    void                    *LoadOptions;
    void                    *ImageBase;
    uint64_t                ImageSize;
    efi_memory_type_t       ImageCodeType;
    efi_memory_type_t       ImageDataType;
} efi_loaded_image_protocol_t;

/* Simple File System Protocol */

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID { 0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }
#define EFI_FILE_INFO_GUID  { 0x9576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define EFI_FILE_MODE_READ      0x0000000000000001

typedef struct {
    uint64_t                Size;
    uint64_t                FileSize;
    uint64_t                PhysicalSize;
    efi_time_t              CreateTime;
    efi_time_t              LastAccessTime;
    efi_time_t              ModificationTime;
    uint64_t                Attribute;
    uint16_t                FileName[262];
} efi_file_info_t;

typedef struct efi_file_handle_s efi_file_handle_t;

typedef efi_status_t (EFIAPI *efi_volume_open_t)(void *This, efi_file_handle_t **Root);
typedef struct {
    uint64_t                Revision;
    efi_volume_open_t       OpenVolume;
} efi_simple_file_system_protocol_t;

typedef efi_status_t (EFIAPI *efi_file_open_t)(efi_file_handle_t *File, efi_file_handle_t **NewHandle, uint16_t *FileName,
    uint64_t OpenMode, uint64_t Attributes);
typedef efi_status_t (EFIAPI *efi_file_close_t)(efi_file_handle_t *File);
typedef efi_status_t (EFIAPI *efi_file_delete_t)(efi_file_handle_t *File);
typedef efi_status_t (EFIAPI *efi_file_read_t)(efi_file_handle_t *File, uintn_t *BufferSize, void *Buffer);
typedef efi_status_t (EFIAPI *efi_file_write_t)(efi_file_handle_t *File, uintn_t *BufferSize, void *Buffer);
typedef efi_status_t (EFIAPI *efi_file_get_pos_t)(efi_file_handle_t *File, uint64_t *Position);
typedef efi_status_t (EFIAPI *efi_file_set_pos_t)(efi_file_handle_t *File, uint64_t Position);
typedef efi_status_t (EFIAPI *efi_file_get_info_t)(efi_file_handle_t *File, guid_t *InformationType, uintn_t *BufferSize,
    void *Buffer);
typedef efi_status_t (EFIAPI *efi_file_set_info_t)(efi_file_handle_t *File, guid_t *InformationType, uintn_t BufferSize,
    void *Buffer);
typedef efi_status_t (EFIAPI *efi_file_flush_t)(efi_file_handle_t *File);

struct efi_file_handle_s {
    uint64_t                Revision;
    efi_file_open_t         Open;
    efi_file_close_t        Close;
    efi_file_delete_t       Delete;
    efi_file_read_t         Read;
    efi_file_write_t        Write;
    efi_file_get_pos_t      GetPosition;
    efi_file_set_pos_t      SetPosition;
    efi_file_get_info_t     GetInfo;
    efi_file_set_info_t     SetInfo;
    efi_file_flush_t        Flush;
};

/* Block IO Protocol */

#ifndef EFI_BLOCK_IO_PROTOCOL_GUID
#define EFI_BLOCK_IO_PROTOCOL_GUID { 0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define EFI_BLOCK_IO_PROTOCOL_REVISION    0x00010000
#define EFI_BLOCK_IO_INTERFACE_REVISION   EFI_BLOCK_IO_PROTOCOL_REVISION

#endif

typedef struct {
    uint32_t                MediaId;
    boolean_t               RemovableMedia;
    boolean_t               MediaPresent;
    boolean_t               LogicalPartition;
    boolean_t               ReadOnly;
    boolean_t               WriteCaching;
    uint32_t                BlockSize;
    uint32_t                IoAlign;
    uint64_t                LastBlock;
} efi_block_io_media_t;

typedef efi_status_t (EFIAPI *efi_block_read_t)(void *This, uint32_t MediaId, uint64_t LBA, uintn_t BufferSize, void *Buffer);

typedef struct {
    uint64_t                Revision;
    efi_block_io_media_t    *Media;
    void*                   Reset;
    efi_block_read_t        ReadBlocks;
    void*                   WriteBlocks;
    void*                   FlushBlocks;
} efi_block_io_t;

/* Graphics Output Protocol */

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID { 0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

typedef enum {
  PixelRedGreenBlueReserved8BitPerColor,
  PixelBlueGreenRedReserved8BitPerColor,
  PixelBitMask,
  PixelBltOnly,
  PixelFormatMax
} efi_gop_pixel_format_t;

typedef struct {
    uint32_t                RedMask;
    uint32_t                GreenMask;
    uint32_t                BlueMask;
    uint32_t                ReservedMask;
} efi_gop_pixel_bitmask_t;

typedef struct {
    uint32_t                Version;
    uint32_t                HorizontalResolution;
    uint32_t                VerticalResolution;
    efi_gop_pixel_format_t  PixelFormat;
    efi_gop_pixel_bitmask_t PixelInformation;
    uint32_t                PixelsPerScanLine;
} efi_gop_mode_info_t;

typedef struct {
    uint32_t                MaxMode;
    uint32_t                Mode;
    efi_gop_mode_info_t     *Information;
    uintn_t                 SizeOfInfo;
    efi_physical_address_t  FrameBufferBase;
    uintn_t                 FrameBufferSize;
} efi_gop_mode_t;

typedef efi_status_t (EFIAPI *efi_gop_query_mode_t)(void *This, uint32_t ModeNumber, uintn_t *SizeOfInfo,
    efi_gop_mode_info_t **Info);
typedef efi_status_t (EFIAPI *efi_gop_set_mode_t)(void *This, uint32_t ModeNumber);
typedef efi_status_t (EFIAPI *efi_gop_blt_t)(void *This, uint32_t *BltBuffer, uintn_t BltOperation,
    uintn_t SourceX, uintn_t SourceY, uintn_t DestinationX, uintn_t DestionationY, uintn_t Width, uintn_t Height, uintn_t Delta);

typedef struct {
    efi_gop_query_mode_t    QueryMode;
    efi_gop_set_mode_t      SetMode;
    efi_gop_blt_t           Blt;
    efi_gop_mode_t          *Mode;
} efi_gop_t;

/* EDID Protocol */

#define EFI_EDID_ACTIVE_GUID     { 0xbd8c1056, 0x9f36, 0x44ec, { 0x92, 0xa8, 0xa6, 0x33, 0x7f, 0x81, 0x79, 0x86 } }
#define EFI_EDID_DISCOVERED_GUID { 0x1c0c34f6, 0xd380, 0x41fa, { 0xa0, 0x49, 0x8a, 0xd0, 0x6c, 0x1a, 0x66, 0xaa } }

typedef struct {
  uint32_t SizeOfEdid;
  uint8_t *Edid;
} efi_edid_t;

/* GUID Partitioning Table */

#define EFI_PTAB_HEADER_ID  "EFI PART"
#define EFI_PART_TYPE_EFI_SYSTEM_PART_GUID  { 0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b} }
/* this is actually the same as 1 << GRUB_GPT_PART_ATTR_OFFSET_LEGACY_BIOS_BOOTABLE */
#define EFI_PART_USED_BY_OS 0x0000000000000004

typedef struct {
  uint64_t  Signature;
  uint32_t  Revision;
  uint32_t  HeaderSize;
  uint32_t  CRC32;
  uint32_t  Reserved;
  uint64_t  MyLBA;
  uint64_t  AlternateLBA;
  uint64_t  FirstUsableLBA;
  uint64_t  LastUsableLBA;
  guid_t    DiskGUID;
  uint64_t  PartitionEntryLBA;
  uint32_t  NumberOfPartitionEntries;
  uint32_t  SizeOfPartitionEntry;
  uint32_t  PartitionEntryArrayCRC32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
  guid_t    PartitionTypeGUID;
  guid_t    UniquePartitionGUID;
  uint64_t  StartingLBA;
  uint64_t  EndingLBA;
  uint64_t  Attributes;
  uint16_t  PartitionName[36];
} __attribute__((packed)) gpt_entry_t;

/* EFI System Partition */

typedef struct {
  uint8_t   jmp[3];
  char      oem[8];
  uint16_t  bps;
  uint8_t   spc;
  uint16_t  rsc;
  uint8_t   nf;
  uint16_t  nr;
  uint16_t  ts16;
  uint8_t   media;
  uint16_t  spf16;
  uint16_t  spt;
  uint16_t  nh;
  uint32_t  hs;
  uint32_t  ts32;
  uint32_t  spf32;
  uint32_t  flg;
  uint32_t  rc;
  char      vol[6];
  char      fst[8];
  char      dmy[20];
  char      fst2[8];
} __attribute__((packed)) esp_bpb_t;

typedef struct {
  char      name[8];
  char      ext[3];
  uint8_t   attr[9];
  uint16_t  ch;
  uint32_t  attr2;
  uint16_t  cl;
  uint32_t  size;
} __attribute__((packed)) esp_dir_t;

/*** Simpleboot (Multiboot2) ***/

/* This should be in the first kernel parameter as well as in %eax. */
#define MULTIBOOT2_BOOTLOADER_MAGIC         0x36d76289

/*  Alignment of multiboot modules. */
#define MULTIBOOT_MOD_ALIGN                 0x00001000

/*  Alignment of the multiboot info structure. */
#define MULTIBOOT_INFO_ALIGN                0x00000008

/*  Flags set in the ’flags’ member of the multiboot header. */

#define MULTIBOOT_TAG_ALIGN                 8
#define MULTIBOOT_TAG_TYPE_END              0
#define MULTIBOOT_TAG_TYPE_CMDLINE          1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT_TAG_TYPE_MODULE           3
#define MULTIBOOT_TAG_TYPE_MMAP             6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER      8
#define MULTIBOOT_TAG_TYPE_EFI64            12
#define MULTIBOOT_TAG_TYPE_SMBIOS           13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD         14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW         15
#define MULTIBOOT_TAG_TYPE_EFI64_IH         20
/* additional, not in the original Multiboot2 spec */
#define MULTIBOOT_TAG_TYPE_EDID             256
#define MULTIBOOT_TAG_TYPE_SMP              257

/* Multiboot2 information header */
typedef struct {
  uint32_t  total_size;
  uint32_t  reserved;
} multiboot_info_t;

/* common tag header */
typedef struct {
  uint32_t  type;
  uint32_t  size;
} multiboot_tag_t;

/* Boot command line (type 1) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  char      string[0];
} multiboot_tag_cmdline_t;

/* Boot loader name (type 2) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  char      string[0];
} multiboot_tag_loader_t;

/* Modules (type 3) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint32_t  mod_start;
  uint32_t  mod_end;
  char      string[0];
} multiboot_tag_module_t;

/* Memory Map (type 6) */
#define MULTIBOOT_MEMORY_AVAILABLE          1
#define MULTIBOOT_MEMORY_RESERVED           2
/* original EFI type stored in "reserved" field */
#define MULTIBOOT_MEMORY_UEFI               MULTIBOOT_MEMORY_RESERVED
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE   3
#define MULTIBOOT_MEMORY_NVS                4
#define MULTIBOOT_MEMORY_BADRAM             5
typedef struct {
  uint64_t  base_addr;
  uint64_t  length;
  uint32_t  type;
  uint32_t  reserved;       /* original EFI Memory Type */
} multiboot_mmap_entry_t;

typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint32_t  entry_size;
  uint32_t  entry_version;
  multiboot_mmap_entry_t entries[0];
} multiboot_tag_mmap_t;

/* Framebuffer info (type 8) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint64_t  framebuffer_addr;
  uint32_t  framebuffer_pitch;
  uint32_t  framebuffer_width;
  uint32_t  framebuffer_height;
  uint8_t   framebuffer_bpp;
  uint8_t   framebuffer_type; /* must be 1 */
  uint16_t  reserved;
  uint8_t   framebuffer_red_field_position;
  uint8_t   framebuffer_red_mask_size;
  uint8_t   framebuffer_green_field_position;
  uint8_t   framebuffer_green_mask_size;
  uint8_t   framebuffer_blue_field_position;
  uint8_t   framebuffer_blue_mask_size;
} multiboot_tag_framebuffer_t;

/* EFI 64-bit image handle pointer (type 12) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint64_t  pointer;
} multiboot_tag_efi64_t;

/* SMBIOS tables (type 13) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint8_t   major;
  uint8_t   minor;
  uint8_t   reserved[6];
  uint8_t   tables[0];
} multiboot_tag_smbios_t;

/* ACPI old RSDP (type 14) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint8_t   rsdp[0];
} multiboot_tag_old_acpi_t;

/* ACPI new RSDP (type 15) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint8_t   rsdp[0];
} multiboot_tag_new_acpi_t;

/* EFI 64-bit image handle pointer (type 20) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint64_t  pointer;
} multiboot_tag_efi64_ih_t;

/* EDID supported monitor resolutions (type 256) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint8_t   edid[0];
} multiboot_tag_edid_t;

/* SMP supported (type 257) */
typedef struct {
  uint32_t  type;
  uint32_t  size;
  uint32_t  numcores;
  uint32_t  running;
  uint32_t  bspid;
} multiboot_tag_smp_t;

/*** Easyboot plugins ***/

/* MUST match itemcount in plg_got[] array in plgld.c */
#define PLG_NUMSYM 25
/* do not include the plugin API in a hosted (not freestanding) environment */
#if !defined(_STDIO_H) && !defined(_STDIO_H_) && !defined(HOSTED)
#ifdef __x86_64__
/* workaround a serious Clang fuck-up:
 * you can use ms_abi varargs in a sysv object (with function attribute ms_abi and __builtin_ms_va_list),
 * but you can't use sysv varargs in a ms_abi object (such as an uefi application, because there's no __builtin_sysv_va_list)
 * so we must use that messed up ms_abi on x86_64, because the compiler doesn't support anything else */
#define PLG_API       __attribute__((ms_abi))
#else
/* all the other architectures */
#define PLG_API       __attribute__((sysv_abi))
#endif
extern uint32_t verbose;
extern uint64_t file_size;
extern uint8_t *root_buf, *tags_buf, *tags_ptr, *rsdp_ptr, *dsdt_ptr;
extern efi_system_table_t *ST;
extern PLG_API void memset(void *dst, uint8_t c, uint32_t n);
extern PLG_API void memcpy(void *dst, const void *src, uint32_t n);
extern PLG_API int  memcmp(const void *s1, const void *s2, uint32_t n);
extern PLG_API void *alloc(uint32_t num);
extern PLG_API void free(void *buf, uint32_t num);
extern PLG_API void printf(char *fmt, ...);
extern PLG_API uint64_t pb_init(uint64_t size);
extern PLG_API void pb_draw(uint64_t curr);
extern PLG_API void pb_fini(void);
extern PLG_API void loadsec(uint64_t sec, void *dst);
extern PLG_API void sethooks(void *o, void *r, void *c);
extern PLG_API int  open(char *fn);
extern PLG_API uint64_t read(uint64_t offs, uint64_t size, void *buf);
extern PLG_API void close(void);
extern PLG_API uint8_t *loadfile(char *path);
extern PLG_API int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
/* MUST match plg_got[] array in plgld.c */
typedef struct {
  uint32_t *verbose;
  uint64_t *file_size;
  uint8_t **root_buf;
  uint8_t **tags_buf;
  uint8_t **tags_ptr;
  uint8_t **rsdp_ptr;
  uint8_t **dsdt_ptr;
  efi_system_table_t **ST;
  void (PLG_API *memset)(void*, uint8_t, uint32_t);
  void (PLG_API *memcpy)(void*, const void*, uint32_t);
  int  (PLG_API *memcmp)(const void*, const void*, uint32_t);
  void* (PLG_API *alloc)(uint32_t);
  void (PLG_API *free)(void*, uint32_t);
  void (PLG_API *printf)(char*, ...);
  uint64_t (PLG_API *pb_init)(uint64_t);
  void (PLG_API *pb_draw)(uint64_t);
  void (PLG_API *pb_fini)(void);
  void (PLG_API *loadsec)(uint64_t, void*);
  void (PLG_API *sethooks)(void*, void*, void*);
  int  (PLG_API *open)(char*);
  uint64_t (PLG_API *read)(uint64_t, uint64_t, void*);
  void (PLG_API *close)(void);
  uint8_t* (PLG_API *loadfile)(char*);
  int (PLG_API *loadseg)(uint32_t, uint32_t, uint64_t, uint32_t);
} plg_got_t;
/* when root partition isn't defined, then open/read/close should use the functions that use the boot partition */
PLG_API int fw_open(char *fn);
PLG_API uint64_t fw_read(uint64_t offs, uint64_t size, void *buf);
PLG_API void fw_close(void);
PLG_API uint8_t *fw_loadfile(char *path);
PLG_API int fw_loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
#endif /* !HOSTED */

/* file format defines */
#define PLG_MAGIC     "EPLG"

/* plugin types */
enum { PLG_T_FS=1, PLG_T_KERNEL, PLG_T_DECOMP, PLG_T_TAG };

/* match types */
enum { PLG_M_CONST=1 /* match at offs */, PLG_M_BYTE /* byte[offs] */, PLG_M_WORD /* word[offs] */, PLG_M_DWORD /* dword[offs] */,
    PLG_M_BADD /* add byte[offs] */, PLG_M_WADD /* add word[offs] */, PLG_M_DADD /* add dword[offs] */, PLG_M_SRCH /* search */ };

typedef struct {
  uint16_t  offs;     /* offset */
  uint8_t   size;     /* number of bytes to match */
  uint8_t   type;     /* one of PLG_M_x enums above */
  uint8_t   magic[4]; /* bytes to match */
} __attribute__((packed)) plg_id_t;

/* relocation record definitions */
#define PLG_A_ABS     0x000 /* absolute address */
#define PLG_A_PCREL   0x100 /* PC relative address */
#define PLG_A_GOTREL  0x200 /* GOT relative address */
/* immediate masks. Index 0 is common to all architectures, indeces 1 - 15 are architecture specific */
#define PLG_IM_0      0xffffffffffffffffUL
#define PLG_IM_AARCH64_1 0x07ffffe0UL
#define PLG_IM_AARCH64_2 0x07fffc00UL
#define PLG_IM_AARCH64_3 0x60ffffe0UL

/* relocation type:
 * bit  0 -  7: symbol index (see plg_got[] in plgld.c and the API function list above)
 * bit  8:      PC relative address mode
 * bit  9:      GOT relative indirect address mode
 * bit 10 - 13: immediate mask index (see PLG_IM_x)
 * bit 14 - 19: start bit offset
 * bit 20 - 25: end bit offset
 * bit 26 - 31: neg flag bit position */
#define PLG_R_SYM(t) ((t) & 0xff)
#define PLG_R_IMSK(t) (((t) >> 10) & 15)
#define PLG_R_SBIT(t) (((t) >> 14) & 63)
#define PLG_R_EBIT(t) (((t) >> 20) & 63)
#define PLG_R_NBIT(t) (((t) >> 26) & 63)
typedef struct {
  uint32_t  offs;     /* offset */
  uint32_t  type;     /* relocation type */
} __attribute__((packed)) plg_rel_t;

typedef struct {
  uint8_t   magic[4]; /* "EPLG" */
  uint32_t  filesz;   /* total size in file */
  uint32_t  memsz;    /* total size in memory (filesz + bss size) */
  uint32_t  codesz;   /* code section size */
  uint32_t  rosz;     /* rodata section size */
  uint32_t  entry;    /* entry point */
  uint16_t  arch;     /* same as Elf64_Ehdr.e_machine */
  uint16_t  nrel;     /* number of relocation records */
  uint8_t   nid;      /* number of identifier match records */
  uint8_t   ngot;     /* number of used GOT entries (highest referenced entry) */
  uint8_t   rev;      /* file format revision */
  uint8_t   type;     /* plugin type, one of the PLG_T_x enums above */
  /* followed by nid plg_id_t identifier match records (to detect when this plugin is used) */
  /* followed by nrel plg_rel_t relocation records */
  /* followed by text section */
  /* followed by rodata section (if any) */
  /* followed by data section (if any) */
  /* followed by bss section (if any, not stored in file, just added to memsz) */
} __attribute__((packed)) plg_hdr_t;          /* sizeof header = 32 */

/* note: this restriction applies to ELF in the macro below, and not to the Easyboot Plugins! */
#define PLG_MAXIDS 15
/* hacky way to construct a proper ELF note section record... if there's an official way to do it, I haven't found it yet */
#define EASY_PLUGIN(a) const __attribute__((section(".note"),aligned(4))) uint32_t plg_type[3] = { 0, PLG_MAXIDS * sizeof(plg_id_t), \
  0xEA570000|(a) }; const __attribute__((section(".note"),aligned(4))) plg_id_t plg_match[PLG_MAXIDS] =
