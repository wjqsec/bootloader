Booten von Kerneln mit Easyboot
===============================

[Easyboot](https://gitlab.com/bztsrc/easyboot) ist ein All-in-One Boot Manager und bootfähiger Disk-Image-Ersteller, der geladen
werden kann verschiedene Betriebssystemkerne und Multiboot2-kompatible Kernel in verschiedenen Binärformaten.

[[_TOC_]]

Installation
------------

```
 easyboot [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e] [-c] <indir> <outfile>

  -v, -vv         Erhöhen Sie die Ausführlichkeit/Validierung
  -s <mb>         Legen Sie die Disk-Image-Größe in Megabyte fest (Standard ist 35 MB)
  -b <mb>         Legen Sie die Größe der Boot-Partition in Megabyte fest (Standard ist 33 MB)
  -u <guid>       Legen Sie die eindeutige Kennung der Boot-Partition fest (Standard ist zufällig)
  -p <t> <u> <i>  Fügen Sie eine zusätzliche Partition hinzu (Typ-GUID, eindeutige GUID, Image-Datei)
  -e              El Torito Boot Catalog hinzufügen (EFI CDROM Boot-Unterstützung)
  -c              Erstellen Sie immer eine neue Abbilddatei, auch wenn diese vorhanden ist
  <indir>         Verwenden Sie den Inhalt dieses Verzeichnisses für die Boot-Partition
  <outfile>       Ausgabebild oder Gerätedateiname
```

Das **Easyboot**-Tool erstellt mithilfe der GUID-Partitionierungstabelle ein bootfähiges Festplatten-Image mit dem Namen `(outfile)`
mit einer einzelnen Partition, die als FAT32 formatiert ist und als „EFI-Systempartition“ (kurz ESP) bezeichnet wird. Der Inhalt
dieser Partition wird aus dem von Ihnen bereitgestellten `(indir)` übernommen. Sie müssen eine einfache Nur-Text-Konfigurationsdatei
mit dem Namen `easyboot/menu.cfg` in diesem Verzeichnis ablegen. Mit NL- oder CRLF-Zeilenenden können Sie diese problemlos mit jedem
Texteditor bearbeiten. Abhängig von Ihrer Konfiguration benötigen Sie möglicherweise auch einige [Plugins](plugins.md) in diesem
Verzeichnis mit dem Namen `easyboot/*.plg`.

Das Image kann auch auf dem Raspberry Pi gebootet werden und funktioniert in qemu; aber um auf einer echten Maschine zu booten,
benötigen Sie weitere Firmware-Dateien `bootcode.bin`, `fixup.dat`, `start.elf` und eine .dtb-Datei im Verzeichnis `(indir)`, diese
können heruntergeladen werden aus dem [offiziellen Repository](https://github.com/raspberrypi/firmware/tree/master/boot) des
Raspberry Pi.

Das Tool verfügt außerdem über einige optionale Befehlszeilen-Flags: `-s (mb)` legt die Gesamtgröße des generierten Disk-Images in
Megabyte fest, während `-b (mb)` die Größe der Boot-Partition in Megabyte festlegt. Offensichtlich muss ersteres größer sein als
letzteres. Wenn nicht angegeben, wird die Partitionsgröße aus der Größe des angegebenen Verzeichnisses berechnet (mindestens 33 MB,
das kleinste FAT32, das es geben kann) und die Festplattengröße ist standardmäßig 2 MB größer (aufgrund der Ausrichtung und des für
die Partitionierung benötigten Platzes Tisch). Wenn zwischen diesen beiden Größenwerten eine Lücke von mehr als 2 MB besteht, können
Sie Tools von Drittanbietern wie `fdisk` verwenden, um dem Image nach Ihren Wünschen weitere Partitionen hinzuzufügen (oder siehe
`-p` unten). Wenn Sie ein vorhersehbares Layout wünschen, können Sie auch die eindeutige Kennung der Startpartition
(UniquePartitionGUID) mit dem Flag `-u <guid>` angeben.

Optional können Sie mit dem Flag „-p“ auch zusätzliche Partition(en) hinzufügen. Dies erfordert 3 Argumente: (PartitionTypeGUID),
(UniquePartitionGUID) und den Namen der Bilddatei, die den Inhalt der Partition enthält.

Das Flag `-e` fügt El Torito Boot Catalog zum generierten Image hinzu, sodass es nicht nur als USB-Stick, sondern auch als
EFI-CDROM gebootet werden kann.

Wenn `(outfile)` eine Gerätedatei ist (z. B. `/dev/sda` unter Linux, `/dev/disk0` unter BSDs und `\\.\PhysicalDrive0` unter Windows),
wird weder GPT noch erstellt ESP, stattdessen lokalisiert es die bereits auf dem Gerät vorhandenen. Es kopiert weiterhin alle
Dateien in `(indir)` auf die Boot-Partition und installiert die Loader. Dies funktioniert auch, wenn `(outfile)` eine bereits
vorhandene Bilddatei ist (verwenden Sie in diesem Fall das Flag `-c` um immer zuerst eine neue, leere Bilddatei zu erstellen).

Konfiguration
-------------

Die Datei `easyboot/menu.cfg` kann die folgenden Zeilen enthalten (sehr ähnlich der Syntax von grub.cfg, eine
Beispielkonfigurationsdatei finden Sie [hier](menu.cfg)):

### Kommentare

Alle Zeilen, die mit einem `#` beginnen, gelten als Kommentare und werden bis zum Ende der Zeile übersprungen.

### Ausführlichkeitsgrad

Sie können den Ausführlichkeitsgrad mithilfe einer Zeile festlegen, die mit `verbose` beginnt.

```
verbose (0-3)
```

Dadurch wird dem Loader mitgeteilt, wie viele Informationen auf der Boot-Konsole gedruckt werden sollen. Verbose 0 bedeutet völlig
leise (Standard) und Verbose 3 gibt die geladenen Kernelsegmente und den Maschinencode am Einstiegspunkt aus.

### Bildschirm

Mit der Zeile, die mit `framebuffer` beginnt, können Sie eine bestimmte Bildschirmauflösung anfordern. Das Format ist wie folgt:

```
framebuffer (Breite) (Höhe) (Bits pro Pixel) [#(Vordergrundfarbe)] [#(Hintergrundfarbe)] [#(Fortschrittsbalkensfarbe)]
```

**Easyboot** richtet einen Framebuffer für Sie ein, auch wenn diese Zeile nicht vorhanden ist (standardmäßig 800 x 600 x 32 bpp).
Wenn diese Zeile jedoch vorhanden ist, wird versucht, die angegebene Auflösung festzulegen. Palettenmodi werden nicht unterstützt,
daher müssen die Bits pro Pixel mindestens 15 betragen.

Wenn der vierte optionale Farbparameter angegeben wird, muss er in HTML-Notation vorliegen, beginnend mit einem Hashmark, gefolgt
von 6 hexadezimalen Ziffern, RRGGBB. Beispielsweise ist `#ff0000` ein sattes helles Rot und `#007f7f` ein dunkleres Cyan. Es legt
den Vordergrund oder mit anderen Worten die Farbe der Schriftart fest. Wenn der fünfte optionale Farbparameter angegeben wird, muss
er ebenfalls in HTML-Notation vorliegen und legt die Hintergrundfarbe fest. Der letzte optionale Farbparameter gilt ebenfalls und
legt die Hintergrundfarbe des Fortschrittsbalkens fest.

Wenn keine Farben angegeben sind, werden standardmäßig weiße Buchstaben auf schwarzem Hintergrund verwendet und der Hintergrund des
Fortschrittsbalkens ist dunkelgrau.

### Standard-Boot-Option

Die Zeile, die mit `default` beginnt, legt fest, welcher Menüeintrag nach dem angegebenen Timeout ohne Benutzerinteraktion gestartet
werden soll.

```
default (Menüeintragsindex) (Timeout msec)
```

Der Menüeintragsindex ist eine Zahl zwischen 1 und 9. Timeout wird in Millisekunden (ein Tausendstel einer Sekunde) angegeben, 1000
ergibt also eine Sekunde.

### Menüausrichtung

Zeilen, die mit `menualign` beginnen, ändern die Art und Weise, wie die Startoptionen angezeigt werden.

```
menualign ("vertical"|"horizontal")
```

Standardmäßig ist das Menü horizontal, Sie können es aber auch in vertikal ändern.

### Boot-Optionen

Sie können bis zu 9 Menüeinträge angeben, wobei die Zeilen mit `menuentry` beginnen. Das Format ist wie folgt:

```
menuentry (Symbol) [Etikett]
```

Für jedes Symbol muss eine Datei `easyboot/(Symbol).tga` auf der Boot-Partition vorhanden sein. Das Bild muss in einem
lauflängencodierten, farblich zugeordneten [Targa-Format](../en/TGA.txt) vorliegen, da dies die am stärksten komprimierte Variante
ist (die ersten drei Bytes der Datei müssen `0`, `1` und `9` sein in dieser Reihenfolge, siehe Datentyp 9 in der Spezifikation).
Seine Abmessungen müssen genau 64 Pixel hoch und 64 Pixel breit sein.

Um in diesem Format aus GIMP zu speichern, wählen Sie zunächst „Bild > Modus > Indiziert ...“, stellen Sie im Popup-Fenster
„Maximale Anzahl an Farben“ auf 256 ein. Wählen Sie dann „Datei > Exportieren als ...“, geben Sie ein Geben Sie einen Dateinamen
ein, der auf `.tga` endet, und aktivieren Sie im Popup-Fenster „RLE-Komprimierung“. Als Befehlszeilen-Konvertierungstool können Sie
ImageMagick verwenden: `convert (jede Bilddatei) -colors 256 -compress RLE Symbol.tga`.

Der optionale Etikett-Parameter dient der Anzeige von ASCII-Versions- oder Release-Informationen im Menü und nicht für beliebige
Zeichenfolgen. Aus Platzgründen wird UTF-8 daher nicht unterstützt, es sei denn, Sie geben auch `easyboot/font.sfn` an. (Eine
UNICODE-Schriftart benötigt viel Speicherplatz, obwohl die [Scalable Screen Font](https://gitlab.com/bztsrc/scalable-font2) sehr
effizient ist, sind es immer noch etwa 820 KB. Im Gegensatz dazu ist unicode.pf2 von GRUB viel größer, etwa 2392 KB, obwohl beide
Schriftarten ca. 55600 Glyphen in 8x16- und 16x16-Bitmaps enthalten. Die eingebettete ASCII-Schriftart hat nur 8x16-Bitmaps und 96
Glyphen.)

Alle Zeilen, die nach einer `menuentry`-Zeile kommen, gehören zu diesem Menüeintrag, es sei denn, diese Zeile ist eine andere
Menüeintragszeile. Der Einfachheit halber können Sie Blöcke wie in GRUB verwenden, diese sind jedoch nur syntaktischer Zucker.
Geschweifte Klammern werden als Leerzeichen behandelt. Sie können sie weglassen und stattdessen Tabulatoren verwenden, genau wie
in einem Python-Skript oder Makefile, wenn Sie das bevorzugen.

Zum Beispiel
```
menuentry FreeBSD backup
{
    kernel bsd.old/boot
}
```

### Partition auswählen

Die Zeile, die mit `partition` beginnt, wählt eine GPT-Partition aus. Muss eine `menuentry`-Zeile voranstellen.

```
partition (Partition eindeutige UUID)
```

Diese Partition wird als Root-Dateisystem für die Startoption verwendet. Von dieser Partition werden der Kernel sowie die Module
geladen und je nach Boot-Protokoll auch an den Kernel übergeben. Die angegebene Partition befindet sich möglicherweise auf einer
anderen Festplatte als der Startfestplatte. **Easyboot** durchläuft während des Startvorgangs eine Iteration auf allen
GPT-partitionierten Festplatten, um sie zu finden.

Der Einfachheit halber wird die Partition auch in der Zeile `kernel` gesucht (siehe unten). Wenn die angegebene Boot-Befehlszeile
eine Zeichenfolge `root=(UUID)` oder `root=*=(UUID)` enthält, ist keine separate Zeile `partition` erforderlich.

Wenn die Partition nicht explizit angegeben wird, werden der Kernel und die Module von der Boot-Partition geladen.

### Gibt einen Kernel an

Die mit `kernel` beginnende Zeile gibt an, welche Datei mit welchen Parametern gebootet werden soll. Muss eine `menuentry`-Zeile
voranstellen.

```
kernel (Pfad zu Ihrer Kerneldatei) (optionale Boot-Befehlszeilenargumente)
```

Der Pfad muss auf eine vorhandene Datei verweisen, eine ausführbare Kernel-Binärdatei, und es muss ein absoluter UTF-8-Pfad auf der
Root-Partition (oder Boot-Partition) sein. Wenn sich der Kernel nicht im Stammverzeichnis der Partition befindet, ist das
Verzeichnistrennzeichen immer `/`, auch auf UEFI-Systemen. Wenn der Name ein Leerzeichen enthält, muss dieses mit `\` maskiert
werden. Dem Pfad können Befehlszeilenargumente folgen, getrennt durch ein Leerzeichen. Bei Multiboot2-kompatiblen Kerneln werden
diese Befehlszeilenargumente im Tag *Boot-Befehlszeile* (Typ 1) übergeben. Sie werden von **Easyboot** nicht verändert und auch
nicht analysiert, außer nach dem Partitionsspezifizierer gesucht.

Standardmäßig kann **Easyboot** Multiboot2-kompatible Kernel in den Formaten ELF64 und PE32+/COFF booten (und auf UEFI-Systemen auch
UEFI-Anwendungen). Normalerweise erlaubt dieses Protokoll keine Kernel der höheren Hälfte, aber **Easyboot** verstößt ein wenig
gegen das Protokoll, so dass normale Kernel, die nicht der höheren Hälfte angehören, nicht beschädigt werden. Wenn Sie einen anderen
Kernel booten möchten, benötigen Sie einen Kernel-Loader [Plugin](plugins.md).

HINWEIS: Im Gegensatz zu GRUB, wo Sie spezielle Befehle wie „linux“ oder „multiboot“ verwenden müssen, um das Boot-Protokoll
auszuwählen, gibt es hier nur einen Befehl und das Protokoll wird zur Laufzeit automatisch von Ihrem Kernel erkannt.

### Weitere Module laden

Sie können beliebige Dateien (ursprüngliche Ramdisks, Kernel-Treiber usw.) zusammen mit dem Kernel laden, indem Sie Zeilen
verwenden, die mit `module` beginnen. Muss eine `menuentry`-Zeile voranstellen. Beachten Sie, dass diese Zeile innerhalb jedes
Menüeintrags mehrmals wiederholt werden kann. Wenn das Boot-Protokoll eine Initrd unterstützt, wird die allererste `module`-Zeile
als Initrd betrachtet.

```
module (Pfad zu Datei) (optionale Modulbefehlszeilenargumente)
```

Der Pfad muss auf eine vorhandene Datei verweisen und es muss ein absoluter UTF-8-Pfad auf der Root-Partition (oder Boot-Partition)
sein. Es können Befehlszeilenargumente folgen, getrennt durch ein Leerzeichen. Wenn die Datei komprimiert ist und es ein
Dekomprimierungs-[Plugin](plugins.md) dafür gibt, wird das Modul transparent dekomprimiert. Informationen zu diesen geladenen
(und unkomprimierten) Modulen werden in den Tags *Moduls* (Typ 3) an einen Multiboot2-kompatiblen Kernel übergeben.

Der Sonderfall ist, wenn das Modul mit den Bytes `DSDT`, `GUDT` oder `0xD00DFEED` beginnt. In diesen Fällen wird die Datei nicht
hinzugefügt *Modules*-Tags, vielmehr wird die ACPI-Tabelle so gepatcht, dass ihre DSDT-Zeiger auf den Inhalt dieser Datei verweisen.
Mit diesem Sie können die ACPI-Tabelle eines fehlerhaften BIOS problemlos durch eine vom Benutzer bereitgestellte ersetzen.

### Boot-Splash-Logo

Sie können auch ein Logo in der Mitte des Bildschirms anzeigen, wenn die Boot-Option ausgewählt ist, wenn Sie eine Zeile platzieren,
die mit `bootsplash` beginnt. Muss eine `menuentry`-Zeile voranstellen.

```
bootsplash [#(Hintergrundfarbe)] (Pfad zur TGA-Datei)
```

Die Hintergrundfarbe ist optional und muss in HTML-Notation erfolgen, beginnend mit einem Hashmark, gefolgt von 6 Hexadezimalziffern,
RRGGBB. Wenn das erste Argument nicht mit `#` beginnt, wird ein Pfadargument angenommen.

Der Pfad muss auf eine vorhandene Datei verweisen und es muss ein absoluter UTF-8-Pfad auf der Boot-Partition (NICHT Root-Partition)
sein. Das Bild muss in einem lauflängencodierten, farblich zugeordneten Targa-Format vorliegen, genau wie die Menüsymbole.
Abmessungen können alles sein, was auf den Bildschirm passt.

### Multicore-Unterstützung

Um den Kernel auf allen Prozessorkernen gleichzeitig zu starten, geben Sie die Direktive `multicore` an. Muss eine `menuentry`-Zeile
voranstellen und erfordert außerdem die Installation des Plugins [smp](../../src/plugins/smp.c).

```
multicore
```

Fehlerbehebung
--------------

Wenn Sie auf Probleme stoßen, führen Sie es einfach mit der Flagge `easyboot -vv` aus. Dadurch wird eine Validierung durchgeführt
und die Ergebnisse zum Zeitpunkt der Bilderstellung ausführlich ausgegeben. Andernfalls fügen Sie `verbose 3` zu `easyboot/menu.cfg`
hinzu, um detaillierte Startzeitmeldungen zu erhalten.

Wenn Sie in der oberen linken Ecke die Zeichenfolge `PMBR-ERR` mit rotem Hintergrund sehen, bedeutet das, dass Ihre CPU sehr alt ist
und den 64-Bit-Long-Modus nicht unterstützt oder der Bootsektor den Loader nicht booten konnte. Tritt möglicherweise nur auf
BIOS-Maschinen auf, bei UEFI oder auf RaspberryPi kann dies niemals passieren.

| Nachricht                           | Beschreibung                                                                      |
|-------------------------------------|-----------------------------------------------------------------------------------|
| `Booting [X]...`                    | zeigt an, dass die Startoption (index des menuentry) X ausgewählt wurde           |
| `Loading 'X' (Y bytes)...`          | Datei X mit Y-Länge wird geladen                                                  |
| `Parsing kernel...`                 | Der Kernel wurde gefunden und erkennt nun sein Format                             |
| `Starting X boot...`                | zeigt an, dass der Bootloader des X-Systems erkannt wurde                         |
| `Starting X kernel...`              | zeigt as, dass der Kernel des X-Systems erkannt wurde                             |
| `Transfering X control to Y`        | zeigt an, dass der X-Modus-Einstiegspunkt an der Y-Adresse aufgerufen werden soll |

Wenn Sie nach der letzten Meldung auf Probleme stoßen, bedeutet dies, dass das Problem beim Bootvorgang des Betriebssystems und
nicht beim **Easyboot**-Loader aufgetreten ist. Sie müssen daher die Dokumentation des jeweiligen Betriebssystems konsultieren.
Andernfalls können Sie gerne ein [Issue](https://gitlab.com/bztsrc/easyboot/-/issues) auf gitlab eröffnen.

### Multiboot1

Erforderliche Plugins: [grubmb1](../../src/plugins/grubmb1.c)

### Multiboot2

Dies ist die flexibelste Variante, da mehrere Varianten über Plugins unterstützt werden:

- ELF64 oder PE32+ mit vereinfachtem Multiboot2: keine Plugins erforderlich
- ELF32 mit vereinfachtem Multiboot2 und 32-Bit-Einstiegspunkt: [elf32](../../src/plugins/elf32.c)
- a.out (struct exec) mit vereinfachtem Multiboot2 und 32-Bit-Einstiegspunkt: [aout](../../src/plugins/aout.c)
- GRUB-kompatibles Multiboot2 mit 32-Bit-Einstiegspunkt: [grubmb2](../../src/plugins/grubmb2.c)

Beachten Sie den Unterschied: Das [vereinfachte Multiboot2](ABI.md) erfordert keine eingebetteten Tags, unterstützt Kernel der
höheren Hälfte, einen sauberen 64-Bit-Einstiegspunkt mit Parametern gemäß den Multiboot2-, SysV- und Fastcall-ABIs ebenfalls
übergeben werden.

Andererseits benötigt [GRUB-kompatibles Multiboot2](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
eingebetteten Tags, unterstützen weder höherwertige noch 64-Bit-Kernel, der Einstiegspunkt ist immer 32-Bit und Parameter nur in
`eax`, `ebx` übergeben werden.

### Windows

Es sind keine Plugins erforderlich, aber Sie müssen [SecureBoot](secureboot.md) einrichten.

```
menuentry win {
  kernel EFI/Microsoft/BOOT/BOOTMGRW.EFI
}
```

### Linux

Erforderliche Plugins: [linux](../../src/plugins/linux.c), [ext234](../../src/plugins/ext234.c)

Wenn der Kernel nicht auf der Boot-Partition platziert ist, können Sie die Root-Partition über die Befehlszeile angeben.

```
menuentry linux {
  kernel vmlinuz-linux root=PARTUUID=01020304-0506-0708-0a0b0c0d0e0f1011
}
```

### OpenBSD

Erforderliche Plugins: [obsdboot](../../src/plugins/obsdboot.c), [ufs44](../../src/plugins/ufs44.c)

```
menuentry openbsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot
}
```

WARNUNG! Verwenden Sie nicht das Plugin [elf32](../../src/plugins/elf32.c), wenn Sie OpenBSD booten! Sein `boot` behauptet
fälschlicherweise, es zu ein ELF mit 32-Bit-SysV-ABI-Einstiegspunkt sein, aber tatsächlich verfügt es über einen
16-Bit-Real-Mode-Eintrag.

### FreeBSD

Erforderliche Plugins: [fbsdboot](../../src/plugins/fbsdboot.c), [ufs2](../../src/plugins/ufs2.c)

Geben Sie auf älteren BIOS-Systemen den Loader `boot` an.

```
menuentry freebsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot/boot
}
```

Verwenden Sie auf UEFI-Maschinen den Loader `loader.efi` auf der Boot-Partition (keine Plugins erforderlich).

```
menuentry freebsd {
  kernel boot/loader.efi
}
```

Wenn Ihr Root-Dateisystem ZFS ist, kopieren Sie diese eine Datei (`boot` im BIOS, `loader.efi` im UEFI) nach `(indir)` und geben
Sie NICHT an eine Root-Partition.

### FreeDOS

Erforderliche Plugins: [fdos](../../src/plugins/fdos.c)

Verschieben Sie die FreeDOS-Dateien nach `(indir)` (FreeDOS verwendet die Boot-Partition als Root-Partition).

```
menuentry freedos {
  kernel KERNEL.SYS
}
```

Wenn der Startvorgang nach dem Drucken von Copyright und `- InitDisk` stoppt, wurde der FreeDOS-Kernel ohne FAT32-Unterstützung
kompiliert. Laden Sie einen anderen Kernel herunter, dessen Name `f32` enthält.

### ReactOS

Erforderliche Plugins: [reactos](../../src/plugins/reactos.c)

```
menuentry reactos {
  kernel FREELDR.SYS
}
```

### MenuetOS

Erforderliche Plugins: [menuet](../../src/plugins/menuet.c)

```
menuentry menuetos {
  kernel KERNEL.MNT
  module CONFIG.MNT
  module RAMDISK.MNT
}
```

Um die automatische Konfiguration zu deaktivieren, fügen Sie `noauto` zur Befehlszeile hinzu.

### KolibriOS

Erforderliche Plugins: [kolibri](../../src/plugins/kolibri.c)

```
menuentry kolibrios {
  kernel KERNEL.MNT syspath=/rd/1/ launcher_start=1
  module KOLIBRI.IMG
  module DEVICES.DAT
}
```

Das Plugin funktioniert auch auf UEFI-Maschinen, aber ist es möglich zu `uefi4kos.efi` auf der Boot-Partition verwenden (und kein
Plugin erforderlich).

### SerenityOS

Erforderliche Plugins: [grubmb1](../../src/plugins/grubmb1.c)

```
menuentry serenityos {
  kernel boot/Prekernel
  module boot/Kernel
}
```

### Haiku

Erforderliche Plugins: [grubmb1](../../src/plugins/grubmb1.c), [befs](../../src/plugins/befs.c)

```
menuentry haiku {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel system/packages/haiku_loader-r1~beta4_hrev56578_59-1-x86_64.hpkg
}
```

Verwenden Sie auf UEFI-Maschinen den Loader `haiku_loader.efi` auf der Boot-Partition (keine Plugins erforderlich).
