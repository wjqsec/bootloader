Ihren Kernel debuggen
=====================

Wenn Ihr Kernel kaputt geht, hilft Ihnen **Easyboot** standardmäßig dabei, Details darüber anzuzeigen, was und wo schief gelaufen
ist.

![](../exc.png)

Dies reicht aus, um Ihnen einen Hinweis zu geben, ermöglicht Ihnen jedoch keine interaktive Untersuchung. Dafür benötigen Sie einen
Debugger.

Mini Debugger
-------------

Vorteile:

- Einfach einzurichten
- Einfach zu verwenden
- Funktioniert sowohl auf realer Hardware als auch auf virtuellen Maschinen

Nachteile:

- Wie der Name schon sagt, minimaler Funktionsumfang

Um den [Mini Debugger](https://gitlab.com/bztsrc/minidbg) zu aktivieren, installieren Sie einfach das **Easyboot**
[Plugin](plugins.md), indem Sie die entsprechende Datei `minidbg_(arch).plg` kopieren auf Ihre Boot-Partition. Das ist es. Es
bietet eine Video-Terminalschnittstelle über eine serielle Leitung (mit 115200 Baud, 8 Datenbits, 1 Stoppbit, keine Parität).

Schließen Sie auf echter Hardware ein VT100- oder VT220-Terminal oder eine andere Maschine über ein serielles Kabel an, auf dem
**PuTTY** (Windows) oder **minicom** (Linux) ausgeführt wird.

Für virtuelle Maschinen führen Sie qemu einfach mit den `-serial stdio`-Argumenten aus, und Sie können den Debugger über dasselbe
Fenster steuern, das Sie zum Ausführen von qemu verwendet haben. Zum Beispiel:

```
qemu-system-x86_64 -serial stdio -hda diskette.img
```

Jedes Mal, wenn Ihr Kernel ausfällt, erhalten Sie sofort eine Debugger-Eingabeaufforderung und können die Situation untersuchen.
Geben Sie in der Debugger-Eingabeaufforderung `?` oder `h` ein, um Hilfe zu erhalten. Um den Debugger explizit aufzurufen, fügen
Sie `int 3` (ein 0xCC-Byte) in Ihren Code ein. Ich schlage vor, die folgende Definition zu Ihrem Kernel hinzuzufügen:

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

Vorteile:

- Keine Einrichtung, sofort einsatzbereit
- Zeigt den internen Zustand der Maschine an, was kein anderer Debugger tun kann

Nachteile:

- Wirklich schwer zu bedienen
- Funktioniert nur mit virtuellen Maschinen

Wenn Ihre virtuelle Maschine ausgeführt wird, wählen Sie im Menü `View` > `compatmonitor0` oder klicken Sie auf das Fenster, damit
es den Fokus erhält, und drücken Sie <kbd>Strg</kbd>+<kbd>Alt</kbd>+<kbd>2</kbd> (um den Fokus freizugeben, drücken Sie
<kbd>Strg</kbd>+<kbd>Alt</kbd>+<kbd>G</kbd>).

GDB
---

Vorteile:

- Voll ausgestatteter Debugger
- Bietet alle Funktionen, die Sie sich vorstellen können

Nachteile:

- Schwierig einzurichten
- Funktioniert nur mit virtuellen Maschinen

Bevor Sie etwas unternehmen, ändern Sie zunächst Ihre Build-Umgebung, um zwei Kerneldateien zu generieren. Eines mit Debug-Symbolen
und eines ohne. Dies ist wichtig, da Debugsymbole leicht viel Platz beanspruchen können, wahrscheinlich mehrere Megabyte.
Kompilieren Sie zunächst Ihren Kernel mit der Flagge `-g`. Nachdem die Kompilierung abgeschlossen ist, kopieren Sie Ihren Kernel
`cp mykernel.elf mykernel_sym.elf` und entfernen Sie die Debug-Informationen mit `strip mykernel.elf`. Anschließend booten Sie
`mykernel.elf` und geben `mykernel_sym.elf` an gdb weiter.

Als nächstes erstellen Sie ein GDB-Skript mit dem Namen `gdb.rc`. Verwenden Sie Folgendes als Vorlage:

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

Dies verbindet gdb mit der virtuellen Maschine, teilt ihr den Typ der Maschine mit, lädt die Debug-Informationen, konfiguriert das
Layout, setzt einen Haltepunkt direkt am Eintrittspunkt Ihres Kernels und startet schließlich die virtuelle Maschine.

Nachdem diese Einrichtung abgeschlossen ist, können Sie mit dem Debuggen beginnen. Eine Debug-Sitzung läuft so ab: Starten Sie qemu
in einem Terminal mit den Flags `-s -S`. Es wird hängen bleiben. Zum Beispiel:

```
qemu-system-x86_64 -s -S -hda diskette.img
```

In einem anderen Terminal, starten Sie dann gdb mit dem Skript, das wir zuvor erstellt haben:

```
gdb -w -x gdb.rc
```

Dies sollte alles in einem Fenster anzeigen, etwa so:

![](../gdb.png)

Wenn Sie es bis hierher geschafft haben, dann herzlichen Glückwunsch, Sie können mit der eigentlichen Arbeit mit Ihrem Kernel
beginnen!
