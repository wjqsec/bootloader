override MAKEFLAGS += -rR

include $(TOOLCHAIN_FILE)
export CC_FOR_TARGET
export LD_FOR_TARGET
export OBJDUMP_FOR_TARGET
export OBJCOPY_FOR_TARGET
export READELF_FOR_TARGET

TARGET ?=
BUILDDIR ?=

override SRCDIR := $(shell pwd -P)

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)),)
    $(error BUILDDIR not specified)
endif

COM_OUTPUT ?= false
E9_OUTPUT ?= false

override S2CFLAGS := -Os

override BASE_CFLAGS := $(CFLAGS_FOR_TARGET)

override CFLAGS_FOR_TARGET += \
    -g \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wvla \
    $(WERROR_FLAG) \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -ffunction-sections \
    -fdata-sections \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-omit-frame-pointer \
    -fno-strict-aliasing \
    -fno-lto \
    -ffile-prefix-map='$(call SHESCAPE,$(SRCDIR))'=.

override CPPFLAGS_FOR_TARGET := \
    -isystem ../freestanding-headers \
    -I'$(call SHESCAPE,$(BUILDDIR))/..' \
    -I. \
    $(CPPFLAGS_FOR_TARGET) \
    -DCOM_OUTPUT=$(COM_OUTPUT) \
    -DE9_OUTPUT=$(E9_OUTPUT) \
    -MMD \
    -MP

$(call MKESCAPE,$(BUILDDIR))/./flanterm/backends/fb.o: override CPPFLAGS_FOR_TARGET += \
    -DFLANTERM_FB_DISABLE_BUMP_ALLOC \
    -DFLANTERM_FB_ENABLE_MASKING

ifeq ($(TARGET),bios)
    override CFLAGS_FOR_TARGET += \
        -fno-PIE \
        -fno-PIC \
        -m32 \
        -march=i686 \
        -mno-80387
    override CPPFLAGS_FOR_TARGET := \
        $(CPPFLAGS_FOR_TARGET) \
        -DBIOS
    override NASM_TARGET := \
        -DIA32_TARGET
endif

ifeq ($(TARGET),uefi-x86-64)
    override CFLAGS_FOR_TARGET += \
        -fPIE \
        -fshort-wchar \
        -m64 \
        -march=x86-64 \
        -mno-80387 \
        -mno-mmx \
        -mno-sse \
        -mno-sse2 \
        -mno-red-zone
    override CPPFLAGS_FOR_TARGET := \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc' \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc/x86_64' \
        $(CPPFLAGS_FOR_TARGET) \
        -DUEFI
    override NASM_TARGET := \
        -DX86_64_TARGET
endif

ifeq ($(TARGET),uefi-ia32)
    override CFLAGS_FOR_TARGET += \
        -fPIE \
        -fshort-wchar \
        -m32 \
        -march=i686 \
        -mno-80387
    override CPPFLAGS_FOR_TARGET := \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc' \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc/ia32' \
        $(CPPFLAGS_FOR_TARGET) \
        -DUEFI
    override NASM_TARGET := \
        -DIA32_TARGET
endif

ifeq ($(TARGET),uefi-aarch64)
    override CFLAGS_FOR_TARGET += \
        -fPIE \
        -fshort-wchar \
        -mgeneral-regs-only
    override CPPFLAGS_FOR_TARGET := \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc' \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc/aarch64' \
        $(CPPFLAGS_FOR_TARGET) \
        -DUEFI
endif

ifeq ($(TARGET),uefi-riscv64)
    override CFLAGS_FOR_TARGET += \
        -fPIE \
        -fshort-wchar

    ifeq ($(CC_FOR_TARGET_IS_CLANG),yes)
        override CFLAGS_FOR_TARGET += -march=rv64imac
    else
        override CFLAGS_FOR_TARGET += -march=rv64imac_zicsr_zifencei
    endif

    override CFLAGS_FOR_TARGET += \
        -mabi=lp64 \
        -mno-relax

    override CPPFLAGS_FOR_TARGET := \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc' \
        -I'$(call SHESCAPE,$(BUILDDIR))/limine-efi/inc/riscv64' \
        $(CPPFLAGS_FOR_TARGET) \
        -DUEFI \
        -D__riscv64
endif

override LDFLAGS_FOR_TARGET += \
    -nostdlib \
    -z max-page-size=0x1000 \
    -gc-sections

ifeq ($(TARGET),bios)
    override LDFLAGS_FOR_TARGET += \
        -m elf_i386 \
        -static \
        --build-id=sha1

    ifeq ($(LD_FOR_TARGET_HAS_NO_PIE),yes)
        override LDFLAGS_FOR_TARGET += -no-pie
    endif
endif

ifeq ($(TARGET),uefi-x86-64)
    override LDFLAGS_FOR_TARGET += \
        -m elf_x86_64 \
        -static \
        -pie \
        --no-dynamic-linker \
        -z text
endif

ifeq ($(TARGET),uefi-ia32)
    override LDFLAGS_FOR_TARGET += \
        -m elf_i386 \
        -static \
        -pie \
        --no-dynamic-linker \
        -z text
endif

ifeq ($(TARGET),uefi-aarch64)
    override LDFLAGS_FOR_TARGET += \
        -m aarch64elf \
        -static \
        -pie \
        --no-dynamic-linker \
        -z text
endif

ifeq ($(TARGET),uefi-riscv64)
    override LDFLAGS_FOR_TARGET += \
        -m elf64lriscv \
        --no-relax \
        -static \
        -pie \
        --no-dynamic-linker \
        -z text
endif

override C_FILES := $(shell find . -type f -name '*.c' | LC_ALL=C sort)
ifeq ($(TARGET),bios)
    override ASMX86_FILES := $(shell find . -type f -name '*.asm_x86' | LC_ALL=C sort)
    override ASM32_FILES := $(shell find . -type f -name '*.asm_ia32' | LC_ALL=C sort)
    override ASMB_FILES := $(shell find . -type f -name '*.asm_bios_ia32' | LC_ALL=C sort)

    override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.o) $(ASM32_FILES:.asm_ia32=.o) $(ASMB_FILES:.asm_bios_ia32=.o) $(ASMX86_FILES:.asm_x86=.o))
    override OBJ_S2 := $(filter %.s2.o,$(OBJ))
endif
ifeq ($(TARGET),uefi-x86-64)
    override ASMX86_FILES := $(shell find . -type f -name '*.asm_x86' | LC_ALL=C sort)
    override ASM64_FILES := $(shell find . -type f -name '*.asm_x86_64' | LC_ALL=C sort)
    override ASM64U_FILES := $(shell find . -type f -name '*.asm_uefi_x86_64' | LC_ALL=C sort)

    override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.o) $(ASM64_FILES:.asm_x86_64=.o) $(ASM64U_FILES:.asm_uefi_x86_64=.o) $(ASMX86_FILES:.asm_x86=.o))
endif
ifeq ($(TARGET),uefi-ia32)
    override ASMX86_FILES := $(shell find . -type f -name '*.asm_x86' | LC_ALL=C sort)
    override ASM32_FILES := $(shell find . -type f -name '*.asm_ia32' | LC_ALL=C sort)
    override ASM32U_FILES := $(shell find . -type f -name '*.asm_uefi_ia32' | LC_ALL=C sort)

    override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.o) $(ASM32_FILES:.asm_ia32=.o) $(ASM32U_FILES:.asm_uefi_ia32=.o) $(ASMX86_FILES:.asm_x86=.o))
endif
ifeq ($(TARGET),uefi-aarch64)
    override ASM64_FILES := $(shell find . -type f -name '*.asm_aarch64' | LC_ALL=C sort)
    override ASM64U_FILES := $(shell find . -type f -name '*.asm_uefi_aarch64' | LC_ALL=C sort)

    override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.o) $(ASM64_FILES:.asm_aarch64=.o) $(ASM64U_FILES:.asm_uefi_aarch64=.o))
endif
ifeq ($(TARGET),uefi-riscv64)
    override ASM64_FILES := $(shell find . -type f -name '*.asm_riscv64' | LC_ALL=C sort)
    override ASM64U_FILES := $(shell find . -type f -name '*.asm_uefi_riscv64' | LC_ALL=C sort)

    override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.o) $(ASM64_FILES:.asm_riscv64=.o) $(ASM64U_FILES:.asm_uefi_riscv64=.o))
endif

override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

.PHONY: all

ifeq ($(TARGET),bios)
all: $(call MKESCAPE,$(BUILDDIR))/limine-bios.sys $(call MKESCAPE,$(BUILDDIR))/stage2.bin.gz
else ifeq ($(TARGET),uefi-x86-64)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTX64.EFI
else ifeq ($(TARGET),uefi-ia32)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTIA32.EFI
else ifeq ($(TARGET),uefi-aarch64)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTAA64.EFI
else ifeq ($(TARGET),uefi-riscv64)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTRISCV64.EFI
endif

ifeq ($(TARGET),bios)

$(call MKESCAPE,$(BUILDDIR))/stage2.bin.gz: $(call MKESCAPE,$(BUILDDIR))/stage2.bin
	gzip -n -9 < '$(call SHESCAPE,$<)' > '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/stage2.bin: $(call MKESCAPE,$(BUILDDIR))/limine-bios.sys
	dd if='$(call SHESCAPE,$<)' bs=$$(( 0x$$("$(READELF_FOR_TARGET)" -S '$(call SHESCAPE,$(BUILDDIR))/limine.elf' | $(GREP) '\.text\.stage3' | $(SED) 's/^.*] //' | $(AWK) '{print $$3}' | $(SED) 's/^0*//') - 0xf000 )) count=1 of='$(call SHESCAPE,$@)' 2>/dev/null

$(call MKESCAPE,$(BUILDDIR))/stage2.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' stage2 32 '\.text\.stage2'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/stage2.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/stage2.map.S' '$(call SHESCAPE,$(BUILDDIR))/stage2.map.d'

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nos3map.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' full 32 '\.text'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/full.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/full.map.S' '$(call SHESCAPE,$(BUILDDIR))/full.map.d'

$(call MKESCAPE,$(BUILDDIR))/limine-bios.sys: $(call MKESCAPE,$(BUILDDIR))/limine_stage2only.elf $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$(BUILDDIR))/limine.elf' '$(call SHESCAPE,$@)'
	chmod -x '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker_stage2only.ld: linker_bios.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_STAGE2ONLY linker_bios.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_stage2only.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_stage2only.elf: $(OBJ_S2)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_stage2only.ld'
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker_stage2only.ld' -o '$(call SHESCAPE,$@)' || \
		( echo "This error may mean that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

$(call MKESCAPE,$(BUILDDIR))/linker_nos2map.ld: linker_bios.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP -DLINKER_NOS2MAP linker_bios.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nos2map.ld'

$(call MKESCAPE,$(BUILDDIR))/empty:
	touch '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(OBJ)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/empty'
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nos2map.ld'
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker_nos2map.ld' -o '$(call SHESCAPE,$@)'
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s2.bin build-id.s2.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s2.o
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s3.bin build-id.s3.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s3.o
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.o' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.o' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker_nos2map.ld' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker_nomap.ld: linker_bios.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP linker_bios.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_nos3map.elf: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/stage2.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/empty'
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' -o '$(call SHESCAPE,$@)'
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s2.bin build-id.s2.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s2.o
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s3.bin build-id.s3.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s3.o
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.o' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.o' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker.ld: linker_bios.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef linker_bios.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker.ld'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/stage2.map.o $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/empty'
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker.ld'
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' -o '$(call SHESCAPE,$@)'
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s2.bin build-id.s2.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s2.o
	$(OBJCOPY_FOR_TARGET) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(OBJCOPY_FOR_TARGET) -I binary -B i386 -O elf32-i386 build-id.s3.bin build-id.s3.o && \
		$(OBJCOPY_FOR_TARGET) --add-section .note.GNU-stack='$(call SHESCAPE,$(BUILDDIR))/empty' --set-section-flags .note.GNU-stack=noload,readonly build-id.s3.o
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.o' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.o' $(LDFLAGS_FOR_TARGET) -T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' -o '$(call SHESCAPE,$@)'

endif

$(call MKESCAPE,$(BUILDDIR))/limine-efi: ../limine-efi/*
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	cp -r ../limine-efi '$(call SHESCAPE,$(BUILDDIR))/'

ifeq ($(TARGET),uefi-x86-64)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' full 64 '\.text'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/full.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/full.map.S' '$(call SHESCAPE,$(BUILDDIR))/full.map.d'

$(call MKESCAPE,$(BUILDDIR))/BOOTX64.EFI: $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'
	chmod -x '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-x86_64.S.o: limine-efi

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_x86_64.c.o: limine-efi

.PHONY: limine-efi
limine-efi: $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/limine-efi/gnuefi' \
		CC="$(CC_FOR_TARGET)" \
		CFLAGS="$(BASE_CFLAGS)" \
		CPPFLAGS='-nostdinc -isystem $(call SHESCAPE,$(SRCDIR))/../freestanding-headers' \
		ARCH=x86_64

$(call MKESCAPE,$(BUILDDIR))/linker_nomap.ld: linker_uefi_x86_64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP linker_uefi_x86_64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-x86_64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_x86_64.c.o $(OBJ)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker.ld: linker_uefi_x86_64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef linker_uefi_x86_64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker.ld'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-x86_64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_x86_64.c.o $(OBJ) $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

endif

ifeq ($(TARGET),uefi-aarch64)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' full 64 '\.text'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/full.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/full.map.S' '$(call SHESCAPE,$(BUILDDIR))/full.map.d'

$(call MKESCAPE,$(BUILDDIR))/BOOTAA64.EFI: $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'
	chmod -x '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-aarch64.S.o: limine-efi

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_aarch64.c.o: limine-efi

.PHONY: limine-efi
limine-efi: $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/limine-efi/gnuefi' \
		CC="$(CC_FOR_TARGET)" \
		CFLAGS="$(BASE_CFLAGS)" \
		CPPFLAGS='-nostdinc -isystem $(call SHESCAPE,$(SRCDIR))/../freestanding-headers' \
		ARCH=aarch64

$(call MKESCAPE,$(BUILDDIR))/linker_nomap.ld: linker_uefi_aarch64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP linker_uefi_aarch64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-aarch64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_aarch64.c.o $(OBJ)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker.ld: linker_uefi_aarch64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef linker_uefi_aarch64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker.ld'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-aarch64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_aarch64.c.o $(OBJ) $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-riscv64)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' full 64 '\.text'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/full.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/full.map.S' '$(call SHESCAPE,$(BUILDDIR))/full.map.d'

$(call MKESCAPE,$(BUILDDIR))/BOOTRISCV64.EFI: $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'
	chmod -x '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-riscv64.S.o: limine-efi

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_riscv64.c.o: limine-efi

.PHONY: limine-efi
limine-efi: $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/limine-efi/gnuefi' \
		CC="$(CC_FOR_TARGET)" \
		CFLAGS="$(BASE_CFLAGS)" \
		CPPFLAGS='-nostdinc -isystem $(call SHESCAPE,$(SRCDIR))/../freestanding-headers' \
		ARCH=riscv64

$(call MKESCAPE,$(BUILDDIR))/linker_nomap.ld: linker_uefi_riscv64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP linker_uefi_riscv64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-riscv64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_riscv64.c.o $(OBJ)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker.ld: linker_uefi_riscv64.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef linker_uefi_riscv64.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker.ld'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-riscv64.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_riscv64.c.o $(OBJ) $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-ia32)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' '$(call SHESCAPE,$<)' full 32 '\.text'
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$(BUILDDIR))/full.map.S' -o '$(call SHESCAPE,$@)'
	rm -f '$(call SHESCAPE,$(BUILDDIR))/full.map.S' '$(call SHESCAPE,$(BUILDDIR))/full.map.d'

$(call MKESCAPE,$(BUILDDIR))/BOOTIA32.EFI: $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'
	chmod -x '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-ia32.S.o: limine-efi

$(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_ia32.c.o: limine-efi

.PHONY: limine-efi
limine-efi: $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/limine-efi/gnuefi' \
		CC="$(CC_FOR_TARGET)" \
		CFLAGS="$(BASE_CFLAGS)" \
		CPPFLAGS='-nostdinc -isystem $(call SHESCAPE,$(SRCDIR))/../freestanding-headers' \
		ARCH=ia32

$(call MKESCAPE,$(BUILDDIR))/linker_nomap.ld: linker_uefi_ia32.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef -DLINKER_NOMAP linker_uefi_ia32.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-ia32.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_ia32.c.o $(OBJ)
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker_nomap.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/linker.ld: linker_uefi_ia32.ld.in
	$(MKDIR_P) '$(call SHESCAPE,$(BUILDDIR))'
	$(CC_FOR_TARGET) -x c -E -P -undef linker_uefi_ia32.ld.in -o '$(call SHESCAPE,$(BUILDDIR))/linker.ld'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/crt0-efi-ia32.S.o $(call MKESCAPE,$(BUILDDIR))/limine-efi/gnuefi/reloc_ia32.c.o $(OBJ) $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/linker.ld'
	$(LD_FOR_TARGET) \
		-T'$(call SHESCAPE,$(BUILDDIR))/linker.ld' \
		'$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$@)'

endif

-include $(HEADER_DEPS)

ifeq ($(TARGET),uefi-x86-64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-aarch64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-riscv64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-ia32)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/limine-efi
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),bios)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET),bios)
$(call MKESCAPE,$(BUILDDIR))/%.s2.o: %.s2.c
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(S2CFLAGS) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET),bios)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_ia32
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_bios_ia32
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_x86
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-x86-64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_x86_64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf64 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_uefi_x86_64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf64 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_x86
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf64 -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-aarch64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_aarch64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -x assembler-with-cpp -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_uefi_aarch64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -x assembler-with-cpp -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-riscv64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_riscv64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -x assembler-with-cpp -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_uefi_riscv64
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -x assembler-with-cpp -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET),uefi-ia32)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_ia32
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_uefi_ia32
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm_x86
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(NASM_TARGET) -f elf32 -o '$(call SHESCAPE,$@)'
endif
