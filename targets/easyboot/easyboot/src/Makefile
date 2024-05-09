TARGET=easyboot
OVMF?=/usr/share/qemu/bios-TianoCoreEFI.bin
PREFIX?=usr/
INSTDIR=$(DESTDIR:/=)/$(PREFIX:/=)
ARCH=$(shell uname -m)
TMP=$(ARCH:x86_64=amd64)
TMP2=$(TMP:armv7l=armhf)
DEBARCH=$(TMP2:aarch64=arm64)
VERSION=$(shell cat easyboot.c|grep ebver|head -1|cut -d '"' -f 2)
SRCS=$(wildcard plugins/*.c)
PLGS=$(SRCS:.c=.o)
ifneq ($(wildcard /usr/bin/clang)$(wildcard /usr/local/bin/clang),)
CROSS=clang --target=ARCH-elf
else
CROSS=ARCH-elf-gcc
endif
ifneq ($(DEBUG),)
COPT=-g
LOPT=-map:output.map
else
COPT=-O3
endif

all: $(TARGET) misc/plgld $(PLGS)

# Legend:
# -ffreestanding -fno-strict-aliasing because we are running bare metal
# -fno-stack-protector -fno-stack-check because we are running bare metal (we check stack with -Wframe-larger-than)
# -fshort-wchar because UEFI uses 16-bit characters
# -fno-asynchronous-unwind-tables because we are using fucking ANSI C, not C++, throw out the unused C++ exceptions overhead
# -fno-plt -fdirect-access-external-data because plugins should use direct addresses to simplify plgld
# -Wno-long-long because UEFI uses Win32 API where long is 32-bit, so we must use non-ANSI C standard long long for 64-bit, don't complain
CFLAGS=-ffreestanding -fno-strict-aliasing -fno-stack-protector -fno-stack-check -fshort-wchar -fno-asynchronous-unwind-tables -fno-plt \
  -mpie-copy-relocations -nostdlib -Wall -Wextra -Werror -Wno-long-long

misc/plgld: misc/plgld.c
	$(CC) -ansi -Wall -Wextra -Wno-format-extra-args -Wno-format $(COPT) $< -o $@

misc/testfs: misc/testfs.c
	$(CC) -ansi -Wall -Wextra -Wno-format-extra-args -Wno-format -Wno-restrict $(COPT) $< -o $@

%.o: %.c
	@echo "plugin	$<"
	@test $*_x86_64.plg -nt $< || ($(subst ARCH,x86_64,$(CROSS)) $(CFLAGS) -mno-red-zone -mno-sse -c $< -o $@ && misc/plgld $@ $*_x86_64.plg)
	@test $*_aarch64.plg -nt $< || ($(subst ARCH,aarch64,$(CROSS)) $(CFLAGS) -c $< -o $@ && misc/plgld $@ $*_aarch64.plg)
	@rm $@ 2>/dev/null || true

boot_x86.bin: boot_x86.asm
	fasm boot_x86.asm boot_x86.bin

loader_x86.efi: loader_x86.c loader.h
	clang --target=x86_64-pc-win32-coff $(CFLAGS) -mno-red-zone -mno-sse -Wframe-larger-than=512 -c loader_x86.c -o loader_x86.o
	lld -flavor link $(LOPT) -subsystem:efi_application -Brepro -nodefaultlib -dll -dynamicbase -base:0x7000 -entry:_start loader_x86.o -out:loader_x86.efi
ifneq ($(DEBUG),)
	@misc/map2sym.sh
endif
	@rm loader_x86.o loader_x86.lib 2>/dev/null

loader_rpi.bin: loader_rpi.c loader.h
	clang --target=aarch64-elf $(CFLAGS) -Wl,-Ttext=0x9000 -Wl,--omagic loader_rpi.c -o loader_rpi.o

data.h: boot_x86.bin loader_x86.efi loader_rpi.bin
	@$(CC) misc/bin2h.c -o bin2h
	@./bin2h boot_x86.bin loader_x86.efi loader_rpi.o
	@rm bin2h
	@touch data.h

$(TARGET): loader.h easyboot.c
	@test -f data.h || make --no-print-directory data.h
	$(CC) -ansi -Wall -Wextra $(COPT) easyboot.c -o $(TARGET)
	@strip $(TARGET) 2>/dev/null || true
	@strip $(TARGET).exe 2>/dev/null || true

install: $(TARGET)
ifneq ("$(INSTDIR)","")
	@strip $(TARGET)
	install -m 755 -g bin $(TARGET) $(INSTDIR)/bin
	@mkdir -p $(INSTDIR)/share/man/man1 $(INSTDIR)/share/easyboot 2>/dev/null || true
	cp misc/easyboot.1.gz $(INSTDIR)/share/man/man1
	cp plugins/*.plg icons/*.tga misc/font.sfn $(INSTDIR)/share/easyboot
else
	@echo "INSTDIR variable not set, not installing."
	@false
endif

package: $(TARGET)
ifneq ("$(wildcard /bin/*.exe)","")
	@strip $(TARGET).exe
	@rm ../distrib/easyboot-i686-win.zip 2>/dev/null || true
	zip -r ../distrib/easyboot-i686-win.zip $(TARGET).exe plugins/*.plg icons/*.tga misc/font.sfn
else
	@mkdir -p bin share/man/man1 share/easyboot share/easyboot/plugins share/easyboot/icons share/easyboot/misc
	@cp $(TARGET) bin
	@strip bin/$(TARGET)
	@cp misc/easyboot.1.gz share/man/man1
	@cp plugins/*.plg share/easyboot/plugins
	@cp icons/*.tga share/easyboot/icons
	@cp misc/font.sfn share/easyboot/misc
	tar -czvf ../distrib/easyboot-x86_64-linux.tgz bin share
	@rm -rf bin share
endif

deb: $(TARGET)
	@rm -rf DEBIAN usr 2>/dev/null || true
	@mkdir -p DEBIAN usr/bin usr/share/man/man1 usr/share/easyboot usr/share/easyboot/plugins usr/share/easyboot/icons usr/share/easyboot/misc
	@cp $(TARGET) usr/bin
	@strip usr/bin/$(TARGET)
	@cp misc/easyboot.1.gz usr/share/man/man1
	@cp plugins/*.plg usr/share/easyboot/plugins
	@cp icons/*.tga usr/share/easyboot/icons
	@cp misc/font.sfn usr/share/easyboot/misc
	@cat misc/deb_control | sed s/ARCH/$(DEBARCH)/g | sed s/VERSION/$(VERSION)/g | sed s/SIZE/`du -s usr|cut -f 1`/g >DEBIAN/control
	@md5sum `find usr -type f` >DEBIAN/md5sums
	@cp ../LICENSE DEBIAN/copyright
	@echo "2.0" >debian-binary
	@tar -czvf data.tar.gz usr
	@tar -C DEBIAN -czvf control.tar.gz control copyright md5sums
	ar r ../distrib/easyboot_$(VERSION)-$(DEBARCH).deb debian-binary control.tar.gz data.tar.gz
	@rm -rf debian-binary control.tar.gz data.tar.gz DEBIAN usr

# generate a bootable disk image from a test "boot" directory
disk.img: boot/easyboot/menu.cfg $(TARGET) misc/plgld $(PLGS)
ifneq ("$(wildcard boot/easyboot/menu.cfg)","")
	cp plugins/*.plg boot/easyboot
endif
	./$(TARGET) -s 64 -vvc -u 3A9CEB3C-A4DE-0ED9-BF8D8F1DEEA76350 boot disk.img

# mount the generated disk (requires root priviledge)
mnt: disk.img
	@mkdir -p mnt || true
	sudo mount -o loop,offset=1048576,user,umask=000 disk.img mnt

umnt:
	sudo umount mnt
	@rmdir mnt

# test in a UEFI machine
efi: disk.img
	qemu-system-x86_64 -bios $(OVMF) -m 256 -drive file=disk.img,format=raw -serial stdio
	@printf '\033[0m'

# test in a BIOS machine
qemu: disk.img
	qemu-system-x86_64 -m 256 -drive file=disk.img,format=raw -serial stdio -no-reboot -no-shutdown

# test with another, non-qemu VM too
bochs: disk.img
	bochs -f misc/bochs.rc -q

# test in a Raspberry Pi 3B
rpi: disk.img
	@mkdir mnt && sudo mount -o loop,offset=1048576,user,umask=000 disk.img mnt
	qemu-system-aarch64 -M raspi3b -kernel mnt/KERNEL8.IMG -drive file=disk.img,if=sd,format=raw -serial stdio || true
	@sudo umount mnt && rmdir mnt

# debug in a UEFI machine (stop before executing)
efidbg: disk.img
	qemu-system-x86_64 -s -S -d int -bios $(OVMF) -m 256 -drive file=disk.img,format=raw -serial stdio || true
	@printf '\033[0m'

# debug in a BIOS machine (stop before executing)
qemudbg: disk.img
	qemu-system-x86_64 -s -S -d int -m 256 -drive file=disk.img,format=raw -serial stdio

# start the debugger and attach it to the stopped VM
gdb:
	gdb -w -x misc/gdb.rc || true
	pkill qemu

clean:
	rm -rf boot_*.bin loader_*.bin loader_*.o loader_*.lib loader_*.efi easyboot misc/plgld plugins/*.o mnt 2>/dev/null || true

distclean: clean
	rm disk.img data.h output.map output.sym plugi