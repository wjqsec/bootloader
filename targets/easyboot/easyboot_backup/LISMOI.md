Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) est un *gestionnaire* de démarrage tout-en-un et un créateur d'image disque amorçable
qui peut charger divers noyaux de système d'exploitation et noyaux compatibles Multiboot2 dans différents formats binaires.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

REMARQUE: Si vous recherchez un *loader* de démarrage, qui charge un seul noyau uniquement, jetez un œil au petit frère de
**Easyboot**, [Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Reconnaissance
--------------

Ce projet n'aurait pas été possible sans le soutien de la [Free Software Foundation Hungary](https://fsf.hu/nevjegy).
Le projet a été créé dans le cadre de l'appel d'offres pour les logiciels libres 2023 de FSF.hu.

Avantages par rapport à GRUB
----------------------------

- programme d'installation multilingue sans dépendance et facile à utiliser
- pas gonflé, il ne représente qu'environ 0,5 % de la taille de GRUB
- facile à configurer, avec vérificateur de syntaxe
- Corrigez facilement les tables ACPI avec le DSDT fourni par l'utilisateur
- affiche les détails de l'erreur si le noyau s'éteint à un stade précoce
- Multiboot2: point d'entrée 64 bits propre (pas besoin d'intégrer de balises ni de code trampoline 32 bits dans le noyau)
- Multiboot2: prise en charge des noyaux supérieurs
- Multiboot2: carte mémoire indépendant du firmware et cohérent sur toutes les plateformes
- Multiboot2: framebuffer indépendant du firmware et cohérent sur toutes les plateformes
- Multiboot2: les informations EDID du moniteur sont également fournies
- Multiboot2: si on le souhaite, démarre le noyau sur tous les cœurs du processeur (support SMP)
- il existe également un plugin pour simuler le démarrage de GRUB avec tous ses pièges et bugs.

Noyaux supportés: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/fr/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/fr/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

Systèmes de fichiers supportés: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(Lorsque le noyau est situé sur la partition de démarrage, n'importe quel système de fichiers peut être utilisé pour root: ZFS,
btrfs, etc.)

Raisonnement
------------

J'ai créé un chargeur de démarrage simple à utiliser et les utilisateurs demandaient de plus en plus de fonctionnalités. Je voulais
garder ce chargeur de démarrage comme simple que possible, mais FSF.hu offrait un support, j'ai donc décidé de le créer et d'ajouter
toutes les fonctionnalités demandées dans ce gestionnaire de démarrage plutôt.

Il s'agit également d'un outil [sans succion](https://suckless.org) comme Simpleboot, qui n'a aucune dépendance et est extrêmement
simple à utiliser:

1. créez un répertoire et mettez-y vos fichiers de démarrage, entre autres la configuration de vos menus et les plugins optionnels
2. exécutez la commande `easyboot (répertoire source) (fichier image de sortie)`
3. et... c'est tout... il ne reste plus rien à faire ! L'image *Just Works (TM)*, elle fera démarrer vos noyaux!

Vous pouvez installer le gestionnaire de démarrage et rendre un périphérique ou une image existant amorçable; ou vous pouvez créer
à nouveau une image de démarrage. Tu peux démarrez cette image dans une VM, ou vous pouvez l'écrire avec `dd` ou
[USBImager](https://bztsrc.gitlab.io/usbimager/) sur un stockage et démarrer cela aussi sur une vraie machine.

La simplicité est la sophistication ultime!

Installation
------------

Téléchargez simplement le binaire pour votre système d'exploitation. Ce sont des exécutables portables, ils ne nécessitent aucune
installation et ils n'en ont pas besoin bibliothèques/DLL partagées.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

De plus, vous pouvez trouver diverses solutions de packaging dans le répertoire [distrib](distrib) (pour Debian, Ubuntu, RaspiOS,
Gentoo, Arch).

Lors de la création d'une image (selon votre configuration), vous aurez peut-être également besoin de plugins dans votre
`(répertoire source)`. Toi Vous pouvez les trouver dans le répertoire [src/plugins](src/plugins) en utilisant l'extension `plg`.
Lorsqu'ils sont installés à partir d'un package, ceux-ci seront placé sous `/usr/share/easyboot`.

Documentation
-------------

La [documentation](docs/fr) détaillée sur la façon d'utiliser le créateur de disque de démarrage et sur la façon dont un noyau
est démarré peut être trouvée dans la documentation annuaire.

Exemple de noyau
----------------

Si vous souhaitez écrire un noyau qui peut être chargé sans aucun plugin en utilisant le protocole [Multiboot2 simplifié](docs/fr/ABI.md)
de **Easyboot**, jetez un œil à [Simpleboot Exemple de noyau](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example). Les deux
chargeurs utilisent le même protocole de démarrage, ce noyau fonctionne avec **Easyboot** tel quel. Vous verrez qu'aucun Assembly
ni balise intégrée n'est requis, sinon le code source est à 99,9 % identique à l'exemple de la spécification Multiboot2 (la seule
différence est qu'il s'imprime sur la console série et non sur l'écran du télétype VGA, car cela ce dernier n'existe pas sur les
machines UEFI et RaspberryPi).

Compilation
-----------

GNU/make est nécessaire pour l'orchestration (bien qu'il s'agisse littéralement de `cc easyboot.c -o easyboot`). La chaîne d'outils
n'a pas d'importance, n'importe quel compilateur ANSI C fera l'affaire, fonctionne également sur POSIX et WIN32 MINGW. Allez
simplement dans le répertoire [src](src) et exécutez `make`. C'est tout. Malgré sa petite taille, il est autonome et la compilation
n'a exactement aucune dépendance de bibliothèque. Il ne s'appelle pas **Easyboot** pour rien :-)

Pour recompiler les chargeurs, vous aurez besoin du [flatassembler](https://flatassembler.net) et de LLVM Clang et lld (gcc et GNU
ld ça ne marchera pas, j'en ai peur). Mais ne vous inquiétez pas, je les ai tous ajoutés à `src/data.h` sous forme de tableau
d'octets, vous n'avez donc pas besoin de compiler à moins que vous ne le vouliez vraiment (pour cela, supprimez simplement data.h
avant d'exécuter make).

Par contre pour compiler les plugins, vous devrez disposer d'un compilateur croisé, soit LLVM CLang, soit GNU gcc (x86_64-elf-gcc,
aarch64-elf-gcc). Le dépôt les inclut également sous forme de binaires. Pour faire la recompilation, supprimez simplement les
fichiers `src/plugins/*.plg` avant d'exécuter make.

Licence
-------

**Easyboot** est un logiciel gratuit et open source, sous licence selon les termes de la GPL version 3 ou (à votre avis) de toute
version ultérieure. Consultez le fichier [LICENSE](LICENSE) pour plus de détails.

bzt
