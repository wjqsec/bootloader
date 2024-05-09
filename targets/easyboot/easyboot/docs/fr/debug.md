Débogage de votre noyau
=======================

Si votre noyau fait des ravages, **Easyboot** vous rendra service par défaut en affichant des détails sur ce qui n'a pas fonctionné
et où.

![](../exc.png)

Cela suffit pour vous donner un indice, mais cela ne vous permet pas d'enquêter de manière interactive. Pour cela, vous aurez besoin
d'un débogueur.

Mini Debugger
-------------

Avantages:

- facile à mettre en place
- facile à utiliser
- fonctionne également sur le matériel réel et les machines virtuelles

Désavantages:

- comme son nom l'indique, ensemble minimal de fonctionnalités

Pour activer le [Mini Debugger](https://gitlab.com/bztsrc/minidbg), installez simplement le **Easyboot** [plugin](plugins.md) en
copiant le fichier `minidbg_(arch).plg` approprié sur votre partition de démarrage. C'est ça. Il fournit une interface de terminal
vidéo sur ligne série (avec 115 200 bauds, 8 bits de données, 1 bit d'arrêt, pas de parité).

Sur du matériel réel, connectez un terminal VT100 ou VT220, ou une autre machine via un câble série exécutant **PuTTY** (Windows)
ou **minicom** (Linux).

Pour les machines virtuelles, exécutez simplement qemu avec les arguments `-serial stdio`, et vous pourrez contrôler le débogueur
depuis la même fenêtre que celle que vous avez utilisée pour exécuter qemu. Par exemple:

```
qemu-system-x86_64 -serial stdio -hda disque.img
```

Chaque fois que votre noyau explose, vous recevrez instantanément une invite du débogueur et vous pourrez examiner la situation.
Dans l'invite du débogueur, tapez `?` ou `h` pour obtenir de l'aide. Pour appeler explicitement le débogueur, insérez `int 3` (un
octet 0xCC) dans votre code. Je suggère d'ajouter la définition suivante à votre noyau:

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

Avantages:

- aucune configuration, fonctionne immédiatement
- affiche l'état interne de la machine, ce qu'aucun autre débogueur ne peut faire

Désavantages:

- vraiment difficile à utiliser
- ne fonctionne qu'avec les machines virtuelles

Lorsque votre machine virtuelle est en cours d'exécution, dans le menu, sélectionnez `View` > `compatmonitor0`, ou cliquez sur la
fenêtre pour qu'elle prenne le focus et appuyez sur <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+ <kbd>2</kbd> (pour libérer le focus, appuyez
sur <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>G</kbd>).

GDB
---

Avantages:

- débogueur complet
- fournit toutes les fonctionnalités auxquelles vous pouvez penser

Désavantages:

- délicat à mettre en place
- ne fonctionne qu'avec les machines virtuelles

Avant de faire quoi que ce soit, modifiez d'abord votre environnement de build pour générer deux fichiers noyau. Un avec des
symboles de débogage et un autre sans. Ceci est important car les symboles de débogage peuvent facilement occuper beaucoup d’espace,
probablement plusieurs mégaoctets. Tout d'abord, compilez votre noyau avec l'indicateur `-g`. Ensuite, une fois la compilation
terminée, copiez votre noyau `cp mykernel.elf mykernel_sym.elf` et supprimez les informations de débogage avec `strip mykernel.elf`.
Ci-après, vous allez démarrer `mykernel.elf` et vous fournirez `mykernel_sym.elf` à gdb.

Ensuite, créez un script gdb nommé `gdb.rc`. Utilisez ce qui suit comme modèle:

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

Cela connecte gdb à la machine virtuelle, lui indique le type de la machine, charge les informations de débogage, configure la
disposition, définit un point d'arrêt juste au point d'entrée de votre noyau et démarre enfin la machine virtuelle.

Une fois cette configuration terminée, vous pouvez commencer le débogage. Une session de débogage se déroule comme ceci: dans un
terminal, démarrez qemu avec les indicateurs `-s -S`. Il va pendre. Par exemple:

```
qemu-system-x86_64 -s -S -hda disque.img
```

Puis dans un autre terminal, démarrez gdb avec le script que nous avons créé précédemment:

```
gdb -w -x gdb.rc
```

Cela devrait tout afficher dans une seule fenêtre comme ceci:

![](../gdb.png)

Si vous êtes arrivé jusqu'ici, félicitations, vous pouvez commencer le vrai travail avec votre noyau!
