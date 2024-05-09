Easyboot kompatíbilis kernelek készítése
========================================

Az [Easyboot](https://gitlab.com/bztsrc/easyboot) számos kernel formátumot támogat a [beépülők](plugins.md) segítségével. Ha
azonban egyetlen alkalmas beépülőt sem talált, akkor ELF64 vagy PE32+ formátumú, egyszerűsített (beágyazott adat nélküli)
Multiboot2 kompatíbilis binárisként próbálja meg betölteni a kernelt.

Pontosan ugyanazt a protokollt használja, mint a [Simpleboot](https://gitlab.com/bztsrc/simpleboot), minden abban a repóban
található példa kernel működik az **Easyboot** betöltővel is.

Használható a GRUB repójában lévő hivatalos multiboot2.h fejléc, vagy a [simpleboot.h](https://gitlab.com/bztsrc/simpleboot/blob/main/simpleboot.h)
C/C++ fejléc fájl, ami annyival egyszerűbb, hogy typedef-eket biztosít. Az alacsony szintű formátum ugyanaz, bármelyik másik
Multiboot2 függvénykönyvtárral is működik, akár még nem-C nyelvűvel is, mint például a [Rust](https://github.com/rust-osdev/multiboot2/tree/main/multiboot2/src)
függvénykönyvtár (megjegyzés: nincs semmiféle kapcsolatom a fejlesztőivel, egyszerűen csak rákerestem arra, hogy "Rust Multiboot2",
és ez volt a legelső találat).

[[_TOC_]]

Indulás
-------

### A betöltő betöltődése

*BIOS* gépeken a diszk legelső szektorát tölti be a förmver a 0:0x7C00 címre és ráadja a vezérlést. Ebben a szektorban az
**Easyboot** a [boot_x86.asm](../../src/boot_x86.asm) kódját tárolja, ami elég okos ahhoz, hogy megtalálja és betöltse a a második
lépcsős betöltőt, valamint hogy beállítsa a long módot hozzá.

*UEFI* gépeken ugyanezt a második lépcsős betöltő fájlt, aminek a neve `EFI/BOOT/BOOTX64.EFI`, közvetlenül a förmver tölti be.
Ennek a forrása a [loader_x86.c](../../src/loader_x86.c) alatt található. Ez minden, az **Easyboot** nem olyan, mint a GRUB vagy
a syslinux, amiknek tucatnyi rendszerfájlra van szükségük a lemezen. Itt egyetlen egy fájl van csak (a beépülők opcionálisak,
a Multiboot2 protokollhoz nincs szükség egyre sem).

*Raspberry Pi* alatt a betöltő neve `KERNEL8.IMG`, ami a [loader_rpi.c](../../src/loader_rpi.c) forrásból fordul.

### A betöltő

Ez a betöltő nagyon körültekintően lett megírva, hogy több, különböző konfiguráción is el tudjon futni. Betölti a GUID Partíciós
Táblát a lemezről, és "EFI System Partition"-t keres benne. Ha ez megvan, akkor betölti erről a boot partícióról az `easyboot/menu.cfg`
nevű konfigurációs fájlt. Miután a boot opció kiválasztásra került és a kernel fájlneve ismerté válik, azt is megkeresi és betölti.

Ezután autodetektálja a kernel formátumát, és elég okos ahhoz, hogy értelmezze a szekció és szegmens információkat, így magától
tudja, mit hova kell betölteni (ha szükséges, akkor a laptábla leképezéseket is beállítja). Aztán beállítja a környezetet a detektált
formátumnak megfelelően (Multiboot2 / Linux / stb. védett vagy long mód, ABI paraméterek stb.). Miután a gép állapota megfelel az
elvártaknak, a legutolsó lépésként a betöltő ráugrik a kernel belépési pontjára.

Gép állapota
------------

Minden, ami a gép állapotáról (Machine State) szerepel a [Multiboot2 specifikáció](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)ban
itt is érvényes, kivéve az általános célú regisztereket. Az **Easyboot** két paramétert ad át a kernel belépési pontjának, a SysV
ABI-nak valamint a Microsoft fastcall ABI-nak is megfelelve egyszerre. Az első paraméter a mágikus érték, a második egy fizikai
memóriacím, ami a Multiboot Információs teglistára mutat (a továbbiakban csak MBI, lásd alább).

Ugyancsak megsérti a Multiboot2 protokollt, hogy a felső címterű kerneleket is támogatja. A Multiboot2 azt írja elő, hogy a
memóriának identikusan leképezettnek kell lennie. Nos, ez csak részben teljesül az **Easyboot** esetén: csak annyi garantált, hogy
a teljes fizikai memória identikusan van leképezve, ahogy az elvárt, azonban bizonyos efölötti (és a kernel programfejlécétől függő)
további memóriacímek is elérhetők. Ez nem zavar be a hagyományos Multiboot2 kompatíbilis kerneleknek, amiknek egyébként sem szabad
az elérhető fizikai RAM-on kívüli memóriát címezniük.

A kernel pontosan ugyanúgy töltődik be BIOS és UEFI alatt, valamint RPi-n is, a förmver különbségek egyszerűen "Másvalaki Problémája".
Az egyetlen amit a kernel lát a dologból, hogy az MBI-ben van-e EFI system table teg vagy nincs. Hogy mégegyszerűbbé tegye az életet,
az **Easyboot** nem generál EFI memory map (17-es típusú) teget sem, hanem csak a [Memóriatérkép](#memóriatérkép) (6-os típusú) teget,
függetlenül attól, hogy milyen platformon indul (így UEFI alatt is, ott a memóriatérképet egyszerűen átkonvertálja, így a kernelnek
mindössze egyetlen féle memóriatérkép teggel kell csak foglalkoznia). A régi és elévült tegeket sem generálja ez a rendszerbetöltő.

A kernel felügyeleti szinten (0-ás gyűrű x86-on, EL1 ARM alatt) fut.

A GDT nincs meghatározva, de érvényes. A verem valahol az alsó 640k-ba kerül, és lefelé nő (a kernelnek érdemes számára megfelelő
vermet beállítania amint csak lehet).

Az IDT-t is meghatározatlannak kell tekinteni; az IRQ-k, NMI és a szoftveres megszakítások le vannak tiltva. Gagyi kivételkezelők
azonban beállításra kerülnek, amik minimális regiszterdumpot jelenítenek meg, majd leállítják a gép futását. Ezekre csak addig
szabad hagyatkozni, amíg a kernel be nem állítja a saját IDT-jét és kivételkezelőit, lehetőleg minnél hamarabb. ARM alatt a vbar_el1
hasonlóan kerül beállításra, ugyanilyen gagyi kivételkezelőket hív (habár ott természetesen más regiszterek jelennek meg).

A képernyő pixel központú framebuffere is alapból beállításra kerül. A felbontás állítható a konfig fájlban, de ennek hiányában is
beállítódik a grafikus képernyő.

Nagyon fontos, hogy a kernel soha se térjen vissza. A betöltő memóriája szabadon felhasználható (amint a kernel végzett az MBI
tegek értelmezésével), így egész egyszerűen nincs hova visszatérni. "Der Mohr hat seine Schuldigkeit getan, der Mohr kann gehen."

A kernelnek átadott Boot Információ (MBI)
-----------------------------------------

Elsőre talán nem nyilvánvaló, de a Multiboot2 specifikáció igazából két, egymástól teljesen független teg csoportot definiál:

- Az első csoportba tartozókat a Multiboot2 kompatíbilis kernelekbe kell beágyazni, OS kép Multiboot2 fejléc a nevük (3.1.2-es
  fejezet), és mint ilyen, a *kernel nyújtja a betöltő felé*. Az **Easyboot** egyáltalán nem törődik ezekkel, és nem is keres
  ilyeneket a kernelben. Egész egyszerűen **Easyboot** használatánál nem kell semmilyen mágikus bájtokat beágyazni, az ELF és PE
  fejlécek önmagukban bőven elegendők.

- A második csoportba azok tartoznak, amik dinamikusan a *kernelnek adódnak át* induláskor, az **Easyboot** csak ezeket használja,
  bár nem generálja le az összes Multiboot2 specifikációban szereplőt (a régi és elavult tegeket kihagyja). Ezeket hívják MBI tegeknek,
  lásd [Boot információ](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information-format) (3.6-os fejezet).

MEGJEGYZÉS: a Multiboot2 specifikáció MBI tegek fejezete bugos, mint állat. Alább egy javított változat található, ami megegyezik
a GRUB forrásában található multiboot2.h fejlécfájlban szereplő definíciókkal.

A kernelnek átadott első paraméter a 0x36d76289 mágikus érték (`rax`, `rcx` valamint `rdi`). Az MBI tegekre a második paraméter
mutat (`rbx`, `rdx` és `rsi`). ARM platformon a mágikus érték az `x0`-ban, a cím az `x1`-ben adódik át. RISC-V és MIPS architektúrákon
az `a0` és az `a1` használatos. Amennyiben ez a betöltő egy új architektúrára kerül portolásra, akkor mindig az adott architektúra
SysV ABI szerinti függvényparaméter regisztereit kell használni. Ha más ABI is általánosan elterjedt azon a platformon, és az
nem ütközik a SysV ABI-val, akkor a paraméterértékeket le kell másolni annak az ABI-nak a regisztereibe (vagy a verembe) is.

### Fejlécek

Az átadott cím mindig 8-bájtra igazított, és az MBI fejléccel kezdődik:

```
        +-------------------+
u32     | total_size        |
u32     | reserved          |
        +-------------------+
```

A `total_size` az MBI lista teljes méretét tartalmazza, míg a `reserved` nem használatos. Ezt a fejlécet követik a tegek, ugyancsak
mindig 8-bájtos címre igazítva. Minden teg egységesen a következő fejléccel indul:

```
        +-------------------+
u32     | type              |
u32     | size              |
        +-------------------+
```

A `type` azonosítja a teget, és ez mondja meg, hogy a további mezőket hogy kell értelmezni. A `size` tartalmazza a teg méretét
bájtokban, beleértve a fejlécet magát is, de a következő teg igazításhoz szükséges bájtokat nem. A tegek közé esetlegesen bájtok
kerülnek beszúrásra, hogy minden teg 8-bájtos címen kezdődjön.

### Lezáró

```
        +-------------------+
u32     | type = 0          |
u32     | size = 8          |
        +-------------------+
```

A tegek listáját egy olyan teg zárja, amiben a típus `0` és a méret `8`.

### Induló parancssor

```
        +-------------------+
u32     | type = 1          |
u32     | size              |
u8[n]   | string            |
        +-------------------+
```

`string` tartalmazza azt az induló parancssort (boot command line), ami a *menuentry* `kernel` sorában lett megadva (a kernelfájl
elérési útja és fájlneve nélkül). A parancsor egy normál C típusú, nullával lezárt UTF-8 sztring.

### Betöltő neve

```
        +----------------------+
u32     | type = 2             |
u32     | size = 17            |
u8[n]   | string "Easyboot"    |
        +----------------------+
```

`string` tartalmazza a rendszerbetöltő nevét, ami a kernelt betöltötte. A név egy normál C típusú, nullával lezárt UTF-8 sztring.

### Modulok

```
        +-------------------+
u32     | type = 3          |
u32     | size              |
u32     | mod_start         |
u32     | mod_end           |
u8[n]   | string            |
        +-------------------+
```

Ez a teg jelzi a kernel számára, hogy a kernellel együtt milyen más modulok kerültek betöltésre, és hogy azok hol találhatók.
A `mod_start` a modult tartalmazó buffer kezdő fizikai címére mutat, a `mod_end` pedig a buffer végére. Gzip tömörített adat
sosem fog szerepelni, mert az **Easyboot** transzparensen kicsomagolja azokat (és ha a megfelelő beépülő biztosítva van, akkor
más, nem gzip tömörített buffer sem lesz). A `string` mező egy tetszőleges, az adott modulhoz kapcsolódó sztringet tartalmaz;
egy normál C típusú, nullával lezárt UTF-8 sztring. Ez a *menuentry* `module` sorában kerül megadásra, és a használata operációs
rendszer függő. A parancssor teggel ellentétben a modul tegek *tartalmazzák* a modul elérési útját és fájlnevét.

Modulonként egy teg szerepel. Ez a teg típus többször is előfordulhat a listában. Ha induló lemezkép (initial ramdisk) is be lett
töltve a kernellel együtt, akkor az a legelső modulként fog megjelenni.

Van egy különleges eset, ha a fájl egy DSDT ACPI tábla, FDT (dtb) vagy GUDT bitkolbász, ekkor nem modulként jelenik meg, hanem a
régi ACPI RSDP (14-es típus) vagy az új ACPI RSDP (15-ös típus) lesz megpecselve, hogy a bennük lévő DSDT mutató ennek a fájlnak a
tartalmára mutasson.

### Memóriatérkép

Ez a teg adja át a memóriatérképet.

```
        +-------------------+
u32     | type = 6          |
u32     | size              |
u32     | entry_size = 24   |
u32     | entry_version = 0 |
varies  | entries           |
        +-------------------+
```

A `size` mező tartalmazza az összes bejegyzés méretét, beleértve ezt a fejlécet is. Az `entry_size` egy bejegyzés mérete, mindig 24.
Az `entry_version` a bejegyzés formátuma, jelenleg mindig `0`. Minden `entries` bejegyzés a következő formátumú:

```
        +-------------------+
u64     | base_addr         |
u64     | length            |
u32     | type              |
u32     | reserved          |
        +-------------------+
```

`base_addr` a memóriablokk kezdő fizikai címe. A `length` a memóriablokk mérete bájtokban. A `type` mondja meg, hogy milyen típusú
az adott memóriablokk, ahol az `1` jelenti a használható RAM-ot, a `3` jelenti a használható RAM-ot, ami ACPI információt tartalmaz,
a `4` jelenti a foglalt memóriát, amit meg kell őrizni hibernálás során, az `5` jelenti a hibás RAM-ot, és minden más érték jelentése
nem használható memóriaterület. A `reserved` BIOS rendszereken mindig `0`.

Amikor UEFI gépen generálódik az MBI, akkor a különféle EFI memóriatérkép típusok `1`-es (felhasználható RAM) vagy `2`-es (foglalt
memória) bejegyzésként kerülnek átadásra, és ha esetleg szükség lenne rá, az eredeti EFI típus a `reserved` mezőbe kerül.

Az átadott térkép garantáltan tartalmazza az összes, normálisan használható RAM-ot, és mindig `base_addr` szerint növekvő sorrendbe
van rendezve. Ez a használható RAM mindazonáltal tartalmazza a kernel által elfoglalt memóriát, az MBI-t, szegmenseket és modulokat
is. A kernelnek nagyon körültekintőnek kell lennie, nehogy idő előtt felülírja ezeket (az **Easyboot** könnyedén kihagyhatná ezeket
a listából, de akkor sérülne a Multiboot2 kompatíbilitás).

### Framebuffer infó

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

A `framebuffer_addr` mező tartalmazza a framebuffer fizikai címét. A `framebuffer_pitch` egy rasztersor hossza, bájtokban. A
`framebuffer_width`, `framebuffer_height` pedig a framebuffer szélessége és magassága pixelekben. A `framebuffer_bpp` mező adja meg,
hogy egy pixel hány bitből áll. A `framebuffer_type` mindig 1, és a `reserved` mindig 0 a jelenlegi specifikáció szerint, és a
kernelnek nem szabad használnia ezeket a mezőket. A többi mező a pixel formátumát írja le, az egyes csatornák bitpozíciójával és
hosszával bitekben. A `((~(0xffffffff << size)) << position) & 0xffffffff` kifejezés használható arra, hogy UEFI GOP szerű
csatornamaszkokká alakítsuk ezeket.

### EFI 64-bit rendszer tábla mutató

Ezt a teget csak akkor generálja az **Easyboot**, ha UEFI gépen fut. BIOS gépeken sosincs ilyen teg.

```
        +-------------------+
u32     | type = 12         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

A `pointer` az "EFI system table"-re mutató mutató.

### EFI 64-bit kép hivatkozás mutató

Ezt a teget csak akkor generálja az **Easyboot**, ha UEFI gépen fut. BIOS gépeken sosincs ilyen teg.

```
        +-------------------+
u32     | type = 20         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

A `pointer` az "EFI image handle"-re mutató mutató. Általában ez a rendszerbetöltő hivatkozása.

### SMBIOS tablák

```
        +-------------------+
u32     | type = 13         |
u32     | size              |
u8      | major             |
u8      | minor             |
u8[6]   | reserved          |
        | smbios táblák     |
        +-------------------+
```

Ez a teg az SMBIOS táblák verzióját és másolatát tartalmazza.

### Régi ACPI RSDP

```
        +-------------------+
u32     | type = 14         |
u32     | size              |
        | RSDPv1 másolat    |
        +-------------------+
```

Ez a teg az ACPI 1.0 specifikáció szerinti RSDP másolatát tartalmazza (32-bites címmekkel).

### Új ACPI RSDP

```
        +-------------------+
u32     | type = 15         |
u32     | size              |
        | RSDPv2 másolat    |
        +-------------------+
```

Ez a teg az ACPI 2.0 és későbbi specifikációk szerinti RSDP másolatát tartalmazza (64-bites címmekkel).

Ezek a tegek (14-es és 15-ös) egy `RSDT` vagy `XSDT` táblára mutatnak, amiben van egy mutató egy `FACP` táblára, amiben további
két mutató található a `DSDT` táblára, ami leírja a gép felszereltségét. Az **Easyboot** lehazudja ezeket a táblákat az olyan
gépeken, amik alapból nem támogatják az ACPI-t. Továbbá ha modulként megadásra kerül egy DSDT tábla, FDT (dtb) vagy GUDT bitkolbász,
akkor az **Easyboot** felülírja ezeket a mutatókat, hogy a felhasználó által megadott táblára mutassanak. Ezen tábla értelmezéséhez
használható a függőségmentes, egyetlen fejlécfájlból álló [hwdet](https://gitlab.com/bztsrc/hwdet) függvénykönyvtáram (vagy a
behemót [apcica](https://github.com/acpica/acpica) és [libfdt](https://github.com/dgibson/dtc)).

Kernelspecifikus tegek
----------------------

Az olyan tegek, amiknél a `type` nagyobb vagy egyenlő 256-tal, nem szerepelnek a Multiboot2 specifikációban. Ezeket opcionális
[beépülők](plugins.md) adhatják hozzá a listához, amennyiben a kernel igényt tart rájuk.

### EDID

```
        +-------------------+
u32     | type = 256        |
u32     | size              |
        | EDID másolat      |
        +-------------------+
```

Ez a teg az EDID specifikáció szerinti támogatott monitorfelbontások listájának másolatát tartalmazza.

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

Ez a teg akkor szerepel, ha a `multicore` direktíva meg lett adva. A `numcores` tartalmazza a processzormagok számát a rendszerben,
a `running` amik sikeresen inicializáltak és mind a kernelt futtatják, párhuzamosan. A `bspid` a BSP mag azonosítója (x86-on az
lAPIC id), hogy a kernelek meg tudják különböztetni az AP-ket, és más kódot tudjanak futtatni azokon. Minden AP-nek saját, külön
verme van, és a verem tetején az aktuális mag azonosítója található.

Memória elrendezése
-------------------

### BIOS gépek

| Kezdete  | Vége    | Leírás                                                       |
|---------:|--------:|--------------------------------------------------------------|
|      0x0 |   0x400 | Interrupt Vector Table (használható, valós módú IDT)         |
|    0x400 |   0x4FF | BIOS Data Area (használható)                                 |
|    0x4FF |   0x500 | BIOS induló meghajtókód (valószínűleg 0x80, használható)     |
|    0x500 |  0x1000 | kivételkezelő verem (saját IDT beállítása után használható)  |
|   0x1000 |  0x8000 | laptáblák (saját laptáblák beállítása után használható)      |
|   0x8000 | 0x20000 | betöltő kód és adat (saját IDT beállítása után használható)  |
|  0x20000 | 0x40000 | konfiguráció + tegek (MBI értelmezése után használható)      |
|  0x40000 | 0x90000 | beépülőazonosítók; fentről lefelé: kernel verem              |
|  0x90000 | 0x9A000 | kizárólag Linux kernel: zero page + cmdline                  |
|  0x9A000 | 0xA0000 | Extended BIOS Data Area (jobb nem piszkálni)                 |
|  0xA0000 | 0xFFFFF | VRAM és BIOS ROM (nem használható)                           |
| 0x100000 |       x | kernel szegmensek, utánna a modulok, laphatárra igazítva     |

### UEFI gépek

Ki tudja. Az UEFI úgy allokálja a memóriát, ahogy neki jólesik. Számíts bármire és mindenre, de valószínűleg 64M alatt lesz minden.
Minden külön listázva lesz a memóriatérképben type = 1 (`MULTIBOOT_MEMORY_AVAILABLE`) és reserved = 2 (`EfiLoaderData`) bejegyzésekben,
de ez nem kízárólagos, azaz a fentieken kívül egyéb memóriatípusok is szerepelhetnek ugyanilyen típussal (a betöltő bss szegmense
például).

### Raspberry Pi

| Kezdete  | Vége    | Leírás                                                        |
|---------:|--------:|---------------------------------------------------------------|
|      0x0 |   0x100 | förmver által használt (jobb nem piszkálni)                   |
|    0x100 |  0x1000 | kivételkezelő verem (saját VBAR beállítása után használható)  |
|   0x1000 |  0x9000 | laptáblák (saját laptáblák beállítása után használható)       |
|   0x9000 | 0x20000 | betöltő kód és adat (saját VBAR beállítása után használható)  |
|  0x20000 | 0x40000 | konfiguráció + tegek (MBI értelmezése után használható)       |
|  0x40000 | 0x80000 | förmver nyújtotta FDT (dtb); fentről lefelé: kernel verem     |
| 0x100000 |       x | kernel szegmensek, utánna a modulok, laphatárra igazítva      |

Az első pár bájt az [armstub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S)nak van fenntartva. Csak az
első processzor (0-ás) mag indul el, ezért a többi indításához egy funkció címét kell írni a 0xE0 (1-es mag), 0xE8 (2-es mag) és
0xF0 (3-as mag) címekre, amik ezen a területen találhatók.

Habár alapesetben az RPi nem támogatja, mégis van régi ACPI RSDP (14-es típus) teg, gyártott adatokkal. Az `APIC` tábla tartalmazza
a kernel számára használható processzormagok adatait. Az indulófunkció címe az RSD PTR -> RSDT -> APIC -> cpu\[x].apic_id mezőben
található (és a mag azonosítója a cpu\[x].acpi_id mezőben, ahol a BSP esetén cpu\[0].acpi_id = 0 és cpu\[0].apic_id = 0xD8. Figyelem,
az "acpi" és az "apic" nagyon hasonló).

Amennyiben a förmver érvényes FDT blobot ad át, vagy az egyik modul .dtb, .gud illetve .aml formátumú, akkor egy FADT tábla (`FACP`
mágikus értékkel) is generálódik. Ebben a táblában a DSDT mutató (32-bites a 40-es offszeten) az átadott flattened device tree-re
fog mutatni.

Habár memóriatérképet sem biztosít a förmver, mégis lesz Memóriatérkép (6-os típus) teg is, amiben szerepelni fog a detektált RAM
és az MMIO terület is. Ez használható arra, hogy megállapítsuk az MMIO bázis címét, ami eltér RPi3 és RPi4 modellek esetén.
