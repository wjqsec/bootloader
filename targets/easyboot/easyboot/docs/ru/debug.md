Отладка вашего ядра
===================

Если ваше ядро выходит из строя, **Easyboot** по умолчанию окажет вам услугу и отобразит подробную информацию о том, что и где
пошло не так.

![](../exc.png)

Этого достаточно, чтобы дать вам подсказку, но это не позволяет вам проводить интерактивное расследование. Для этого вам понадобится
отладчик.

Mini Debugger
-------------

Преимущества:

- легко настроить
- легко использовать
- работает как на реальном оборудовании, так и на виртуальных машинах

Недостатки:

- как следует из названия, минимальный набор функций

Чтобы включить [Mini Debugger](https://gitlab.com/bztsrc/minidbg), просто установите **Easyboot** [плагин](plugins.md), скопировав
соответствующий файл `minidbg_(arch).plg` в ваш загрузочный раздел. Вот и все. Он обеспечивает интерфейс видеотерминала по
последовательной линии (115200 бод, 8 бит данных, 1 стоповый бит, без контроля четности).

На реальном оборудовании подключите терминал VT100 или VT220 или другую машину через последовательный кабель, работающий под
управлением **PuTTY** (Windows) или **minicom** (Linux).

Для виртуальных машин просто запустите qemu с аргументами `-serial stdio`, и вы сможете управлять отладчиком из того же окна,
которое вы использовали для запуска qemu. Например:

```
qemu-system-x86_64 -serial stdio -hda диск.img
```

Каждый раз, когда ваше ядро выходит из строя, вы мгновенно получаете приглашение отладчика и сможете изучить ситуацию. В командной
строке отладчика введите `?` или `h`, чтобы получить справку. Чтобы явно вызвать отладчик, вставьте в свой код int 3 (байт 0xCC). Я
предлагаю добавить в ваше ядро следующее определение:

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

Преимущества:

- не требует настройки, работает сразу из коробки
- отображает внутреннее состояние машины, чего не может сделать ни один другой отладчик

Недостатки:

- действительно сложно использовать
- работает только с виртуальными машинами

Когда ваша виртуальная машина запущена, выберите в меню `View` > `compatmonitor0` или щелкните окно, чтобы оно захватило фокус, и
нажмите <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+. <kbd>2</kbd> (чтобы снять фокус, нажмите <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>G</kbd>).

GDB
---

Преимущества:

- полнофункциональный отладчик
- предоставляет все функции, о которых вы только можете подумать

Недостатки:

- непросто настроить
- работает только с виртуальными машинами

Прежде чем что-либо делать, сначала измените среду сборки, чтобы сгенерировать два файла ядра. Один с символами отладки, другой без.
Это важно, поскольку символы отладки могут легко занимать много места, возможно, несколько мегабайт. Сначала скомпилируйте ядро с
флагом `-g`. Затем, после завершения компиляции, скопируйте ядро `cp mykernel.elf mykernel_sym.elf` и удалите отладочную информацию
с помощью `strip mykernel.elf`. В дальнейшем вы собираетесь загрузить `mykernel.elf` и передать `mykernel_sym.elf` в gdb.

Затем создайте сценарий GDB с именем `gdb.rc`. В качестве шаблона используйте следующее:

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

Это подключает GDB к виртуальной машине, сообщает ему тип машины, загружает отладочную информацию, настраивает макет, устанавливает
точку останова прямо в точке входа вашего ядра и, наконец, запускает виртуальную машину.

После завершения настройки вы можете начать отладку. Один сеанс отладки выглядит следующим образом: в одном терминале запустите qemu
с флагами `-s -S`. Оно будет висеть. Например:

```
qemu-system-x86_64 -s -S -hda диск.img
```

Затем в другом терминале запустите gdb со сценарием, который мы создали ранее:

```
gdb -w -x gdb.rc
```

Это должно отображать все в одном окне следующим образом:

![](../gdb.png)

Если вы зашли так далеко, то поздравляем, вы можете начать настоящую работу с вашим ядром!
