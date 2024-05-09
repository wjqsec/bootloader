Easyboot Plugins
================

Standardmäßig startet [Easyboot](https://gitlab.com/bztsrc/easyboot) Multiboot2-kompatible Kernel in den Formaten ELF und PE von der
Boot-Partition. Wenn Ihr Kernel ein anderes Dateiformat oder ein anderes Boot-Protokoll verwendet oder sich nicht auf der
Boot-Partition befindet, benötigen Sie Plugins auf der Boot-Partition. Sie finden diese im Verzeichnis [src/plugins](../../src/plugins).

[[_TOC_]]

Installation
------------

Um Plugins zu installieren, kopieren Sie sie einfach in das im Parameter `(indir)` angegebene Verzeichnis im Unterverzeichnis
`easyboot` neben der Datei menu.cfg.

​Zum Beispiel:
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
$ easyboot bootpart diskette.img
```

Kompilation
-----------

*Es war von Anfang an klar, dass ELF für diese Aufgabe nicht geeignet ist. Es ist zu aufgebläht und zu komplex. Ursprünglich wollte
ich also struct exec (das klassische UNIX-a.out-Format) verwenden, aber moderne Toolchains können das leider nicht mehr erstellen.
Deshalb habe ich beschlossen, mein eigenes Format und einen eigenen Linker für die Plugins zu erstellen.*

Sie können die Quelle des Plugins mit jedem Standard-ANSI-C-Cross-Compiler in eine ELF-Objektdatei kompilieren, müssen dann aber zum
Erstellen den Linker [plgld](../../src/misc/plgld.c) verwenden die letzte Binärdatei. Dies ist ein architekturunabhängiger
Cross-Linker, der unabhängig davon funktioniert, für welchen Maschinencode das Plugin kompiliert wurde. Die endgültige .plg-Datei
hat nur einen Bruchteil der Größe der .o ELF-Datei, aus der sie generiert wurde.

### Plugin API

Die C-Quelle eines Plugins muss die Header-Datei `src/loader.h` und eine Zeile `EASY_PLUGIN` enthalten. Dies hat einen Parameter,
den Typ des Plugins, gefolgt von der Spezifikation für die Identifizierungsübereinstimmung. Letzteres wird vom Loader verwendet, um
zu bestimmen, wann dieses bestimmte Plugin verwendet werden soll.

Zum Beispiel:

```c
#include "../loader.h"

/* Magische Bytes, die einen Linux-Kernel identifizieren */
EASY_PLUGIN(PLG_T_KERNEL) {
   /* Versetzt Größe Match-Typ     magische Bytes */
    { 0x1fe,      2, PLG_M_CONST, { 0xAA, 0x55, 0, 0 } },
    { 0x202,      4, PLG_M_CONST, { 'H', 'd', 'r', 'S' } }
};

/* Einstiegspunkt, Prototyp wird durch den Typ des Plugins definiert */
PLG_API void _start(uint8_t *buf, uint64_t size);
{
    /* Bereiten Sie die Umgebung für einen Linux-Kernel vor */
}
```

Die Plugins können mehrere Variablen und Funktionen verwenden, diese werden alle in der Header-Datei definiert und zur Laufzeit
verknüpft.

```c
uint32_t verbose;
```
Ausführlichkeitsgrad. Ein Plugin darf nur dann eine Ausgabe drucken, wenn diese ungleich Null ist, mit Ausnahme von Fehlermeldungen.
Je größer der Wert ist, desto mehr Details sollten gedruckt werden.

```c
uint64_t file_size;
```
Die Gesamtgröße der geöffneten Datei (siehe `open` und `loadfile` unten).

```c
uint8_t *root_buf;
```
Wenn ein Dateisystem-Plugin initialisiert wird, enthält dieses die ersten 128 KB der Partition (hoffentlich einschließlich des
Superblocks). Später kann ein Dateisystem-Plugin diesen 128-KB-Puffer für beliebige Zwecke (FAT-Cache, Inode-Cache usw.)
wiederverwenden.

```c
uint8_t *tags_buf;
```
Enthält die Multiboot2-Tags. Ein Kernel-Plugin kann dies analysieren, um die vom Boot-Manager bereitgestellten Daten in das vom
Kernel erwartete Format zu konvertieren. Dieser Zeiger zeigt auf den Anfang des Puffers.

```c
uint8_t *tags_ptr;
```
Dieser Zeiger zeigt auf das Ende des Multiboot2-Tags-Puffers. Tag-Plugins fügen hier möglicherweise neue Tags hinzu und passen
diesen Zeiger an.

```c
uint8_t *rsdp_ptr;
```
Zeigt auf den RSDP-ACPI-Zeiger.

```c
uint8_t *dsdt_ptr;
```
Verweist auf den DSDT- (oder GUDT-, FDT-)Hardwarebeschreibungsblob.

```c
efi_system_table_t *ST;
```
Zeigt auf UEFI-Maschinen auf die EFI-Systemtabelle, andernfalls `NULL`.

```c
void memset(void *dst, uint8_t c, uint32_t n);
void memcpy(void *dst, const void *src, uint32_t n);
int  memcmp(const void *s1, const void *s2, uint32_t n);
```
Obligatorische Speicherfunktionen (der C-Compiler generiert möglicherweise Aufrufe für diese, auch wenn kein direkter Aufruf
erfolgt).

```c
void *alloc(uint32_t num);
```
Weist `num` Seiten (4 KB) Speicher zu. Plugins dürfen nicht viel zuweisen, sie müssen auf einen minimalen Speicherbedarf abzielen.


```c
void free(void *buf, uint32_t num);
```
Geben Sie zuvor zugewiesenen Speicher für `num` Seiten frei.

```c
void printf(char *fmt, ...);
```
Formatierte Zeichenfolge an die Boot-Konsole ausgeben.

```c
uint64_t pb_init(uint64_t size);
```
Startet den Fortschrittsbalken. `size` ist die Gesamtgröße, die er darstellt. Gibt zurück, wie viele Bytes ein Pixel wert ist.

```c
void pb_draw(uint64_t curr);
```
Zeichnet den Fortschrittsbalken für den aktuellen Wert. `curr` muss zwischen 0 und der Gesamtgröße liegen.

```c
void pb_fini(void);
```
Schließt die Fortschrittsleiste und räumt ihren Platz auf dem Bildschirm frei.

```c
void loadsec(uint64_t sec, void *dst);
```
Wird von den Dateisystem-Plugins verwendet und lädt einen Sektor von der Festplatte in den Speicher. `sec` ist die Nummer des
Sektors, relativ zur Root-Partition.

```c
void sethooks(void *o, void *r, void *c);
```
Wird von den Dateisystem-Plugins verwendet und setzt die Hooks der open / read / close für das Dateisystem der Root-Partition.

```c
int open(char *fn);
```
Öffnen Sie eine Datei auf der Root-Partition (oder Boot-Partition) zum Lesen. Bei Erfolg wird 1 zurückgegeben. Es kann jeweils nur
eine Datei geöffnet werden. Wenn vorher kein `sethooks`-Aufruf erfolgt ist, wird es auf der Boot-Partition ausgeführt.

```c
uint64_t read(uint64_t offs, uint64_t size, void *buf);
```
Liest Daten aus der geöffneten Datei an der Suchposition `offs` in den Speicher und gibt die Anzahl der tatsächlich gelesenen Bytes
zurück.

```c
void close(void);
```
Schließt die geöffnete Datei.

```c
uint8_t *loadfile(char *path);
```
Laden Sie eine Datei vollständig von der Root-Partition (oder Boot-Partition) in einen neu zugewiesenen Speicherpuffer und
dekomprimieren Sie sie transparent, wenn das Plugin gefunden wird. In `file_size` zurückgegebene Größe.

```c
int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
```
Laden Sie ein Segment aus dem Kernel-Puffer. Dies prüft, ob der Speicher `vaddr` verfügbar ist, und ordnet das Segment zu, wenn es
sich in der höheren Hälfte befindet. Die `offs` sind der Datei-Offset, also relativ zum Kernel-Puffer. Wenn `memsz` größer als
`filesz` ist, wird die Differenz mit Nullen aufgefüllt.

```c
void _start(void);
```
Einstiegspunkt für Dateisystem-Plugins (`PLG_T_FS`). Es sollte den Superblock in `root_buf` analysieren und `sethooks` aufrufen.
Bei einem Fehler sollte es einfach zurückkehren, ohne seine Hooks zu setzen.

```c
void _start(uint8_t *buf, uint64_t size);
```
Einstiegspunkt für Kernel-Plugins (`PLG_T_KERNEL`). Erhält das Kernel-Image im Speicher, sollte es seine Segmente verschieben, die
richtige Umgebung einrichten und die Kontrolle darüber übertragen. Wenn es keinen Fehler gibt, wird es nie zurückgegeben.

```c
uint8_t *_start(uint8_t *buf);
```
Einstiegspunkt für Dekomprimierungs-Plugins (`PLG_T_DECOMP`). Empfängt den komprimierten Puffer (und seine Größe in `file_size`)
und sollte einen zugewiesenen neuen Puffer mit den unkomprimierten Daten zurückgeben (und die Größe des neuen Puffers ebenfalls in
`file_size` festlegen). Es muss den alten Puffer freigeben (Achtung, `file_size` wird in Bytes angegeben, free() erwartet jedoch
die Größe in Seiten). Im Fehlerfall darf `file_size` nicht geändert werden und muss den unveränderten Originalpuffer zurückgeben.

```c
void _start(void);
```
Einstiegspunkt für Tag-Plugins (`PLG_T_TAG`). Sie könnten neue Tags bei `tags_ptr` hinzufügen und diesen Zeiger auf eine neue,
8 Bytes ausgerichtete Position anpassen.
​
### Lokale Funktionen

Die Plugins können lokale Funktionen verwenden, diese *müssen* jedoch aufgrund eines CLang-Fehlers als `static` deklariert werden.
(Der Fehler besteht darin, dass CLang PLT-Datensätze für diese Funktionen generiert, obwohl das Flag „-fno-plt“ in der Befehlszeile
übergeben wird. Die Verwendung von `static` umgeht dieses Problem.)

Low-Level-Dateiformatspezifikation
----------------------------------

Falls jemand ein Plugin in einer Nicht-C-Sprache schreiben möchte (z. B. in Assembly), finden Sie hier die Beschreibung des
Dateiformats auf niedriger Ebene.

Es ist dem a.out-Format sehr ähnlich. Die Datei besteht aus einem Header fester Größe, gefolgt von Abschnitten unterschiedlicher
Länge. Es gibt keine Abschnittsüberschrift. Die Daten jedes Abschnitts folgen direkt den Daten des vorherigen Abschnitts in der
folgenden Reihenfolge:

```
(Header)
(Identifier-Match-Datensätze)
(Umzugsunterlagen)
(Codeabschnitt)
(schreibgeschützte Daten)
(initialisierte lesbare/schreibbare Daten)
```

Für den ersten echten Abschnitt, Maschinencode, ist die Ausrichtung enthalten. Für alle anderen Abschnitte wird der Abstand zur
Größe des vorherigen Abschnitts hinzugefügt.

TIPP: Wenn Sie ein Plugin als einzelnes Argument an `plgld` übergeben, werden die Abschnitte in der Datei mit einer Ausgabe
ausgegeben, die der von `readelf -a` oder `objdump -xd` ähnelt.

### Header

Alle Zahlen liegen unabhängig von der Architektur im Little-Endian-Format vor.

| Versatz | Größe | Beschreibung                                                  |
|--------:|------:|---------------------------------------------------------------|
|       0 |     4 | magische Bytes `EPLG`                                         |
|       4 |     4 | Gesamtgröße der Datei                                         |
|       8 |     4 | Gesamtspeicherbedarf beim Laden der Datei                     |
|      12 |     4 | Größe des Codeabschnitts                                      |
|      16 |     4 | Größe des schreibgeschützten Datenabschnitts                  |
|      20 |     4 | Einstiegspunkt des Plugins                                    |
|      24 |     2 | Architekturcode (identisch mit ELF)                           |
|      26 |     2 | Anzahl der Umzugsdatensätze                                   |
|      28 |     1 | Anzahl der ID-Übereinstimmungsdatensätze                      |
|      29 |     1 | höchster referenzierter GOT-Eintrag                           |
|      30 |     1 | Dateiformatrevision (vorerst 0)                               |
|      31 |     1 | Plugin Typ (1=Dateisystem, 2=Kernel, 3=Dekomprimierer, 4=Tag) |

Der Architekturcode ist derselbe wie in ELF-Headern, zum Beispiel 62 = x86_64, 183 = Aarch64 und 243 = RISC-V.

Der Typ des Plugins gibt den Prototyp des Einstiegspunkts an, der ABI ist immer SysV.

### Abschnitt „Bezeichnerübereinstimmung“

Dieser Abschnitt enthält so viele der folgenden Datensätze, wie im Feld „Anzahl der ID-Übereinstimmungsdatensätze“ des Headers
angegeben sind.

| Versatz | Größe | Beschreibung                                             |
|--------:|------:|----------------------------------------------------------|
|       0 |     2 | Versatz                                                  |
|       2 |     1 | Größe                                                    |
|       3 |     1 | Typ                                                      |
|       4 |     4 | passende magische Bytes                                  |

Zunächst wird der Betreffanfang in einen Puffer geladen. Es wird ein Akkumulator eingerichtet, zunächst mit einer 0. Offsets in
diesen Datensätzen beziehen sich immer auf diesen Akkumulator und adressieren dieses Byte im Puffer.

Das Feld „Typ“ gibt an, wie der Versatz zu interpretieren ist. Wenn es eine 1 ist, wird der Offset plus der Akkumulator als Wert
verwendet. Wenn es 2 ist, wird ein 8-Bit-Byte-Wert am Offset verwendet, 3 bedeutet, dass ein 16-Bit-Wortwert verwendet wird, und 4
bedeutet, dass ein 32-Bit-Dword-Wert verwendet wird. 5 bedeutet, dass man einen 8-Bit-Bytewert nimmt und den Akkumulator dazu
addiert, 6 bedeutet, dass man einen 16-Bit-Wortwert nimmt und den Akkumulator dazu addiert, und 7 bedeutet dasselbe, aber mit
32-Bit-Wert. 8 sucht in Offset-Schritten nach den Magic Bytes vom akkumulierenden Byte bis zum Ende des Puffers und gibt, wenn
sie gefunden werden, den passenden Offset als Wert zurück.

Wenn die Größe Null ist, wird der Akkumulator auf den Wert gesetzt. Wenn die Größe nicht Null ist, wird überprüft, ob diese Anzahl
Bytes mit den angegebenen magischen Bytes übereinstimmt.

Um beispielsweise zu prüfen, ob eine ausführbare PE-Datei mit einer NOP-Anweisung beginnt:
```c
EASY_PLUGIN(PLG_T_KERNEL) {
   /* Versetzt Größe Match-Typ     magische Bytes */
    { 0,          2, PLG_M_CONST, { 'M', 'Z', 0, 0 } },     /* Überprüfen Sie die magischen Bytes */
    { 60,         0, PLG_M_DWORD, { 0, 0, 0, 0 } },         /* Ermitteln Sie den Offset des PE-Headers zum Akkumulator */
    { 0,          4, PLG_M_CONST, { 'P', 'E', 0, 0 } },     /* Überprüfen Sie die magischen Bytes */
    { 40,         1, PLG_M_DWORD, { 0x90, 0, 0, 0 } }       /* Prüfen Sie, ob am Einstiegspunkt eine NOP-Anweisung vorhanden ist */
};
```

### Umzugsabteilung

Dieser Abschnitt enthält so viele der folgenden Datensätze, wie im Feld „Anzahl der Umzugsdatensätze“ des Headers angegeben sind.

| Versatz | Größe | Beschreibung                                             |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | Offset                                                   |
|       4 |     4 | Umzugsart                                                |

Bedeutung der Bits im Typ:

| Aus     | Zu    | Beschreibung                                             |
|--------:|------:|----------------------------------------------------------|
|       0 |     7 | Symbol (0 - 255)                                         |
|       8 |     8 | PC-relative Adressierung                                 |
|       9 |     9 | GOT-relative indirekte Adressierung                      |
|      10 |    13 | unmittelbarer Maskenindex (0 - 15)                       |
|      14 |    19 | Start Bit (0 - 63)                                       |
|      20 |    25 | Ende Bit (0 - 63)                                        |
|      26 |    31 | negierte Adressen-Flag-Bitposition (0 - 63)              |

Das Offset-Feld ist relativ zur Magie im Header des Plugins und wählt eine Ganzzahl im Speicher aus, an der die Verschiebung
durchgeführt werden muss.

Das Symbol gibt an, welche Adresse verwendet werden soll. 0 bedeutet die BASE-Adresse, an der das Plugin in den Speicher geladen
wurde, d. h. die Adresse der Magie des Headers im Speicher. Andere Werte wählen eine externe Symboladresse aus dem GOT aus, die
im Loader oder in einem anderen Plugin definiert ist. Sehen Sie sich das Array `plg_got` in der Quelle von plgld.c an, um zu sehen,
welcher Wert welchem Symbol entspricht. Wenn das relative GOT-Bit 1 ist, wird die Adresse des GOT-Eintrags des Symbols anstelle der
tatsächlichen Adresse des Symbols verwendet.

Wenn das relative PC-Bit 1 ist, wird der Offset zuerst von der Adresse abgezogen (relativer Adressierungsmodus des Befehlszeigers).

Der unmittelbare Maskenindex gibt an, welche Bits die Adresse im Befehl speichern. Wenn dieser 0 ist, wird die Adresse unabhängig
von der Architektur unverändert in den Offset geschrieben. Für x86_64 ist nur Index 0 zulässig. Für ARM Aarch64: 0 = wie es ist,
1 = 0x07ffffe0 (5 Bit nach links verschieben), 2 = 0x07fffc00 (10 Bit nach links verschieben), 3 = 0x60ffffe0 (mit
ADR/ADRP-Anweisungen wird das Unmittelbare verschoben und aufgeteilt zwei Bitgruppen). Zukünftige Architekturen könnten mehr und
unterschiedliche unmittelbare Bitmasken definieren.

Unter Verwendung der Sofortmaske werden Ende-Start + 1-Bits aus dem Speicher entnommen und vorzeichenbehaftet erweitert. Dieser Wert
wird zur Adresse addiert (Addend, bei internen Referenzen wird hier auch die Adresse des internen Symbols kodiert).

Wenn das negierte Adressflagbit nicht 0 ist und die Adresse positiv ist, wird dieses Bit gelöscht. Wenn die Adresse negativ ist,
wird dieses Bit gesetzt und die Adresse negiert.

Schließlich wählen die Start- und Endbits aus, welcher Teil der Adresse in die ausgewählte Ganzzahl geschrieben werden soll. Dies
definiert auch die Größe der Verschiebung. Bits außerhalb dieses Bereichs und diejenigen, die nicht Teil der unmittelbaren Maske
sind, bleiben unverändert.

### Codeabschnitt

Dieser Abschnitt enthält Maschinenanweisungen für die im Header angegebene Architektur und hat so viele Bytes, wie im Codegrößenfeld
angegeben ist.

### Schreibgeschützter Datenabschnitt

Dies ist ein optionaler Abschnitt, der möglicherweise fehlt. Es ist so lang, wie im schreibgeschützten Abschnittsgrößenfeld in der
Kopfzeile angegeben. Alle konstanten Variablen werden in diesem Abschnitt platziert.

### Initialisierter Datenabschnitt

Dies ist ein optionaler Abschnitt, der möglicherweise fehlt. Wenn die Datei nach dem Codeabschnitt (oder dem optionalen
schreibgeschützten Datenabschnitt) noch Bytes enthält, werden diese Bytes alle als Datenabschnitt betrachtet. Wenn eine Variable mit
einem Wert ungleich Null initialisiert wird, wird sie in diesen Abschnitt eingefügt.

### BSS-Abschnitt

Dies ist ein optionaler Abschnitt, der möglicherweise fehlt. Dieser Abschnitt wird niemals in der Datei gespeichert. Wenn das Feld
„Gesamtspeicherbedarf beim Laden der Datei“ größer ist als das Feld „Gesamtgröße der Datei“ im Header, wird die Differenz im
Speicher mit Nullen aufgefüllt. Wenn eine Variable nicht initialisiert ist oder als Null initialisiert ist, wird sie in diesem
Abschnitt platziert.
