Schreiben von Easyboot-kompatiblen Kerneln
==========================================

[Easyboot](https://gitlab.com/bztsrc/easyboot) unterstützt verschiedene Kernel mit [Plugins](plugins.md). Wenn jedoch kein passendes
Plugin gefunden wird, greift es auf ELF64- oder PE32+-Binärdateien mit einer vereinfachten Variante des Multiboot2-Protokolls zurück
(keine Notwendigkeit, etwas einzubetten).

Dies ist genau das gleiche Protokoll, das [Simpleboot](https://gitlab.com/bztsrc/simpleboot) verwendet. Alle Beispielkernel in
diesem Repo müssen auch mit **Easyboot** funktionieren.

Sie können den ursprünglichen multiboot2.h-Header im GRUB-Repo oder die C/C++-Header-Datei [simpleboot.h](https://gitlab.com/bztsrc/simpleboot/blob/main/simpleboot.h)
verwenden, um einfacher darauf zuzugreifen Verwenden Sie Typedefs. Das Low-Level-Binärformat ist dasselbe. Sie können auch alle
vorhandenen Multiboot2-Bibliotheken verwenden, auch mit Nicht-C-Sprachen, wie [Rust](https://github.com/rust-osdev/multiboot2/tree/main/multiboot2/src)
-Bibliothek zum Beispiel (Hinweis: Ich bin in keiner Weise mit diesen Entwicklern verbunden, ich habe nur nach „Rust Multiboot2“ gesucht
und das war die erste Ergebnis).

[[_TOC_]]

Startvorgang
------------

### Bootstrapping des Loaders

Auf *BIOS*-Maschinen wird der allererste Sektor der Festplatte von der Firmware auf 0:0x7C00 geladen und ihm wird die Kontrolle
übergeben. In diesem Sektor verfügt **Easyboot** über [boot_x86.asm](../../src/boot_x86.asm), das intelligent genug ist, um den 2.
Stage-Loader zu finden und zu laden und auch den Langmodus dafür einzurichten.

Auf *UEFI*-Maschinen wird dieselbe Datei der zweiten Stufe mit dem Namen `EFI/BOOT/BOOTX64.EFI` direkt von der Firmware geladen. Die
Quelle für diesen Loader finden Sie in [loader_x86.c](../../src/loader_x86.c). Das ist es, **Easyboot** ist weder GRUB noch Syslinux,
die beide Dutzende und Aberdutzende Systemdateien auf der Festplatte erfordern. Hier werden keine weiteren Dateien benötigt, nur
diese (Plugins sind optional, keine erforderlich, um Multiboot2-Kompatibilität bereitzustellen).

Auf *Raspberry Pi* heißt der Loader `KERNEL8.IMG`, kompiliert aus [loader_rpi.c](../../src/loader_rpi.c).

### Der Lader

Dieser Loader wurde sehr sorgfältig geschrieben, um auf mehreren Konfigurationen zu funktionieren. Es lädt die
GUID-Partitionierungstabelle von der Festplatte und sucht nach einer „EFI-Systempartition“. Wenn es gefunden wird, sucht es auf
dieser Boot-Partition nach der Konfigurationsdatei `easyboot/menu.cfg`. Nachdem die Boot-Option ausgewählt und der Dateiname des
Kernels bekannt ist, findet und lädt der Loader ihn.

Es erkennt dann automatisch das Format des Kernels und ist intelligent genug, um die Abschnitts- und Segmentinformationen darüber
zu interpretieren, wo was geladen werden soll (es führt bei Bedarf eine Speicherzuordnung bei Bedarf durch). Anschließend richtet
es je nach erkanntem Boot-Protokoll (Multiboot2 / Linux / usw. geschützter oder langer Modus, ABI-Argumente usw.) eine geeignete
Umgebung ein. Nachdem der Maschinenzustand stabil und klar definiert ist, springt der Loader als allerletzten Schritt zum
Einstiegspunkt Ihres Kernels.

Maschinenzustand
----------------

Es gilt alles, was in der [Multiboot2-Spezifikation](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) (auf
Englisch) über den Maschinenstatus geschrieben steht, mit Ausnahme der Allzweckregister. **Easyboot** übergibt gemäß der SysV-ABI
und der Microsoft-Fastcall-ABI zwei Argumente an den Einstiegspunkt Ihres Kernels. Der erste Parameter ist die Magie, der zweite
ist a physische Speicheradresse, die auf eine Multiboot-Informations-Tagliste verweist (im Folgenden als MBI abgekürzt, siehe unten).

Wir verletzen auch ein wenig das Multiboot2-Protokoll, um Kernel der höheren Hälfte zu verarbeiten. Multiboot2 schreibt vor, dass
dem Speicher eine Identitätszuordnung zugewiesen werden muss. Nun, unter **Easyboot** trifft dies nur teilweise zu: Wir garantieren
nur, dass der gesamte physische RAM sicher wie erwartet identitätszugeordnet ist; Einige darüber liegende Regionen (abhängig von
den Programm-Headern des Kernels) sind jedoch möglicherweise noch verfügbar. Dies beeinträchtigt nicht normale Multiboot2-kompatible
Kernel, die nicht auf Speicher außerhalb des verfügbaren physischen RAM zugreifen sollen.

Ihr Kernel wird sowohl auf BIOS- und UEFI-Systemen als auch auf RPi genau gleich geladen, Firmware-Unterschiede sind nur „Jemand
Else's Problem“. Das Einzige, was Ihr Kernel sehen wird, ist, ob der MBI das EFI-Systemtabellen-Tag enthält oder nicht. Um Ihnen das
Leben zu vereinfachen, generiert **Easyboot** auch kein EFI-Speicherzuordnungs-Tag (Typ 17), sondern stellt nur bereit das Tag
[Speicherzuordnungs](#speicherzuordnungs) (Typ 6) wahllos auf allen Plattformen (auch auf UEFI-Systemen, dort wird die
Speicherzuordnung einfach für Sie konvertiert, Ihr Kernel muss sich also nur mit einer Art von Speicherlisten-Tags befassen). Auch
alte, veraltete Tags werden von diesem Bootmanager weggelassen und nie generiert.

Der Kernel läuft auf Supervisor-Ebene (Ring 0 auf x86, EL1 auf ARM).

GDT nicht spezifiziert, aber gültig. Der Stapel wird in den ersten 640k eingerichtet und wächst nach unten (Sie sollten dies jedoch
so schnell wie möglich auf den Stapel ändern, den Sie für würdig halten).

Sie sollten IDT als nicht spezifiziert betrachten; IRQs, NMI und Software-Interrupts deaktiviert. Dummy-Ausnahmehandler werden so
eingerichtet, dass sie einen minimalen Dump anzeigen und die Maschine anhalten. Sie sollten sich nur darauf verlassen, dass sie
melden, wenn Ihr Kernel kaputt geht, bevor Sie Ihren eigenen IDT und Handler einrichten konnten, vorzugsweise so schnell wie möglich.
Auf ARM ist vbar_el1 so eingerichtet, dass es dieselben Dummy-Ausnahmehandler aufruft (obwohl sie natürlich unterschiedliche
Register ausgeben).

Framebuffer ist ebenfalls standardmäßig festgelegt. Sie können die Auflösung in der Konfiguration ändern, aber wenn sie nicht
angegeben wird, ist der Framebuffer weiterhin konfiguriert.

Es ist wichtig, niemals von Ihrem Kernel zurückzukehren. Es steht Ihnen frei, jeden Teil des Loaders im Speicher zu überschreiben
(sobald Sie mit den MBI-Tags fertig sind), sodass Sie einfach nirgendwohin zurückkehren können.
„Der Mohr hat seine Schuldigkeit getan, der Mohr kann gehen.“

An Ihren Kernel übergebene Boot-Informationen (MBI)
---------------------------------------------------

Es ist zunächst nicht offensichtlich, aber die Multiboot2-Spezifikation definiert tatsächlich zwei völlig unabhängige Sätze von Tags:

- Der erste Satz soll in einen Multiboot2-kompatiblen Kernel eingebunden sein und wird als Multiboot2-Header des Betriebssystem-Images
  bezeichnet (Abschnitt 3.1.2) und wird daher *vom Kernel bereitgestellt*. **Easyboot** kümmert sich nicht um diese Tags und
  analysiert Ihren Kernel auch nicht danach. Sie benötigen einfach keine speziellen magischen Daten, die in Ihre Kerneldatei
  eingebettet sind, da **Easyboot**, ELF- und PE-Header ausreichen.

- Der zweite Satz wird beim Booten dynamisch *an den Kernel übergeben*, **Easyboot** verwendet nur diese Tags. Es generiert jedoch
  nicht alles, was Multiboot2 angibt (es lässt einfach die alten, veralteten oder Legacy-Angaben weg). Diese Tags werden MBI-Tags
  genannt, siehe [Boot-Informationen](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information-format)
  (Abschnitt 3.6).

HINWEIS: Die Multiboot2-Spezifikation für MBI-Tags ist verdammt fehlerhaft. Unten finden Sie eine korrigierte Version, die mit der
Header-Datei multiboot2.h übereinstimmt, die Sie im Quell-Repository von GRUB finden.

Der erste Parameter Ihres Kernels ist der magische 0x36d76289 (in `rax`, `rcx` und `rdi`). Sie können die MBI-Tags mithilfe des
zweiten Parameters finden (in `rbx`, `rdx` und `rsi`). Auf der ARM-Plattform befindet sich Magic in `x0` und die Adresse in `x1`.
Auf RISC-V und MIPS werden jeweils `a0` und `a1` verwendet. Wenn dieser Loader auf eine andere Architektur portiert wird, müssen
immer die durch SysV ABI für Funktionsargumente angegebenen Register verwendet werden. Wenn es andere gemeinsame ABIs auf der
Plattform gibt, die die SysV-ABI nicht beeinträchtigen, sollten die Werte auch in den Registern dieser ABIs (oder oben im Stapel)
dupliziert werden.

### Überschriften

Die übergebene Adresse ist immer 8 Bytes lang ausgerichtet und beginnt mit einem MBI-Header:

```
        +-------------------+
u32     | total_size        |
u32     | reserved          |
        +-------------------+
```

Darauf folgt eine Reihe von ebenfalls 8 Byte großen ausgerichteten Tags. Jedes Tag beginnt mit den folgenden Tag-Header-Feldern:

```
        +-------------------+
u32     | type              |
u32     | size              |
        +-------------------+
```

`type` enthält eine Kennung des Inhalts des restlichen Tags. `size` enthält die Größe des Tags einschließlich der Header-Felder,
jedoch ohne Auffüllung. Tags folgen aufeinander und werden bei Bedarf aufgefüllt, damit jedes Tag an einer 8-Byte-ausgerichteten
Adresse beginnt.

### Abschlusszeichen

```
        +-------------------+
u32     | type = 0          |
u32     | size = 8          |
        +-------------------+
```

Tags werden durch ein Tag vom Typ `0` und der Größe `8` abgeschlossen.

### Boot-Befehlszeile

```
        +-------------------+
u32     | type = 1          |
u32     | size              |
u8[n]   | string            |
        +-------------------+
```

`string` enthält die in der „kernel“-Zeile von *menuentry* angegebene Befehlszeile (ohne Pfad und Dateinamen des Kernels). Die
Befehlszeile ist eine normale, nullterminierte UTF-8-Zeichenfolge im C-Stil.

### Bootloader-Name

```
        +----------------------+
u32     | type = 2             |
u32     | size = 17            |
u8[n]   | string "Easyboot"    |
        +----------------------+
```

`string` enthält den Namen eines Bootloaders, der den Kernel bootet. Der Name ist eine normale nullterminierte UTF-8-Zeichenfolge
im C-Stil.

### Moduls

```
        +-------------------+
u32     | type = 3          |
u32     | size              |
u32     | mod_start         |
u32     | mod_end           |
u8[n]   | string            |
        +-------------------+
```

Dieses Tag zeigt dem Kernel an, welches Bootmodul zusammen mit dem Kernel-Image geladen wurde und wo es zu finden ist. `mod_start`
und `mod_end` enthalten die physischen Start- und Endadressen des Bootmoduls selbst. Sie erhalten nie einen mit gzip komprimierten
Puffer, da **Easyboot** diese transparent für Sie dekomprimiert (und wenn Sie ein Plugin bereitstellen, funktioniert es auch mit
anderen als mit gzip komprimierten Daten). Das Feld `string` stellt eine beliebige Zeichenfolge bereit, die diesem bestimmten
Bootmodul zugeordnet werden soll; Es handelt sich um eine normale nullterminierte UTF-8-Zeichenfolge im C-Stil. Wird in der
„Module“-Zeile von *menuentry* angegeben und seine genaue Verwendung ist betriebssystemspezifisch. Im Gegensatz zum
Boot-Befehlszeilen-Tag enthalten die Modul-Tags *auch* den Pfad und den Dateinamen des Moduls.

Pro Modul erscheint ein Tag. Dieser Tag-Typ kann mehrmals vorkommen. Wenn eine erste Ramdisk zusammen mit Ihrem Kernel geladen
wurde, wird diese als erstes Modul angezeigt.

Es gibt einen Sonderfall: Wenn es sich bei der Datei um eine DSDT-ACPI-Tabelle, einen FDT-Blob (dtb) oder GUDT-Blob handelt, wird
sie nicht als Modul angezeigt, sondern ACPI altes RSDP (Typ 14) oder ACPI neues RSDP (Typ 15) wird gepatcht und ihre DSDT durch
den Inhalt dieser Datei ersetzt.

### Speicherzuordnungs

Dieses Tag stellt eine Speicherzuordnung bereit.

```
        +-------------------+
u32     | type = 6          |
u32     | size              |
u32     | entry_size = 24   |
u32     | entry_version = 0 |
varies  | entries           |
        +-------------------+
```

`size` enthält die Größe aller Einträge einschließlich dieses Feldes selbst. `entry_size` ist immer 24. `entry_version` ist auf `0`
gesetzt. Jeder Eintrag hat folgenden Aufbau:

```
        +-------------------+
u64     | base_addr         |
u64     | length            |
u32     | type              |
u32     | reserved          |
        +-------------------+
```

`base_addr` ist die physische Startadresse. `length` ist die Größe des Speicherbereichs in Bytes. `type` ist die Art des
dargestellten Adressbereichs, wobei der Wert „1“ den verfügbaren RAM angibt, der Wert „3“ den nutzbaren Speicher mit
ACPI-Informationen angibt und der Wert „4“ den reservierten Speicher angibt, der im Ruhezustand erhalten bleiben muss. Der Wert
„5“ zeigt einen Speicher an, der durch defekte RAM-Module belegt ist, alle anderen Werte weisen derzeit auf einen reservierten
Bereich hin. `reserved` wird beim BIOS-Booten auf „0“ gesetzt.

Wenn der MBI auf einem UEFI-Computer generiert wird, werden verschiedene EFI-Speicherzuordnungseinträge als Typ „1“ (verfügbarer RAM)
oder „2“ (reservierter RAM) gespeichert, und falls Sie ihn benötigen, wird der ursprüngliche EFI-Speichertyp darin abgelegt Feld
`reserved`.

Die bereitgestellte Karte listet garantiert den gesamten Standard-RAM auf, der für die normale Verwendung verfügbar sein sollte,
und sie ist immer nach aufsteigender „base_addr“ sortiert. Dieser verfügbare RAM-Typ umfasst jedoch die von Kernel, MBI, Segmenten
und Modulen belegten Bereiche. Der Kernel muss darauf achten, diese Regionen nicht zu überschreiben (**Easyboot** könnte diese
Regionen leicht ausschließen, aber das würde die Multiboot2-Kompatibilität beeinträchtigen).

### Bildschirminformationen

```
        +----------------------------------+
u32     | type = 8                         |
u32     | size = 38                        |
u64     | framebuffer_addr                 |
u32     | framebuffer_pitch                |
u32     | framebuffer_width                |
u32     | framebuffer_height               |
u8      | framebuffer_bpp                  |
u8      | framebuffer_type = 1             |
u16     | reserved                         |
u8      | framebuffer_red_field_position   |
u8      | framebuffer_red_mask_size        |
u8      | framebuffer_green_field_position |
u8      | framebuffer_green_mask_size      |
u8      | framebuffer_blue_field_position  |
u8      | framebuffer_blue_mask_size       |
        +----------------------------------+
```

Das Feld `framebuffer_addr` enthält die physische Adresse des Framebuffers. Das Feld `framebuffer_pitch` enthält die Länge einer
Zeile in Bytes. Die Felder `framebuffer_width` und `framebuffer_height` enthalten Framebuffer-Abmessungen in Pixel. Das Feld
`framebuffer_bpp` enthält die Anzahl der Bits pro Pixel. `framebuffer_type` ist immer auf 1 gesetzt und `reserved` enthält in der
aktuellen Version der Spezifikation immer 0 und muss vom Betriebssystem-Image ignoriert werden. Die übrigen Felder beschreiben das
gepackte Pixelformat, die Position und Größe der Kanäle in Bits. Sie können den Ausdruck `((~(0xffffffff << size)) << position) & 0xffffffff`
verwenden, um eine UEFI-GOP-ähnliche Kanalmaske zu erhalten.

### EFI 64-Bit-Systemtabellenzeiger

Dieses Tag existiert nur, wenn **Easyboot** auf einem UEFI-Computer ausgeführt wird. Auf einem BIOS-Computer wurde dieses Tag nie
generiert.

```
        +-------------------+
u32     | type = 12         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Dieses Tag enthält einen Zeiger auf die EFI-Systemtabelle.

### EFI 64-Bit-Bild-Handle-Zeiger

Dieses Tag existiert nur, wenn **Easyboot** auf einem UEFI-Computer ausgeführt wird. Auf einem BIOS-Computer wurde dieses Tag nie
generiert.

```
        +-------------------+
u32     | type = 20         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Dieses Tag enthält einen Zeiger auf das EFI-Bildhandle. Normalerweise ist es das Bootloader-Image-Handle.

### SMBIOS-Tabellen

```
        +-------------------+
u32     | type = 13         |
u32     | size              |
u8      | major             |
u8      | minor             |
u8[6]   | reserved          |
        | smbios tables     |
        +-------------------+
```

Dieses Tag enthält eine Kopie der SMBIOS-Tabellen sowie deren Version.

### Alte ACPI-RSDP

```
        +-------------------+
u32     | type = 14         |
u32     | size              |
        | copy of RSDPv1    |
        +-------------------+
```

Dieses Tag enthält eine Kopie von RSDP gemäß der Definition in der ACPI 1.0-Spezifikation. (Mit einer 32-Bit-Adresse.)

### Neue ACPI-RSDP

```
        +-------------------+
u32     | type = 15         |
u32     | size              |
        | copy of RSDPv2    |
        +-------------------+
```

Dieses Tag enthält eine Kopie von RSDP, wie in der Spezifikation ACPI 2.0 oder höher definiert. (Wahrscheinlich mit einer
64-Bit-Adresse.)

Diese (Typ 14 und 15) verweisen auf eine `RSDT`- oder `XSDT`-Tabelle mit einem Zeiger auf eine `FACP`-Tabelle, die wiederum zwei
Zeiger auf eine `DSDT`-Tabelle enthält, die die Maschine beschreibt. **Easyboot** fälscht diese Tabellen auf Maschinen, die ACPI
ansonsten nicht unterstützen. Auch wenn Sie eine DSDT-Tabelle, einen FDT-Blob (dtb) oder GUDT-Blob als Modul bereitstellen,
patcht **Easyboot** die Zeiger so, dass sie auf diese vom Benutzer bereitgestellte Tabelle verweisen. Um diese Tabellen zu
analysieren, können Sie meine abhängigkeitsfreie Einzelheader-Bibliothek [hwdet](https://gitlab.com/bztsrc/hwdet) (oder die
aufgeblähte Bibliothek [apcica](https://github.com/acpica/) und [libfdt](https://github.com/dgibson/dtc)) verwenden.

Kernelspezifische Tags
----------------------

Tags mit `type` größer oder gleich 256 sind nicht Teil der Multiboot2-Spezifikation. Diese können durch optionale
[Plugins](plugins.md) zur Liste hinzugefügt werden, wenn ein Kernel sie benötigt.

### EDID

```
        +-------------------+
u32     | type = 256        |
u32     | size              |
        | copy of EDID      |
        +-------------------+
```

Dieses Tag enthält eine Kopie der Liste der unterstützten Monitorauflösungen gemäß der EDID-Spezifikation.

### SMP

```
        +-------------------+
u32     | type = 257        |
u32     | size              |
u32     | numcores          |
u32     | running           |
u32     | bspid             |
        +-------------------+
```

Dieses Tag existiert, wenn die Anweisung `multicore` angegeben wurde. `numcores` enthält die Anzahl der CPU-Kerne im System,
`running` ist die Anzahl der Kerne, die erfolgreich initialisiert wurden und denselben Kernel parallel ausführen. Das `bspid`
enthält die Kennung des BSP-Kerns (lAPIC-ID auf x86), sodass Kernel APs unterscheiden und auf diesen einen anderen Code
ausführen können. Alle APs haben ihren eigenen Stapel und oben auf dem Stapel befindet sich die ID des aktuellen Kerns.

Speicherlayout
--------------

### BIOS-Maschinen

| Start    | Ende    | Beschreibung                                                                      |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x400 | Interrupt Vector Table (verwendbar, real-Modus IDT)                               |
|    0x400 |   0x4FF | BIOS Data Area (verwendbar)                                                       |
|    0x4FF |   0x500 | BIOS-Startlaufwerkscode (höchstwahrscheinlich 0x80, verwendbar)                   |
|    0x500 |  0x1000 | Ausnahmehandler-Stack (verwendbar, nachdem Sie Ihr IDT eingerichtet haben)        |
|   0x1000 |  0x8000 | Paging-Tabellen (verwendbar, nachdem Sie Ihre Paging-Tabellen eingerichtet haben) |
|   0x8000 | 0x20000 | Loader-Code und Daten (verwendbar, nachdem Sie Ihr IDT eingerichtet haben)        |
|  0x20000 | 0x40000 | Konfiguration + Tags (verwendbar nach der MBI-Analyse)                            |
|  0x40000 | 0x90000 | plugin ids; Von oben nach unten: Kernel-Stack                                     |
|  0x90000 | 0x9A000 | Nur Linux-Kernel: zero page + cmdline                                             |
|  0x9A000 | 0xA0000 | Extended BIOS Data Area (besser nicht anfassen)                                   |
|  0xA0000 | 0xFFFFF | VRAM und BIOS ROM (nicht verwendbar)                                              |
| 0x100000 |       x | Kernel-Segmente, gefolgt von den Modulen, jede Seite ausgerichtet                 |

### UEFI-Maschinen

Niemand weiß. UEFI weist den Speicher nach Belieben zu. Erwarten Sie alles und jedes, aber höchstwahrscheinlich unter 64 Mio.
platziert. Alle Bereiche werden in der Speicherzuordnung sicherlich als Typ = 1 (`MULTIBOOT_MEMORY_AVAILABLE`) und reserved = 2
(`EfiLoaderData`) aufgeführt, dies ist jedoch nicht exklusiv, auch andere Arten von Speicher könnten so aufgeführt werden (BSS des
Boot-Managers zum Beispiel).

### Raspberry Pi

| Start    | Ende    | Beschreibung                                                                      |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x100 | Von der Firmware reserviert (besser nicht anfassen)                               |
|    0x100 |  0x1000 | Ausnahmehandler-Stack (verwendbar, nachdem Sie Ihr VBAR eingerichtet haben)       |
|   0x1000 |  0x9000 | Paging-Tabellen (verwendbar, nachdem Sie Ihre Paging-Tabellen eingerichtet haben) |
|   0x9000 | 0x20000 | Loader-Code und Daten (verwendbar, nachdem Sie Ihr IDT eingerichtet haben)        |
|  0x20000 | 0x40000 | Konfiguration + Tags (verwendbar nach der MBI-Analyse)                            |
|  0x40000 | 0x80000 | Firmware bereitgestellt FDT (dtb); Von oben nach unten: Kernel-Stack              |
| 0x100000 |       x | Kernel-Segmente, gefolgt von den Modulen, jede Seite ausgerichtet                 |

Die ersten paar Bytes sind für [armstub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S) reserviert. Nur der
Kern 0 wurde gestartet. Um Anwendungsprozessoren zu starten, schreiben Sie die Adresse einer Funktion auf 0xE0 (Kern 1), 0xE8
(Kern 2), 0xF0 (Kern 3). Diese Adressen befinden sich in diesem Bereich.

Obwohl das RPi von Haus aus nicht unterstützt wird, erhalten Sie dennoch ein altes ACPI-RSDP-Tag (Typ 14) mit gefälschten Tabellen.
Die `APIC`-Tabelle wird verwendet, um dem Kernel die Anzahl der verfügbaren CPU-Kerne mitzuteilen. Die Adresse der Startfunktion
wird im Feld RSD PTR -> RSDT -> APIC -> cpu\[x].apic_id gespeichert (und die Kern-ID in cpu\[x].acpi_id, wobei BSP immer
cpu\[0].acpi_id ist = 0 und cpu\[0].apic_id = 0xD8. Achtung, `acpi` und `apic` sehen ziemlich ähnlich aus.

Wenn von der Firmware ein gültiger FDT-Blob übergeben wird oder eines der Module eine .dtb-, .gud- oder .aml-Datei ist, wird auch
eine FADT-Tabelle (mit magischem `FACP`) hinzugefügt. In dieser Tabelle zeigt der DSDT-Zeiger (32 Bit, bei Offset 40) auf den
bereitgestellten abgeflachten Gerätebaum-Blob.

Obwohl die Firmware keine Speicherzuordnungsfunktion bereitstellt, erhalten Sie dennoch auch ein Speicherzuordnungs-Tag (Typ 6), das
den erkannten RAM und die MMIO-Region auflistet. Damit können Sie die Basisadresse des MMIO erkennen, die bei RPi3 und RPi4
unterschiedlich ist.
