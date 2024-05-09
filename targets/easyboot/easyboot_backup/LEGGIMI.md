Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) è un *gestore* di avvio tutto-in-uno e un creatore di immagini disco avviabili che
può caricare vari kernel del sistema operativo e kernel compatibili con Multiboot2 in vari formati binari.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

NOTA: Se stai cercando un *loader* di avvio, che carica solo un singolo kernel, dai un'occhiata al fratellino di **Easyboot**,
[Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Riconoscenza
------------

Questo progetto non sarebbe stato possibile senza il supporto della [Free Software Foundation Hungary](https://fsf.hu/nevjegy).
Il progetto è stato creato nell'ambito del Bando per il Software Libero 2023 di FSF.hu.

Vantaggi rispetto a GRUB
------------------------

- programma di installazione multilingue, privo di dipendenze e facile da usare
- non gonfio, è solo circa lo 0,5% delle dimensioni di GRUB
- facile da configurare, con controllo della sintassi
- applicare facilmente patch alle tabelle ACPI con DSDT fornito dall'utente
- mostra i dettagli dell'errore se il kernel si spegne nella fase iniziale
- Multiboot2: punto di ingresso pulito a 64 bit (non è necessario incorporare tag né codice trampolino a 32 bit nel kernel)
- Multiboot2: supporto per kernel di metà superiore
- Multiboot2: mappa di memoria coerente e indipendente dal firmware su tutte le piattaforme
- Multiboot2: framebuffer coerente e indipendente dal firmware su tutte le piattaforme
- Multiboot2: vengono fornite anche le informazioni EDID del monitor
- Multiboot2: se desiderato, avvia il kernel su tutti i core del processore (supporto SMP)
- c'è anche un plugin per simulare l'avvio di GRUB con tutte le sue insidie e bug.

Kernel supportati: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/en/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/it/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

File system supportati: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(Quando il kernel si trova nella partizione di avvio, è possibile utilizzare qualsiasi file system per root: ZFS, btrfs, ecc.)

Motivazione
-----------

Ho creato un boot loader semplice da usare e gli utenti chiedevano sempre più funzionalità. Volevo mantenere quel boot loader come
semplice possibile, ma FSF.hu offriva supporto, quindi ho deciso di biforcarlo e aggiungere invece tutte le funzionalità richieste
in questo boot gestore.

Anche questo è uno strumento [senza succhiotto](https://suckless.org) come Simpleboot, non ha dipendenze ed estremamente facile da
usare:

1. crea una directory e inserisci i tuoi file di avvio, tra le altre cose la configurazione del menu e i plugin opzionali
2. eseguire il comando `easyboot (directory sorgente) (file immagine di output)`.
3. e... questo è tutto... non resta altro da fare! L'immagine *Just Works (TM)*, avvierà i tuoi kernel!

È possibile installare il boot manager e rendere avviabile un dispositivo o un'immagine esistente; oppure puoi creare di nuovo
un'immagine avviabile. Puoi avvia l'immagine in una VM oppure puoi scriverla con `dd` o [USBImager](https://bztsrc.gitlab.io/usbimager/)
in un dispositivo di archiviazione e avviare anche quello su una macchina reale.

La semplicità è la sofisticatezza finale!

Installazione
-------------

Basta scaricare il file binario per il tuo sistema operativo. Questi sono eseguibili portatili, non richiedono installazione e non
necessitano di librerie/DLL condivise.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

Inoltre puoi trovare varie soluzioni di pacchettizzazione nella directory [distrib](distrib) (per Debian, Ubuntu, RaspiOS, Gentoo,
Arch).

Quando crei un'immagine quindi (a seconda della tua configurazione) potresti anche aver bisogno di alcuni plugin nella tua
`(directory sorgente)`. Puoi trovarli nella directory [src/plugins](src/plugins) usando l'estensione `plg`. Se installati da un
pacchetto, questi verranno posizionati in `/usr/share/easyboot`.

Documentazione
--------------

La [documentazione](docs/en) dettagliata (in inglese) su come utilizzare il creatore del disco di avvio e su come viene avviato un
kernel può essere trovata nella directory docs.

Kernel di esempio
-----------------

Se vuoi scrivere un kernel che possa essere caricato senza alcun plugin utilizzando il protocollo [Multiboot2 semplificato](docs/en/ABI.md)
di **Easyboot**, dai un'occhiata a [Simpleboot Kernel di esempio](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example). Entrambi
i caricatori utilizzano lo stesso protocollo di avvio, il kernel funziona con **Easyboot** così com'è. Vedrai che non sono richiesti
Assembly né tag incorporati, altrimenti il codice sorgente è uguale al 99,9% all'esempio nelle specifiche Multiboot2 (l'unica
differenza è che viene stampato sulla console seriale e non sullo schermo della telescrivente VGA, perché quello quest'ultimo non
esiste sulle macchine UEFI e RaspberryPi).

Compilazione
------------

GNU/make necessario per l'orchestrazione (anche se letteralmente è solo `cc easyboot.c -o easyboot`). La toolchain non ha
importanza, va bene qualsiasi compilatore ANSI C, funziona anche su POSIX e WIN32 MINGW. Basta andare nella directory [src](src) ed
eseguire `make`. È tutto. Nonostante le sue piccole dimensioni, è autonomo e la compilazione ha esattamente zero dipendenze dalla
libreria. Non per niente si chiama **Easyboot** :-)

Per ricompilare i caricatori, avrai bisogno di [flatassembler](https://flatassembler.net) e LLVM Clang e lld (gcc e GNU ld
non funzionerà, temo). Ma non preoccuparti, li ho aggiunti tutti a `src/data.h` come array di byte, quindi non devi compilare
questi a meno che tu non lo voglia davvero (per questo, elimina semplicemente data.h prima di eseguire make).

D'altra parte per compilare i plugin, dovrai avere un cross-compiler, LLVM CLang o GNU gcc (x86_64-elf-gcc, aarch64-elf-gcc). Il
repository include anche questi come file binari. Per effettuare la ricompilazione, basta eliminare i file `src/plugins/*.plg`
prima di eseguire make.

Licenza
-------

**Easyboot** è un software gratuito e open source, concesso in licenza secondo i termini della GPL versione 3 o (a tuo parere)
qualsiasi versione successiva. Per i dettagli vedere il file [LICENSE](LICENSE).

bzt
