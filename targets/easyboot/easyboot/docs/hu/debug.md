Kernel Hibakeresés
==================

Ha egy kernel megmakacsolja magát, akkor alapból az **Easyboot** megjelenít részleteket arról, hogy mi és hol ment félre.

![](../exc.png)

Ez elég ahhoz, hogy tippet adjon, de nem teszi lehetővé a helyzet interkatív kivizsgálását. Ahhoz debuggerre van szükség.

Mini Debugger
-------------

Előnyei:

- könnyű beállítani
- könnyű használni
- egyaránt használható igazi vason és virtuális gépen is

Hátrányai:

- amint a neve sugallja, csak minimális funkcionalitással bír

A [Mini Debugger](https://gitlab.com/bztsrc/minidbg) engedélyézéséhez csak telepíteni kell az **Easyboot** [beépülőt](plugins.md),
a megfelelő `minidbg_(arch).plg` fájl felmásolásával a boot partícióra, ennyi. Ez egy videó terminál interfészt nyújt a soros
vonalon (115200 baud, 8 adat bit, 1 stop bit, nincs paritás bit).

Igazi vason csatlakoztatni kell egy VT100-as vagy VT220-as terminált, vagy pedig egy másik gépet soros kábellel, amin **PuTTY**
(Windows) vagy **minicom** (Linux) fut.

Virtuális gép esetén a qemu-t `-serial stdio` kapcsolóval kell indítani, és a debugger abból az ablakból lesz elérhető, ahonnan a
qemu indult. Például:

```
qemu-system-x86_64 -serial stdio -hda lemez.img
```

Bármikor, amikor a kernel elszáll, egyből megjelenik egy interaktív debugger parancssor, hogy a helyzetet kielemezhessük. A debugger
parancssorában a `?` vagy `h` utasításra megjeleníti a súgót. A debugger direktben történő meghívásához a kernel adott pontjára egy
`int 3` utasítást (egy 0xCC bájtot) kell elhelyezni. Ehhez javaslom a következő define hozzáadását a kernelhez:

```c
/* x86 */
#define breakpoint __asm__ __volatile__("int $3")
/* ARM */
#define breakpoint __asm__ __volatile__("brk #0")
```

```
Mini debugger by bzt
Exception 03: Breakpoint instruction, code 0
rax: 0000000000000000  rbx: 00000000000206C0  rcx: 000000000000270F
rdx: 00000000000003F8  rsi: 00000000000001B0  rdi: 0000000000102336
rsp: 000000000008FFB8  rbp: 000000000008FFF8   r8: 0000000000000004
 r9: 0000000000000002  r10: 0000000000000000  r11: 0000000000000003
r12: 0000000000000000  r13: 0000000000000000  r14: 0000000000000000
r15: 0000000000000000
> ?
Mini debugger commands:
  ?/h		this help
  c		continue execution
  n		move to the next instruction
  r		dump registers
  x [os [oe]]	examine memory from offset start (os) to offset end (oe)
  i [os [oe]]	disassemble instructions from offset start to offset end
> i pc-1 pc+4
00101601: CC                             int	3
00101602: FA                             cli
00101603: F4                             hlt
00101604: EB FC                          jmp	101602h
00101606: 90                              1 x nop
>
```

Qemu Debugger
-------------

Előnyei:

- nem kell semmit sem beállítani, egyből használható
- a gép belső állapotát is megmutatja, amire semelyik másik debugger sem képes

Hátrányai:

- nagyon nehéz használni
- csak virtuális gépen működik

Amikor a virtuális gép fut, válasszuk ki a `View` > `compatmonitor0` menüpontot, vagy kattintsunk az ablakra, hogy magához ragadja
a fókuszt és üssük le a <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>2</kbd> billentyűkombinációt (a fókusz elengedése
<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>G</kbd>).

GDB
---

Előnyei:

- teljes értékű debugger
- minden olyan funkciót biztosít, amit csak el tudsz képzelni

Hátrányai:

- trükkös a beállítása
- csak virtuális gépen működik

Mielőtt bármi mást tennénk, módosítani kell a fordítási környezetet, hogy két kernel fájlt generáljon. Egyet debug szimbólumokkal,
egy másikat pedig annélkül. Ez azért fontos, mert a debug szimbólumok rengeteg helyet foglalnak, könnyedén több megányi lehet.
Először is, a kernelt a `-g` kapcsolóval fordítsuk. A fordítás végeztével másoljuk le `cp mykernel.elf mykernel_sym.elf`, majd
távolítsuk el a debug infókat a `strip mykernel.elf` paranccsal. Innentől a `mykernel.elf` lesz bootolva, a gdb-nek pedig a
`mykernel_sym.elf` fájl lesz megadva.

Következő lépésként hozzunk létre egy `gdb.rc` nevű gdb szkriptfájlt. A következő minta használható sablonnak ehhez:

```
target remote localhost:1234
set architecture i386:x86-64
symbol-file mykernel_sym.elf
layout split
layout src
layout regs
break *_start
continue
```

Ez csatlakoztatja a gdb-t a virtuális géphez, beállítja a gép típusát, betölti a debug információkat, bekonfigurálja a kinézetet,
beállít egy töréspontot mindjárt a kernel belépési pontjára, majd végül elindítja a virtuális gépet.

Miután ezzeket a beállításokat egyszer megtettük, lehet debuggolni. Egy menet a következőképp néz ki: az egyik terminálban el kell
indítani a qemu-t a `-s -S` kapcsolókkal. Úgy fog tűnni, hogy lefagyott. Például:

```
qemu-system-x86_64 -s -S -hda lemez.img
```

Ezután egy másik terminálban indítsuk el a gdb-t azzal a szkripttel, amit fentebb létrehoztunk:

```
gdb -w -x gdb.rc
```

Ez egy képernyőn kell, hogy megjelenítsen mindenfélét, valahogy így:

![](../gdb.png)

Ha idáig sikerült eljutni, akkor gratulálunk, kezdődhet az igazi munka a kernellel!
