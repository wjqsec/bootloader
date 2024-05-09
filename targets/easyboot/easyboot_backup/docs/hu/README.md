Kernelek betöltése Easyboot-al
==============================

Az [Easyboot](https://gitlab.com/bztsrc/easyboot) egy minden-az-egyben rendszerbetöltő menedzser és indítható lemezkép készítő,
ami különböző operációs rendszer kerneleket és különféle bináris formátumú, Multiboot2 kompatíbilis kerneleket tud betölteni.

[[_TOC_]]

Telepítés
---------

```
 easyboot [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e] [-c] <indir> <outfile>

  -v, -vv         bőbeszédűség / ellenőrzés szintjének növelése
  -s <mb>         lemezkép méretének megadása (alapból 35M)
  -b <mb>         boot partíció méretének megadása (alapból 33M)
  -u <guid>       boot partíció egyedi azonosítójának megadása (alapból véletlen)
  -p <t> <u> <i>  további partitió hozzáadása (típus guid, egyedi guid, képfájl)
  -e              El Torito Boot Catalog hozzáadása (EFI CDROM boot támogatás)
  -c              mindig új lemezkép létrehozása, ha létezik, akkor is
  <indir>         ennek a mappának a tartalmából csinálja a boot partíciót
  <outfile>       kimeneti lemezkép vagy eszközfájl neve
```

Az **Easyboot** eszköz egy `(outfile)` nevű, indítható lemezképet hoz létre GUID Partíciós Táblával és egyetlen, FAT32-re formázott
partícióval, aminek a neve "EFI System Partition" (röviden ESP). A partíció tartalmát a felhasználó által megadott `(indir)`
könyvtárból veszi. Ebbe a könyvtárba el kell helyezni egy sima szöveges fájlt `easyboot/menu.cfg` néven. Akár NL, akár CRLF
sorvégekkel ez könnyen szerkeszthető bármilyen szövegszerkesztő programmal. A konfigurációtól függően szükség lehet még
`easyboot/*.plg` nevű [beépülők](plugins.md)re is ebben a könyvtárban.

A lemezkép működik Raspberry Pi-n is, és el fog indulni qemu-n; de ahhoz, hogy igazi vason is fusson, szükség lesz további förmver
fájlokra, `bootcode.bin`, `fixup.dat`, `start.elf` és egy .dtb fájlra az `(indir)` könyvtárban, ezeket a Raspberry Pi
[hivatalos repó](https://github.com/raspberrypi/firmware/tree/master/boot)jából lehet letölteni.

A parancsnak van néhány opcionális parancssori kapcsolója: `-s (mb)` beállítja a generálandó teljes lemezkép méretét Megabájtokban,
míg a `-b (mb)` a boot partíció méretét állítja, szintén Megabájtokban. Nyilvánvalóan az előbbinek nagyobbnak kell lennie, mint az
utóbbinak. Ha nincs megadva, akkor a partíció mérete a megadott könyvtár méretéből számítódik (de legalább 33 Mb, ez a lehető
legkissebb FAT32-es fájlrendszer méret) és a lemezméret pedig ennél 2 Mb-al nagyobb (igazítások és a partíciós tábla által
elfoglalt hely miatt). Ha e két megadott méret között több, mint 2 Mb a különbség, akkor külsős programmal, például az `fdisk`-el
szabadon, kedved szerint hozzáadhatsz további partíciókat a lemezképhez (vagy lásd a `-p` kapcsolót alább). Ha kiszámítható
kimenetre van szükség, akkor a boot partíció egyedi azonosítója (UniquePartitionGUID) megadható a `-u <guid>` kapcsolóval.

Opcionálisan hozzáadható további partíció is a `-p` kapcsolóval. Ennek 3 paramétere van: (PartitionTypeGUID), (UniquePartitionGUID)
és annak a képfájlnak a neve, ami a partíció adatait tartalmazza.

A `-e` kapcsoló hatására El Torito Boot Catalog is generálódik a lemezképbe, így az nemcsak USB pendrive-on lesz használható, hanem
EFI CDROM lemezképként is indítható lesz.

Amennyiben az `(outfile)` egy eszközfájl (pl. `/dev/sda` Linuxon, `/dev/disk0` BSD-ken, vagy `\\.\PhysicalDrive0` Windowson), akkor
nem hoz létre sem GPT-t, sem ESP-t, hanem megkeresi az eszközön már meglévőket, és azokat használja. Ilyenkor is minden fájlt
felmásol az `(indir)` könyvtárból a boot partícióra és telepíti a betöltő programokat. Ez akkor is így működik, ha a megadott
lemezkép egy már létező fájl (ez esetben használható a `-c` kapcsoló, ami először mindig egy új, üres lemezképet kreál).

Konfigurálás
------------

Az `easyboot/menu.cfg` fájlban a következő sorok szerepelhetnek (nagyon hasonló a grub.cfg szintaktikájához, példa konfigurációs
fájlt [itt találsz](menu.cfg)):

### Megjegyzések

Minden sor, ami a `#` karakterrel kezdődik, kommentnek minősül és a sor végéig kihagyásra kerül.

### Bőbeszédűség

A bőbeszédűség szintje a `verbose` kezdetű sorral állítható.

```
verbose (0-3)
```

Ez megmondja a betöltőnek, hogy mennyi információt írjon ki az induló konzolra. A 0 teljesen csendes (alapértelmezett), míg a 3
kiírja a betöltött kernelszegmenseket és még a belépési pont gépikódját is dumpolja.

### Framebuffer

Kérhető egy bizonyos képernyő felbontás a `framebuffer` kezdetű sorral. A formátuma a következő:

```
framebuffer (szélesség) (magasság) (pixelenkénti bitméret) [#(betűszín)] [#(háttérszín)] [#(foly.háttér)]
```

Az **Easyboot** mindenképp beállítja a framebuffert, még akkor is, ha ez a sor nem szerepel (alapból 800 x 600 x 32bpp). De ha
szerepel, akkor megpróbálja az itt megadott felbontást beállítani. Palettás módokat nem támogat, ezért a pixelenkénti bitméretnek
legalább 15-nek kell lennie.

Ha a negyedik opcionális szín paraméter meg van adva, akkor HTML formátumúnak kell lennie, egy kettőskereszttel indítva, amit
6 hexadecimális szám követ, PPZZKK. Például a `#ff0000` egy teljes fényerejű piros és a `#007f7f` egy sötétebb cián. Ez mondja
meg a betű színét. Hasonlóan, ha az ötödik opcionális szín paraméter meg van adva, annak is HTML formátumúnak kell lennie, ez
a háttér színét állítja. Az utolsó opcionális szín paraméter szintúgy, ez a folyamatjelző háttérszínét állítja.

Ha a szinek nincsenek megadva, akkor alapból fehér színnel ír fekete háttére, és a folyamatjelző sötétszürke.

### Alapértelmezett boot opció

A `default`-al kezdődő sor adja meg, hogy melyik menüpontot kell elindítani adott idő után, ha a felhasználó addig nem választ.

```
default (menüpont sorszám) (időkeret msec)
```

A menüpont sorszám egy szám 1-től 9-ig. Az időkeret milliszekundumokban van megadva (egy ezred másodperc), tehát 1000 jelent egy
másodpercet.

### Menü elrendezés

A `menualign` kezdetű sor állítja, hogyan jelenjenek meg a boot opciók.

```
menualign ("vertical"|"horizontal")
```

Alapból a menü `horizontal` (vízszintes), de ez átállítható `vertical`-ra (függőleges).

### Boot opciók

Összesen 9 boot opciót lehet megadni a `menuentry` kezdetű sorokkal. A formátuma a következő:

```
menuentry (ikon) [cimke]
```

Minden ikonhoz léteznie kell egy `easyboot/(ikon).tga` fájlnak a boot partíción. A képnek run length encoded, color-mapped
[Targa formátum](../en/TGA.txt)únak kell lennie, mert az a legjobban tömörített változat (az első három bájt a fájlban rendre
`0`, `1` és `9`, lásd Data Type 9 a specifikációban). A dimenziója pontosan 64 pixel magas és 64 pixel széles.

GIMP alatt ebben a formátumban úgy kell menteni, hogy először kiválasztjuk az "Image > Mode > Indexed..." menüpontot, a felugró
ablakban beállítjuk a "Maximum number of colors"-t 256-ra. Aztán "File > Export As..." és a fájlnévnek olyant adunk meg, ami
`.tga`-ra végződik, aztán a felugró ablakban bepipáljuk az "RLE compression" opciót. Parancssori megoldáshoz használható az
ImageMagick, `convert (bármilyen formátumú kép) -colors 256 -compress RLE ikon.tga`.

Az opcionális cimke főleg arra való, hogy ASCII verzió vagy kiadás információt jelenítsünk meg a menüben, nem pedig tetszőleges
szövegekre, ezért helytakarékosságból az UTF-8 csak akkor van támogatva, ha létezik egy `easyboot/font.sfn` fájl is. (Egy UNICODE
font rengeteg helyet foglal, még úgy is, hogy a [Scalable Screen Font](https://gitlab.com/bztsrc/scalable-font2) roppant hatékony,
kábé 820K. Összehasonlításképp a GRUB unicode.pf2-je még ennél is jóval több, 2392K, annak ellenére, hogy mindkét font kb. 55600
glifet tartalmaz 8x16-os és 16x16-os bitmapekben. A beépített ASCII fontban 8x16-os bitmap és 96 glif van csak.)

Minden sor, ami egy `menuentry` sort követ, ahhoz a menüponthoz tartozik, kivéve, ha az is egy újabb menuentry sor.
Kényelmi okokból használhatók blokkok, mint a GRUB-ban, de ezek csak szemgyönyörködtetésre valók. Igazából ugyanúgy kezeli a
kapcsoszárójeleket, mint bármilyen más szóköz karaktert. Elhagyhatók, és tab használható helyettük, mint Pythonban vagy a
Makefileokban, ha valakinek az lenne a heppje.

Például
```
menuentry FreeBSD backup
{
    kernel bsd.old/boot
}
```

### Partícióválasztás

Az olyan sor, ami `partition`-al kezdődik, egy GPT partíciót választ ki. Meg kell előznie egy `menuentry` sornak.

```
partition (partíció egyedi UUID)
```

Ezt a partíciót fogja root fájlrendszernek használni a boot opció. A kernel valamint a modulok erről a partícióról fognak
betöltődni, és a boot protokolltól függően a kernelnek is ez adódik át. A megadott partíció lehet bármelyik lemezen, nem kell
feltétlenül az indítólemezen lennie, az **Easyboot** induláskor végignézi az összes GPT partícionált lemezt, hogy megkeresse.

Kényelmi okokból a partíció megadható a `kernel` sorral is (lásd alább). Ha az induló parancssor tartalmaz `root=(UUID)` vagy
`root=*=(UUID)` sztringet, akkor nincs szükség külön `partition` sorra.

Ha a partíció nincs kifejezetten megadva, akkor a kernel és a modulok a boot partícióról töltődnek be.

### Kernel megadása

A `kernel` kezdetű sor mondja meg, hogy melyik fájlt kell bebootolni és hogy milyen paraméterekkel. Meg kell előznie egy `menuentry`
sornak.

```
kernel (kernel fájl elérési út) (opcionális induláskori parancssor)
```

Az UTF-8 elérési útnak abszolútnak kell lennie, és egy létező, futtatható kernel fájlra kell mutatnia a root (vagy boot) partíción.
Ha a kernel nem a partíció gyökérkönyvtárában található, akkor a könyvtárelvasztó mindig a `/`, még UEFI rendszereken is. Ha a
névben szóköz található, akkor azt egy `\` előtaggal kell ellátni. Az elérési út után egy szóközzel elválasztva jöhet az induló
parancssor. Multiboot2 kompatíbilis kernelek esetén ez a *Induló parancssor* (1-es típusú) tegben kerül átadásra. Ezt az
**Easyboot** nem módosítja, és nem is értelmezi, leszámítva, hogy root partíció specifikációt keres benne.

Alapból az **Easyboot** Multiboot2 kompatíbilis ELF64 és PE32+/COFF formátumú kerneleket kezel (valamint UEFI rendszereken UEFI
applikációkat is). Alapesetben ez a boot protokoll nem támogatja a magas címtérbe töltést, de az **Easyboot** megszegi kicsit a
protokollt úgy, hogy az a normál, alacsony címterű kerneleket ne zavarja. Bármilyen más fajta kernel bootolásához kernel betöltő
[beépülő](plugins.md)re van szükség.

FONTOS: a GRUB-bal ellentétben, ahol külön "linux" meg "multiboot" parancsok vannak a boot protokoll kiválasztására, itt csak
egyetlen parancs van, és a protokollt futás időben autodetektálja a kernelből.

### További modulok betöltése

Tetszőleges fájlok (induló memória lemezképek, kernel meghajtók, stb.) tölthetők be a kernellel együtt a `module` kezdetű sorokkal.
Ezeket meg kell előznie egy `menuentry` sornak. Fontos, hogy ilyen sorból több is lehet egy menuentry alatt. Ha a boot protokoll
támogat initrd-t, akkor a legelső `module` sort veszi initrd-nek.

```
module (fájl elérési út) (opcionális modul paraméterek)
```

Az UTF-8 elérési útnak abszolútnak kell lennie, és egy létező fájlra kell mutatnia a root (vagy boot) partíción. Ezt követheti egy
paraméterlista, szóközzel elválasztva. Ha a fájl tömörített, és van hozzá kitömörítő [beépülő](plugins.md), akkor a modul
transzparensen ki lesz csomagolva. A betöltött (és kitömörített) modulok listája a Multiboot2 kompatíbilis kerneleknek a *Modulok*
(3-as típusú) tegekben adódik át.

Van egy speciális eset, amikor a modul a `DSDT`, `GUDT` vagy a `0xD00DFEED` bájtokkal kezdődik. Ebben az esetben a fájl nem kerül a
*Modulok* tegekbe, hanem az ACPI tábla kerül módosításra úgy, hogy a benne lévő DSDT mutató ennek a fájlnak a tartalmára mutasson.
Ezáltal könnyedén kicserélhető egy bugos BIOS ACPI táblája a felhasználó által megadottra.

### Boot logó

A boot opció kiválasztásakor a képernyő közepén megjeleníthető egy logó, ha van egy `bootsplash` kezdetű sor. Meg kell előznie
egy `menuentry` sornak.

```
bootsplash [#(háttérszín)] (tga fájl elérési út)
```

A háttérszín opcionális, és ha meg van adva, akkor HTML formátumúnak kell lennie, egy kettőskereszttel indítva, amit 6 hexadecimális
szám követ, PPZZKK. Ha az első paraméter nem `#`-el kezdődik, akkor azt elérési útnak veszi.

Az UTF-8 elérési útnak abszolútnak kell lennie, és egy létező fájlra kell mutatnia a boot (és NEM a root) partíción. A képnek run
length encoded, color-mapped Targa formátumúnak kell lennie, akár csak az ikonoknak. A mérete bármi lehet, ami kifér a képernyőre.

### Többmagos támogatás

Hogy a kernel minden processzormagon elinduljon egyszerre, a `multicore` direktívát kell megadni. Meg kell előznie egy `menuentry`
sornak, és az [smp](../../src/plugins/smp.c) beépülőnek is telepítve kell lennie, hogy működjön.

```
multicore
```

Hibakeresés
-----------

Ha bármilyen probléma merülne fel, akkor csak az `easyboot -vv` kapcsolókkal kell futtatni. Ez egyben ellenőriz is és részletes
kimenetet ír ki a létrehozáskor. Egyébként pedig hozzá kell adni a `verbose 3` sort az `easyboot/menu.cfg`-hoz, hogy megjelenjenek
az induláskori üzenetek.

Ha a bal felső sarokban piros háttérrel az látszik, hogy `PMBR-ERR`, akkor az azt jelenti, hogy a processzorod túl régi, nem
támogatja a 64-bites long módot vagy az indulószektor nem találta a betöltőt. Ez csakis BIOS gépeken fordulhat elő, UEFI-n és
RaspberryPi-n soha.

| Üzenet                              | Leírás                                                                            |
|-------------------------------------|-----------------------------------------------------------------------------------|
| `Booting [X]...`                    | jelzi, hogy melyik boot opció (menuentry sorszám) lett kiválasztva                |
| `Loading 'X' (Y bytes)...`          | az X nevű és Y hosszú fájlt készül betölteni                                      |
| `Parsing kernel...`                 | a kernelt megtalálta, most meghatározza a formátumát                              |
| `Starting X boot...`                | azt jelenti, hogy az X operációs rendszer betöltőjét találta meg                  |
| `Starting X kernel...`              | azt jelenti, hogy az X operációs rendszer kernelét találta meg                    |
| `Transfering X control to Y`        | jelzi, hogy átadja a vezérlést az X módú belépési pontra az Y címen               |

Ha egy hiba azután jönne elő, hogy ez a legutolsó üzenet is megjelent, akkor a probléma az operációs rendszer indítási folyamatában
keletkezett, és nem pedig az **Easyboot** betöltőben, ezért az adott operációs rendszer dokumentációjában található meg rá a válasz.
Egyébként meg bátran nyiss egy gitlab hibajegyet a [hibabejelentő](https://gitlab.com/bztsrc/easyboot/-/issues)ben.

### Multiboot1

Szükséges beépülők: [grubmb1](../../src/plugins/grubmb1.c)

### Multiboot2

Ez a legrugalmasabb, többféle variáció is támogatott a beépülők által:

- ELF64 vagy PE32+ egyszerűsített Multiboot2-al: nincs szükség beépülőre
- ELF32 egyszerűsített Multiboot2-al és 32-bites belépési ponttal: [elf32](../../src/plugins/elf32.c)
- a.out (struct exec) egyszerűsített Multiboot2-al és 32-bites belépési ponttal: [aout](../../src/plugins/aout.c)
- GRUB-kompatíbilis Multiboot2, 32-bites belépési ponttal: [grubmb2](../../src/plugins/grubmb2.c)

Fontos a kölönbség: az [egyszerűsített Multiboot2](ABI.md) esetén nincsenek beágyazott tegek, felső címtér is támogatva van
akárcsak a tisztán 64-bites belépési pont, a paraméterek pedig mind Multiboot2, SysV és fastcall ABI szerint is átadódnak.

Ezzel szemben a [GRUB-kompatíbilis Multiboot2](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) esetén kellenek
a beágyazott tegek, nem támogatja a felső címteret sem a 64-bites kerneleket, a belépési pont csakis 32-bites lehet, a paraméterek
pedig csak az `eax`, `ebx` regiszterekben adódnak át.

### Windows

Nincs szükség beépülőre, de be kell konfigurálni a [SecureBoot](secureboot.md)-ot.

```
menuentry win {
  kernel EFI/Microsoft/BOOT/BOOTMGRW.EFI
}
```

### Linux

Szükséges beépülők: [linux](../../src/plugins/linux.c), [ext234](../../src/plugins/ext234.c)

Ha a kernel nem a boot partíción található, akkor a parancssorban is megadható a root partíció.

```
menuentry linux {
  kernel vmlinuz-linux root=PARTUUID=01020304-0506-0708-0a0b0c0d0e0f1011
}
```

### OpenBSD

Szükséges beépülők: [obsdboot](../../src/plugins/obsdboot.c), [ufs44](../../src/plugins/ufs44.c)

```
menuentry openbsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot
}
```

VIGYÁZAT! Nem használható az [elf32](../../src/plugins/elf32.c) beépülő, ha OpenBSD-t is indítunk! A `boot`-ja valótlanul állítja
magáról, hogy egy ELF 32-bites SysV ABI belépési ponttal, holott valójában 16-bites valós módú belépési pontja van.

### FreeBSD

Szükséges beépülők: [fbsdboot](../../src/plugins/fbsdboot.c), [ufs2](../../src/plugins/ufs2.c)

A hagyományos BIOS rendszereken a `boot` betöltőt kell megadni.

```
menuentry freebsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot/boot
}
```

UEFI gépeken pedig a `loader.efi` betöltőt a boot partíción (ehhez nincs szükség beépülőkre).

```
menuentry freebsd {
  kernel boot/loader.efi
}
```

Ha a root fájlrendszer ZFS, akkor ezt az egy fájlt (`boot` BIOS-on, `loader.efi` UEFI-n) be kell másolni az `(indir)`-be és
NEM kell megadni root partíciót.

### FreeDOS

Szükséges beépülők: [fdos](../../src/plugins/fdos.c)

A FreeDOS fájljait be kell másolni az `(indir)`-be (a FreeDOS a boot partíciót használja root partícióként).

```
menuentry freedos {
  kernel KERNEL.SYS
}
```

Ha a bootolás a jogi figyelmeztetés és az `- InitDisk` kiírása után megáll, akkor a FreeDOS kernel FAT32 támogatás nélkül lett
fordítva. Egy másik kernelt kell letölteni, olyant, aminek a nevében szerepel az `f32`.

### ReactOS

Szükséges beépülők: [reactos](../../src/plugins/reactos.c)

```
menuentry reactos {
  kernel FREELDR.SYS
}
```

### MenuetOS

Szükséges beépülők: [menuet](../../src/plugins/menuet.c)

```
menuentry menuetos {
  kernel KERNEL.MNT
  module CONFIG.MNT
  module RAMDISK.MNT
}
```

Az automatikus konfiguráció kikapcsolható a `noauto` parancssori kapcsolóval.

### KolibriOS

Szükséges beépülők: [kolibri](../../src/plugins/kolibri.c)

```
menuentry kolibrios {
  kernel KERNEL.MNT syspath=/rd/1/ launcher_start=1
  module KOLIBRI.IMG
  module DEVICES.DAT
}
```

A beépülő működik UEFI gépeken is, de használható akár az `uefi4kos.efi` is a boot partíción (ehhez nincs szükség beépülőkre).

### SerenityOS

Szükséges beépülők: [grubmb1](../../src/plugins/grubmb1.c)

```
menuentry serenityos {
  kernel boot/Prekernel
  module boot/Kernel
}
```

### Haiku

Szükséges beépülők: [grubmb1](../../src/plugins/grubmb1.c), [befs](../../src/plugins/befs.c)

```
menuentry haiku {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel system/packages/haiku_loader-r1~beta4_hrev56578_59-1-x86_64.hpkg
}
```

UEFI gépeken pedig a `haiku_loader.efi` betöltőt kell megadni a boot partíción (ehhez nincs szükség beépülőkre).
