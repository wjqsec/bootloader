Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) ist ein All-in-One Boot *Manager* und bootfähiger Disk-Image-Ersteller, der geladen
werden kann verschiedene Betriebssystemkerne und Multiboot2-kompatible Kernel in verschiedenen Binärformaten.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

HINWEIS: Wenn Sie nach einem Boot-*Loader* suchen, der nur einen einzelnen Kernel lädt, dann werfen Sie einen Blick auf den kleinen
Bruder von **Easyboot**, der [Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Anerkennung
-----------

Dieses Projekt wäre ohne die Unterstützung der [Free Software Foundation Hungary](https://fsf.hu/nevjegy) nicht möglich gewesen.
Das Projekt wurde in Anwendung der FSF.hu-Ausschreibung für Die Freie Software 2023 erstellt.

Vorteile gegenüber GRUB
-----------------------

- Abhängigkeitsfreies, benutzerfreundliches, mehrsprachiges Installationsprogramm
- Nicht aufgebläht, es macht nur etwa 0,5 % der Größe von GRUB aus
- Einfach zu konfigurieren, mit Syntax-Checker
- Einfaches Patchen von ACPI-Tabellen mit vom Benutzer bereitgestelltem DSDT
- Zeigt Fehlerdetails an, wenn der Kernel in einem frühen Stadium ausfällt
- Multiboot2: sauberer 64-Bit-Einstiegspunkt (kein Einbetten von Tags oder 32-Bit-Trampolincode im Kernel erforderlich)
- Multiboot2: Unterstützung für höherwertige Kernel
- Multiboot2: Firmware-unabhängiger, konsistenter Speicherzuordnung auf allen Plattformen
- Multiboot2: Firmware-unabhängiger, konsistenter Framebuffer auf allen Plattformen
- Multiboot2: Die EDID-Informationen des Monitors werden ebenfalls bereitgestellt
- Multiboot2: startet bei Bedarf den Kernel auf allen Prozessorkernen (SMP-Unterstützung)
- Es gibt auch ein Plugin zur Simulation des GRUB-Bootens mit all seinen Fallstricken und Fehlern.

Unterstützte Kernel: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/de/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/de/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

Unterstützte Dateisysteme: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(Wenn sich der Kernel auf der Boot-Partition befindet, kann jedes Dateisystem als Root verwendet werden: ZFS, btrfs usw.)

Begründung
----------

Ich habe einen einfach zu verwendenden Bootloader erstellt und die Benutzer fragten nach immer mehr Funktionen. Ich wollte diesen
Boot Loader behalten So einfach wie möglich, aber FSF.hu bot Unterstützung an, also habe ich beschlossen, es zu forken und alle
gewünschten Funktionen in diesem Boot Manager hinzuzufügen stattdessen.

Dies ist ebenfalls ein [sauglos](https://suckless.org) Tool wie Simpleboot, hat keine Abhängigkeiten und ist äußerst einfach zu
verwenden:

1. Erstellen Sie ein Verzeichnis und legen Sie darin Ihre Bootdateien ab, unter anderem Ihre Menükonfiguration und die optionalen Plugins
2. Führen Sie den Befehl `easyboot (Quellverzeichnis) (Ausgabebilddatei)` aus
3. Und... nichts, das war's... nichts anderes bleibt zu tun! Das Image *Just Works (TM)* wird Ihre Kernel booten!

Sie können den Boot Manager installieren und ein vorhandenes Gerät oder Image bootfähig machen; oder Sie können ein bootfähiges
Image neu erstellen. Sie können Booten dieses Image in einer VM, oder Sie können es mit `dd` oder
[USBImager](https://bztsrc.gitlab.io/usbimager/) auf einen Speicher schreiben und booten das auch auf einer echten Maschine.

Einfachheit ist die entscheidende Kultiviertheit!

Installation
------------

Laden Sie einfach die Binärdatei für Ihr Betriebssystem herunter. Dabei handelt es sich um portable ausführbare Dateien, die keine
Installation erfordern und auch nicht benötigt werden gemeinsam genutzte Bibliotheken/DLLs.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

Darüber hinaus finden Sie im Verzeichnis [distrib](distrib) verschiedene Verpackungslösungen (für Debian, Ubuntu, RaspiOS, Gentoo,
Arch).

Wenn Sie dann ein Image erstellen, benötigen Sie (abhängig von Ihrer Konfiguration) möglicherweise auch einige Plugins in Ihrem
`(Quellverzeichnis)`. Du Sie finden diese im Verzeichnis [src/plugins](src/plugins) mit der Erweiterung `plg`. Bei der Installation
aus einem Paket sind dies der Fall unter `/usr/share/easyboot` abgelegt.

Dokumentation
-------------

Die ausführliche [Dokumentation](docs/de) zur Verwendung des Disk-Image-Ersteller und zum Booten eines Kernels finden Sie in den
docs Verzeichnis.

Beispielkernel
--------------

Wenn Sie einen Kernel schreiben möchten, der ohne Plugin geladen werden kann, indem Sie das [vereinfachte Multiboot2](docs/de/ABI.md)
-Protokoll von **Easyboot** verwenden, dann werfen Sie einen Blick auf
[Simpleboot-Beispielkernel](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example) Verzeichnis. Beide Loader verwenden das
gleiche Boot-Protokoll, sodass der Kernel unverändert mit **Easyboot** funktioniert. Sie werden sehen, dass weder Assembly noch
eingebettete Tags erforderlich sind, ansonsten ist der Quellcode zu 99,9 % mit dem Beispiel in der Multiboot2-Spezifikation
identisch (der einzige Unterschied besteht darin, dass er auf der seriellen Konsole und nicht auf dem VGA-Teletype-Bildschirm
gedruckt wird, als Letzteres existiert nicht auf UEFI und auf RaspberryPi-Maschinen).

Zusammenstellung
----------------

GNU/make wird für die Orchestrierung benötigt (obwohl es buchstäblich nur `cc easyboot.c -o easyboot` ist). Die Toolchain spielt
keine Rolle, Jeder ANSI-C-Compiler reicht aus und funktioniert auch unter POSIX und WIN32 MINGW. Gehen Sie einfach in das
Verzeichnis [src](src) und führen Sie `make` aus. Das ist alles. Trotz seiner geringen Größe ist es eigenständig und bei der
Kompilierung gibt es genau keine Bibliotheksabhängigkeiten. Es heißt nicht **Easyboot** für nichts :-)

Um die Loader neu zu kompilieren, benötigen Sie den [flatassembler](https://flatassembler.net) sowie LLVM Clang und lld (gcc und
GNU ld wird leider nicht funktionieren). Aber keine Sorge, ich habe sie alle als Byte-Array zu `src/data.h` hinzugefügt, sodass Sie
nicht kompilieren müssen diese, es sei denn, Sie möchten es wirklich (löschen Sie dazu einfach data.h, bevor Sie make ausführen).

Andererseits benötigen Sie zum Kompilieren der Plugins einen Cross-Compiler, entweder LLVM CLang oder GNU gcc (x86_64-elf-gcc,
aarch64-elf-gcc). Das Repo enthält diese auch als Binärdateien. Um die Neukompilierung durchzuführen, löschen Sie einfach die
Dateien `src/plugins/*.plg` bevor Sie make ausführen.

Lizenz
------

**Easyboot** ist kostenlose Open-Source-Software, lizenziert unter den Bedingungen der GPL Version 3 oder (Ihrer Meinung nach)
einer späteren Version. Einzelheiten finden Sie in der Datei [LICENSE](LICENSE).

bzt
