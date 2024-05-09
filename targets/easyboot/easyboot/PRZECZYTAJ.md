Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) to wszechstronny *menedżer* rozruchu i kreator obrazu dysku startowego, który może
załadować różne jądra systemu operacyjnego i jądra zgodne z Multiboot2 w różnych formatach binarnych.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

UWAGA: Jeśli szukasz *loadera* startowego, który ładuje tylko jedno jądro, spójrz na młodszego brata **Easyboot**,
[Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Uznanie
-------

Ten projekt nie byłby możliwy bez wsparcia [Free Software Foundation Hungary](https://fsf.hu/nevjegy). Projekt powstał w ramach
konkursu Free Software Miękki 2023 organizowanego przez FSF.hu.

Zalety w stosunku do GRUB-a
---------------------------

- niezależny, łatwy w użyciu, wielojęzyczny instalator
- nie jest rozdęty, stanowi tylko około 0,5% wielkości GRUB-a
- łatwy w konfiguracji, ze sprawdzaniem składni
- łatwo łatać tabele ACPI za pomocą dostarczonego przez użytkownika DSDT
- pokazuje szczegóły błędu, jeśli jądro przestaje działać na wczesnym etapie
- Multiboot2: czysty 64-bitowy punkt wejścia (nie ma potrzeby osadzania tagów ani 32-bitowego kodu trampoliny w jądrze)
- Multiboot2: obsługa jąder z wyższej połowy
- Multiboot2: niezależna od oprogramowania sprzętowego, spójna mapa pamięci na wszystkich platformach
- Multiboot2: niezależny od oprogramowania sprzętowego, spójny bufor ramki na wszystkich platformach
- Multiboot2: podano również informacje EDID monitora
- Multiboot2: w razie potrzeby uruchamia jądro na wszystkich rdzeniach procesora (obsługa SMP)
- dostępna jest także wtyczka symulująca uruchamianie GRUB-a ze wszystkimi jego pułapkami i błędami.

Obsługiwane jądra: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/en/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/en/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

Obsługiwane systemy plików: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(Gdy jądro znajduje się na partycji rozruchowej, dla roota można użyć dowolnego systemu plików: ZFS, btrfs itp.)

Uzasadnienie
------------

Stworzyłem prosty w użyciu program ładujący, a użytkownicy prosili o coraz więcej funkcji. Chciałem zachować ten moduł ładujący jako
tak proste, jak to możliwe, ale FSF.hu zaoferowało wsparcie, więc zdecydowałem się je rozwidlić i dodać wszystkie żądane funkcje w
tym menedżerze rozruchu Zamiast.

Jest to również narzędzie [bez smaku](https://suckless.org), takie jak Simpleboot, nie ma żadnych zależności i jest niezwykle łatwe
w użyciu:

1. utwórz katalog i umieść w nim pliki startowe, między innymi konfigurację menu i opcjonalne wtyczki
2. wykonaj polecenie `easyboot (katalog źródłowy) (plik obrazu wyjściowego)`
3. i... to tyle... nic więcej nie zostało do zrobienia! Obraz *Just Works (TM)* spowoduje uruchomienie jądra!

Możesz zainstalować menedżera rozruchu i sprawić, że istniejące urządzenie lub obraz będzie można uruchamiać; lub możesz utworzyć
obraz startowy od nowa. Możesz uruchom ten obraz na maszynie wirtualnej lub możesz zapisać go za pomocą `dd` lub
[USBImager](https://bztsrc.gitlab.io/usbimager/) do pamięci i uruchomić to także na prawdziwej maszynie.

Prostota jest szczytem wyrafinowania!

Instalacja
----------

Wystarczy pobrać plik binarny dla swojego systemu operacyjnego. Są to przenośne pliki wykonywalne, nie wymagają instalacji i nie
potrzebują jej biblioteki współdzielone/biblioteki DLL.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

Ponadto możesz znaleźć różne rozwiązania w zakresie pakowania w katalogu [distrib](distrib) (dla Debian, Ubuntu, RaspiOS, Gentoo,
Arch).

Podczas tworzenia obrazu (w zależności od konfiguracji) możesz także potrzebować kilku wtyczek w swoim `(katalog źródłowy)`. Ty
można je znaleźć w katalogu [src/plugins](src/plugins) z rozszerzeniem `plg`. Po zainstalowaniu z pakietu będą to umieszczony w
`/usr/share/easyboot`.

Dokumentacja
------------

Szczegółową [dokumentację](docs/en) (w języku angielskim) dotyczącą korzystania z kreatora dysku startowego i uruchamiania jądra
można znaleźć w katalogu docs.

Przykładowe jądro
-----------------

Jeśli chcesz napisać jądro, które można załadować bez żadnej wtyczki, używając protokołu [uproszczonego Multiboot2](docs/en/ABI.md)
**Easyboot**, spójrz na [Przykładowe jądro Simpleboot](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example). Oba programy
ładujące używają tego samego protokołu rozruchowego, jądro współpracuje z **Easyboot** w niezmienionej postaci. Zobaczysz, że nie
jest wymagany żaden montaż ani znaczniki osadzone, w przeciwnym razie kod źródłowy jest w 99,9% taki sam jak przykład w specyfikacji
Multiboot2 (jedyna różnica polega na tym, że drukuje na konsoli szeregowej, a nie na ekranie teletype VGA, ponieważ to ten ostatni
nie istnieje w UEFI i na maszynach RaspberryPi).

Kompilacja
----------

GNU/make potrzebne do orkiestracji (choć dosłownie jest to po prostu `cc easyboot.c -o easyboot`). Łańcuch narzędzi nie ma
znaczenia, zrobi to każdy kompilator ANSI C, działa również na POSIX i WIN32 MINGW. Po prostu przejdź do katalogu [src](src) i
uruchom `make`. To wszystko. Pomimo niewielkich rozmiarów jest samowystarczalny, a kompilacja ma dokładnie zerowe zależności
bibliotek. Nie na darmo nazywa się to **Easyboot** :-)

Aby ponownie skompilować programy ładujące, będziesz potrzebować [flatassembler](https://flatassembler.net) oraz LLVM Clang i lld
(gcc i GNU ld obawiam się, że nie zadziała). Ale nie martw się, dodałem je wszystkie do `src/data.h` jako tablicę bajtów, więc nie
musisz się kompilować te, chyba że naprawdę tego chcesz (w tym celu po prostu usuń data.h przed uruchomieniem make).

Z drugiej strony, aby skompilować wtyczki, będziesz musiał mieć kompilator krzyżowy, albo LLVM CLang, albo GNU gcc (x86_64-elf-gcc,
aarch64-elf-gcc). Repo zawiera je również jako pliki binarne. Aby przeprowadzić rekompilację, po prostu usuń pliki `src/plugins/*.plg`
przed uruchomieniem make.

Licencja
--------

**Easyboot** jest oprogramowaniem bezpłatnym i otwartym, licencjonowanym na warunkach GPL w wersji 3 lub (według Ciebie) dowolnej
późniejszej wersji. Aby uzyskać szczegółowe informacje, zobacz plik [LICENSE](LICENSE).

bzt
