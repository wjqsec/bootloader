Easyboot Beépülők
=================

Alapból az [Easyboot](https://gitlab.com/bztsrc/easyboot) ELF és PE formátumú, Multiboot2 kompatíbilis kerneleket tölt be a boot
partícióról. Ha a kerneled más fájlformátumú, más boot protokollt használ, vagy nem a boot partíción helyezkedik el, akkor szükség
lesz beépülőkre a boot partíción. Ezek az [src/plugins](../../src/plugins) könyvtárban találhatók.

[[_TOC_]]

Telepítés
---------

A beépülök telepítéséhez simán csak be kell másolni őket az `(indir)` paraméterben megadott könyvtár `easyboot` nevű alkönyvtárába
a menu.cfg mellé.

Például:
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
$ easyboot bootpart lemez.img
```

Fordítás
--------

*Az már a legelején nyilvánvaló volt, hogy az ELF tökéletesen alkalmatlan a feladatra. Túl kövér és túl bonyolult. Ezért eredetileg
struct exec-et (a klasszikus UNIX a.out formátumát) akartam használni, de sajnálatos módon a modern eszközök nem képesek már olyant
létrehozni. Ezért egy saját formátum és saját linkelő mellett döntöttem.*

A beépülők forrása bármilyen szabványos ANSI C keresztfordítóval lefordítható ELF tárgyfájllá, de aztán a végső bináris
előállításhoz a [plgld](../../src/misc/plgld.c) linkelőt kell használni. Ez egy architektúra független kereszt-linkelő, nem számít
neki, hogy a beépülő milyen utasításkészletre lett lefordítva. A végső .plg fájl mérete általában töredéke csupán az .o ELF-nek,
amiből készült.

### Beépülő API

A beépülők C forrásának be kell húznia az `src/loader.h` fejlécfájlt, és tartalmaznia kell egy `EASY_PLUGIN` sort. Ennek egy
paramétere van, a beépülő típusa, amit az azonosító egyezés specifikáció követ. Ez utóbbit arra használja a betöltő, hogy eldöntse,
mikor kell az adott beépülőt használni.

Például:

```c
#include "../loader.h"

/* mágikus bájtok, amik a linux kernelt azonosítják */
EASY_PLUGIN(PLG_T_KERNEL) {
   /* offszet  hossz  egyezés típusa    mágikus bájtok */
    { 0x1fe,       2, PLG_M_CONST,    { 0xAA, 0x55, 0, 0 } },
    { 0x202,       4, PLG_M_CONST,    { 'H', 'd', 'r', 'S' } }
};

/* belépési pont, a prototípusát a beépülő típusa határozza meg */
PLG_API void _start(uint8_t *buf, uint64_t size);
{
    /* környezet beállítása linux kernel indításához */
}
```

A beépülők több változót és funkciót is használhatnak, ezek mind a fejlécfájlban vannak definiálva, és futásidőben linkelődnek.

```c
uint32_t verbose;
```
Bőbeszédűség. Egy beépülő csak akkor írhat a kimenetre bármit, ha ez nem nulla, kivéve persze a hibaüzeneteket. Egyébként minnél
nagyobb az értéke, annál több részletet kell kiírni.

```c
uint64_t file_size;
```
A megnyitott fájl teljes mérete (lásd `open` és `loadfile` alább).

```c
uint8_t *root_buf;
```
Amikor egy fájlrendszer beépülő inicializál, akkor a partíció első 128k-ját tartalmazza (remélhetőleg a szuperblokkal együtt).
Később a fájlrendszer beépülő újrahasznosíthatja ezt a 128k-s buffert bármilyen célra (FAT gyorsítótár, inode gyorsítótár stb).

```c
uint8_t *tags_buf;
```
A Multiboot2 tageket tartalmazza. A kernel beépülő ezt értelmezve átalakíthatja a boot menedzser által szolgáltatott adatokat
olyan formátumúra, amit a kernel vár. Ez a mutató a buffer elejére mutat.

```c
uint8_t *tags_ptr;
```
Ez a mutató a Multiboot2 teg buffer végére mutat. A teg beépülők új tegeket adhatnak hozzá és növelhetik ezt a mutatót.

```c
uint8_t *rsdp_ptr;
```
Az RSDP ACPI mutatót tartalmazza.

```c
uint8_t *dsdt_ptr;
```
A DSDT (vagy GUDT, FDT) hardver leíró bináris adathalmazra mutat.

```c
efi_system_table_t *ST;
```
UEFI gépeken az EFI rendszerleíró táblára mutat, egyébként `NULL`.

```c
void memset(void *dst, uint8_t c, uint32_t n);
void memcpy(void *dst, const void *src, uint32_t n);
int  memcmp(const void *s1, const void *s2, uint32_t n);
```
Kötelező memória funkciók (a C fordító hívásokat generálhat ezekre, akkor is, ha direktben nem lettek meghívva).

```c
void *alloc(uint32_t num);
```
Lefoglal `num` lapot (4k) a memóriából. A beépülők nem foglalhatnak sokat, minimális memóriahasználatra kell törekedniük.

```c
void free(void *buf, uint32_t num);
```
Felszabadít egy korábban lefoglalt, `num` lapnyi memóriaterületet.

```c
void printf(char *fmt, ...);
```
Formázott szöveget ír a boot konzolra.

```c
uint64_t pb_init(uint64_t size);
```
Elindítja a folyamatjelzőt, a `size` a teljes méret, amit ábrázol. Visszaadja, hogy egy pixel ebből hány bájtnak felel meg.

```c
void pb_draw(uint64_t curr);
```
Kirajzolja a folyamatjelzőt az aktuális értéknek megfelelően. A `curr` értékének 0 és a teljes méret közöttinek kell lennie.

```c
void pb_fini(void);
```
Bezárja a folyamatjelzőt, letörli a helyét a képernyőn.

```c
void loadsec(uint64_t sec, void *dst);
```
A fájlrendszer beépülők használják, szektort tölt be a lemezről a memóriába. A `sec` egy root partíció relatív szektorsorszám.

```c
void sethooks(void *o, void *r, void *c);
```
A fájlrendszer beépülők használják, beállítja a root partíció fájlrendszerének megfelelő open / read / close funkciók címeit.

```c
int open(char *fn);
```
Megnyit egy fájlt a root (vagy boot) partíción olvasása, sikeresség esetén 1-el tér vissza. Egyszerre csak egy fájl lehet megnyitva.
Ha előtte nem volt `sethooks` meghívva, akkor a boot partícióról olvas.

```c
uint64_t read(uint64_t offs, uint64_t size, void *buf);
```
Adatot olvas be a megnyitott fájl `offs` pozíciójáról a memóriába, a ténylegesen beolvasott bájtok számát adja vissza.

```c
void close(void);
```
Lezárja a megnyitott fájlt.

```c
uint8_t *loadfile(char *path);
```
Betölt teljes egészében egy fájlt a root (vagy boot) partícióról egy frissen allokált memóriabufferbe, és transzparensen ki is
csomagolja, ha talál hozzá beépülőt. A méretet a `file_size` változóban adja vissza.

```c
int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
```
Betölti a szegmenst a kernel bufferből. Ellenőrzi, hogy a `vaddr`-on a memória szabad-e, illetve leképezi, ha magas címterű.
Az `offs` fájlbeli offszet, azaz a kernel buffer első bájtjától számítódik. Ha a `memsz` nagyobb, mint a `filesz`, akkor a
különbségük nullákkal lesz feltöltve.

```c
void _start(void);
```
Fájlrendszer beépülők (`PLG_T_FS`) belépési pontja. Értelmeznie kell a szuperblokkot a `root_buf`-ban, majd meghívnia a
`sethooks`-t. Hiba esetén egyből vissza kell térnie, a hookok beállítása nélkül.

```c
void _start(uint8_t *buf, uint64_t size);
```
Kernel beépülők (`PLG_T_KERNEL`) belépési pontja. A kernelt tartalmazó memóriabuffert kapja paraméterül, relokálnia kell a
szegmenseit, beállítani a környezetet és átadni a vezérlést. Ha nincs hiba, sosem tér vissza.

```c
uint8_t *_start(uint8_t *buf);
```
Kitömörítő beépülők (`PLG_T_DECOMP`) belépési pontja. A tömörített bufferrel hívódik (mérete `file_size`-ban), és egy újjonan
allokált buffert kell visszaadnia a kicsomagolt adatokkal (aminek a mérete ugyancsak a `file_size`-ba kerül). A régi buffert fel
kell szabadítania (vigyázat, a `file_size` bájtokban van, de a free() lapokban várja a méretet). Hiba esetén a `file_size` nem
változhat, és a módosítatlan eredeti buffert kell visszaadnia.

```c
void _start(void);
```
Teg beépülök (`PLG_T_TAG`) belépési pontja. Ezek új tegeket adhatnak a `tags_ptr` által mutatott memóriába, majd megnövelik ezt
a mutatót egy új, 8 bájtos határra igazított címre.

### Helyi függvények

A beépülőkben lehetnek helyi függvények, azonban egy CLang bug miatt ezeket *muszáj* `static`-nak deklarálni. (A bug az az, hogy
a CLang ezekhez a függvényekhez PLT rekordokat generál, hiába van megadva a "-fno-plt" parancssori kapcsoló. A `static` használata
kerülőmegoldás erre).

Alacsony szintű Fájl Formátum Specifikáció
------------------------------------------

Arra az esetre, ha valaki nem C-ben szeretne beépülőt készíteni (hanem mondjuk Assembly-ben), itt a fájl alacsony szintű leírása.

Nagyon hasonló az a.out formátumhoz. A fájl egy fix méretű fejlécből áll, amit változó hosszúságú szekciók követnek. A szekcióknak
nincs külön fejlécük, minden szekció adata közvetlenül követi az azt megelőző szekció adatát, adott sorrendben:

```
(fejléc)
(azonosító egyezés rekordok)
(relokációs rekordok)
(gépi kód)
(csak olvasható adat)
(inicializált írható-olvasható adat)
```

Az első igazi szekció, a gépi kód tartalmazza az igazítását. Az összes többinél az azt megelőző szekció méretéhez van hozzátoldva.

TIPP: ha egy beépülőt adsz meg egyedüli paraméterként a `plgld`-nek, akkor dumpolni fogja a fájlban lévő szekciókat egy olyan
formátumú kimenetben, mint a `readelf -a` vagy az `objdump -xd`.

### Fejléc

Minden szám kicsi-elöl (little-endian) formátumú, az architektúrától függetlenül.

| Offszet | Méret | Leírás                                                         |
|--------:|------:|----------------------------------------------------------------|
|       0 |     4 | mágikus `EPLG` bájtok                                          |
|       4 |     4 | a fájl teljes mérete                                           |
|       8 |     4 | szükséges memória mérete, amikor a fájl be van töltve          |
|      12 |     4 | kód szekció mérete                                             |
|      16 |     4 | csak olvasható adat szekció mérete                             |
|      20 |     4 | a beépülő belépési pontja                                      |
|      24 |     2 | architektúrakód (ugyanaz, mint az ELF-nél)                     |
|      26 |     2 | relokációs rekordok száma                                      |
|      28 |     1 | azonosító egyezés rekordok száma                               |
|      29 |     1 | legmagasabb hivatkozott GOT bejegyzés száma                    |
|      30 |     1 | fájl formátum revízió (jelenleg 0)                             |
|      31 |     1 | beépülő típusa (1=fájlrendszer, 2=kernel, 3=kitömörítő, 4=teg) |

Az architektúrakód megegyezik az ELF fejlécben használt architektúrakóddal, például 62 = x86_64, 183 = Aarch64 és 243 = RISC-V.

A beépülő típusa dönti el a belépési pont prototípusát, az ABI mindig SysV.

### Azonosító Egyezés Szekció

Ez a szekció a következő rekordból annyit tartalmaz, amennyi meg van adva a fejléc "azonosító egyezés rekordok száma" mezejében.

| Offszet | Méret | Leírás                                                   |
|--------:|------:|----------------------------------------------------------|
|       0 |     2 | offszet                                                  |
|       2 |     1 | méret                                                    |
|       3 |     1 | típus                                                    |
|       4 |     4 | mágikus bájtok, amikkel egyezni kell                     |

Először is, a kérdéses tárgy eleje betöltődik egy bufferbe. Az akkumulátor kezdésnél 0-ra állítódik. A rekordokban szereplő
offszetek mindig ehhez az akkumulátorhoz képest értendők, és a buffer annyadik bájtjára mutatnak.

A típus mező mondja meg, hogyan kell értelmezni az offszetet. Ha ez 1, akkor az offszet plusz az akkumulátor az érték. Ha 2,
akkor egy 8-bites bájt érték olvasódik be az offszetről, ha 3, akkor egy 16-bites szó, a 4 pedig 32-bites duplaszó értéket
jelent. 5 kiolvas egy 8-bites bájtot és hozzáadja az akkumulátort, 6 egy 16-bites szót olvas ki és ahhoz adja az akkumulátort,
végül a 7 hasonló, de 32-bites dupla szóval. A 8 a megadott mágikus bájtokat keresi az akkumulátoradik pozíciótól a buffer
végéig, offszet lépésben, és ha van találat, annak a pozícióját veszi értékként.

Ha a méret mező nulla, akkor az érték kerül az akkumulátorba. Ha a méret nem nulla, akkor annyi bájtot ellenőriz, hogy egyeznek-e
a megadott mágikus bájtokkal.

Például, hogy leellenőrizzük, egy PE futtatható NOP utasítással kezdődik-e:
```c
EASY_PLUGIN(PLG_T_KERNEL) {
   /* offszet  hossz  egyezés típusa    mágikus bájtok */
    { 0,           2, PLG_M_CONST,    { 'M', 'Z', 0, 0 } },      /* egyezés mágikus bájtokkal */
    { 60,          0, PLG_M_DWORD,    { 0, 0, 0, 0 } },          /* a PE fejléc címének betöltése az akkumulátorba */
    { 0,           4, PLG_M_CONST,    { 'P', 'E', 0, 0 } },      /* egyezés mágikus bájtokkal */
    { 40,          1, PLG_M_DWORD,    { 0x90, 0, 0, 0 } }        /* ellenőrzés, hogy a belépési ponton NOP utasítás van-e */
};
```

### Relokációk Szekció

Ez a szekció a következő rekordból annyit tartalmaz, amennyi meg van adva a fejléc "relokációs rekordok száma" mezejében.

| Offszet | Méret | Leírás                                                   |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | offszet                                                  |
|       4 |     4 | relokáció típusa                                         |

A típus bitjei:

| Kezdő   | Vége  | Leírás                                                   |
|--------:|------:|----------------------------------------------------------|
|       0 |     7 | szimbólum (0 - 255)                                      |
|       8 |     8 | PC relatív címzés                                        |
|       9 |     9 | GOT relatív indirekt címzés                              |
|      10 |    13 | értékmaszk index (0 - 15)                                |
|      14 |    19 | első cím bit (0 - 63)                                    |
|      20 |    25 | utolsó cím bit (0 - 63)                                  |
|      26 |    31 | negált cím jelzőbit pozíciója (0 - 63)                   |

Az offszet a fejléc mágikus bájtjaihoz képesti relatív cím, ez kijelöl egy integert a memóriában, ahol a relokációt el kell végezni.

A szimbólum mondja meg, hogy milyen címet kell használni. A 0 jelenti a beépülő betöltési címét (BASE), azaz hogy a fejléc mágikus
bájtja melyik címre került a memóriában. A többi érték egy külsős szimbólumcímet választ a GOT-ból, amik a betöltőben vagy egy
másik beépülőben lettek definiálva. Hogy melyik szám melyik szimbólumnak felel meg, ahhoz lásd a `plg_got` tömböt a plgld.c
forrásában. Ha a GOT relatív bit 1, akkor a szimbólum GOT-beli bejegyzésének a címét használja, nem pedig magának a szimbólumnak a
tényleges címét.

Ha a PC relatív bit 1, akkor ebből címből kivonódik maga az offszet (utasításszámláló-relatív címzési mód).

Az értékmaszk mondja meg, hogy az utasításban mely bitek tárolják a cím értékét. Ha ez 0, akkor architektúrától függetlenül a címet
egy-az-egyben kell a megadott offszetre írni. Az x86_64 esetén csak a 0-ás index megengedett. ARM Aarch64 esetén: 0 = egy-az-egyben,
1 = 0x07ffffe0 (shiftelés balra 5 bittel), 2 = 0x07fffc00 (shiftelés balra 10 bittel), 3 = 0x60ffffe0 (ADR/ADRP utasítások esetén
két bitcsoportra oszlik a shiftelt érték). Jövőbeli architektúrák további és más értékmaszkokat definiálhatnak.

Az értékmaszkot használva utolsó - első + 1 bitnyi érték kiolvasódik a memóriából és előjelesen kiegészítésre kerül. Ez hozzáadódik
a címhez (addend, illetve belsős hivatkozás esetén a belső szimbólum címét is magában foglalja).

Ha a negált cím jelzőbit meg van adva, és a cím pozitív, akkor ez a bit törölve lesz. Ha azonban negatív, akkor beállítódik, a cím
pedig negálódik.

Végül az első és utolsó bitek megadják, hogy a cím mely részét kell kiírni a kijelölt integerbe. Ez egyben megadja a relokáció
méretét is, az ezen kívül eső, valamint az értékmaszkban nem szereplő bitek nem fognak megváltozni.

### Kód Szekció

Ez a szekció a fejlécben megadott architektúrának megfelelő gépi kódú utasításokat tartalmaz, és az ugyancsak a fejlécben megadott
kódméret hosszú.

### Csak olvasható Adat Szekció

Ez egy opcionális szekció, lehet hiányzik. Ha van, akkor olyan hosszú, mint ami a fejléc csak olvasható adat szekció mérete
mezőjében meg lett adva. A konstans változók ebbe a szekcióba kerülnek.

### Inicializált Adat Szekció

Ez egy opcionális szekció, lehet hiányzik. Ha a kód szekció (vagy az opcionális csak olvasható adat szekció) után a fájl még
tartalmaz bájtokat, akkor azok mind az inicializált írható-olvasható adat szekciónak számítanak. Ha egy változó nem nulla
indulóértéket kap, akkor ebbe a szekcióba kerül.

### BSS Szekció

Ez egy opcionális szekció, lehet hiányzik. Ez a szekció a fájlban sosincs tárolva. Ha a fejlécben a memóriabeli méret nagyobb, mint
a fájlbeli méret, akkor a memóriában a különbségük nullákkal lesz feltöltve. Minden olyan változó, ami nem lett, vagy 0 értékkel
lett inicializálva, ide kerül.
