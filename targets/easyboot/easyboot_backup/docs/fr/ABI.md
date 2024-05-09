Écriture de noyaux compatibles Easyboot
=======================================

[Easyboot](https://gitlab.com/bztsrc/easyboot) prend en charge divers noyaux à l'aide de [plugins](plugins.md). Mais si aucun plugin
approprié n'est trouvé, il revient aux binaires ELF64 ou PE32+ avec une variante simplifiée (pas besoin d'intégrer quoi que ce soit)
du protocole Multiboot2.

Il s'agit du même protocole que celui utilisé par [Simpleboot](https://gitlab.com/bztsrc/simpleboot), tous les exemples de noyaux de
ce référentiel doivent également fonctionner avec **Easyboot**.

Vous pouvez utiliser l'en-tête multiboot2.h d'origine dans le dépôt de GRUB, ou le fichier d'en-tête
[simpleboot.h](https://gitlab.com/bztsrc/simpleboot/blob/main/simpleboot.h) C/C++ pour faciliter la tâche. utilisez des typedefs. Le
format binaire de bas niveau est le même, vous pouvez également utiliser n'importe quelle bibliothèque Multiboot2 existante, même
avec des langages non C, comme celui-ci [Rust](https://github.com/rust-osdev/multiboot2/tree/main/multiboot2/src) par exemple
(remarque: je ne suis en aucun cas affilié à ces développeurs, j'ai juste recherché "Rust Multiboot2" et c'était le premier résultat).

[[_TOC_]]

Séquence d'amorçage
-------------------

### Amorcer le chargeur

Sur les machines *BIOS*, le tout premier secteur du disque est chargé à 0:0x7C00 par le firmware et le contrôle lui est transmis.
Dans ce secteur, **Easyboot** a [boot_x86.asm](../../src/boot_x86.asm), qui est suffisamment intelligent pour localiser et charger
le chargeur de deuxième étape, ainsi que pour configurer le mode long pour celui-ci.

Sur les machines *UEFI*, le même fichier de 2ème étape, appelé `EFI/BOOT/BOOTX64.EFI`, est chargé directement par le firmware. La
source de ce chargeur se trouve dans [loader_x86.c](../../src/loader_x86.c). Voilà, **Easyboot** n'est ni GRUB ni syslinux, qui
nécessitent tous deux des dizaines et des dizaines de fichiers système sur le disque. Ici, plus aucun fichier n'est nécessaire,
juste celui-ci (les plugins sont facultatifs, aucun n'est nécessaire pour assurer la compatibilité Multiboot2).

Sur *Raspberry Pi*, le chargeur s'appelle `KERNEL8.IMG`, compilé à partir de [loader_rpi.c](../../src/loader_rpi.c).

### Le chargeur

Ce chargeur est très soigneusement écrit pour fonctionner sur plusieurs configurations. Il charge la table de partition GUID à
partir du disque et recherche une « partition système EFI ». Une fois trouvé, il recherche le fichier de configuration
`easyboot/menu.cfg` sur cette partition de démarrage. Une fois l'option de démarrage sélectionnée et le nom de fichier du noyau
connu, le chargeur le localise et le charge.

Il détecte ensuite automatiquement le format du noyau et est suffisamment intelligent pour interpréter les informations de section
et de segment indiquant où charger quoi (il effectue un mappage de mémoire à la demande chaque fois que nécessaire). Ensuite, il
met en place un environnement approprié en fonction du protocole de démarrage détecté (Multiboot2 / Linux / etc. mode protégé ou
long, arguments ABI etc.). Une fois que l'état de la machine est solide et bien défini, en tout dernier acte, le chargeur passe au
point d'entrée de votre noyau.

État de la machine
------------------

Tout ce qui est écrit dans la [spécification Multiboot2](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) (en
anglais) sur l'état de la machine est valable, à l'exception des registres à usage général. **Easyboot** transmet également deux
arguments au point d'entrée de votre noyau conformément à l'ABI SysV et à l'ABI fastcall de Microsoft. Le premier paramètre est
la magie, le second est une adresse mémoire physique, pointant vers une liste de tags Multiboot Information (abrégé en MBI ci-après,
voir ci-dessous).

Nous violons également un peu le protocole Multiboot2 pour gérer les noyaux supérieurs. Multiboot2 exige que la mémoire soit mappée
d'identité. Eh bien, sous **Easyboot**, cela n'est que partiellement vrai : nous garantissons uniquement que toute la RAM physique
est sûrement mappée comme prévu; cependant, certaines régions supérieures (en fonction des en-têtes de programme du noyau) peuvent
encore être disponibles. Cela ne brise pas les noyaux normaux compatibles Multiboot2, qui ne sont pas censés accéder à la mémoire
en dehors de la RAM physique disponible.

Votre noyau est chargé exactement de la même manière sur les systèmes BIOS et UEFI ainsi que sur RPi, les différences de firmware ne
sont que "le problème de quelqu'un d'autre". La seule chose que votre noyau verra est si le MBI contient ou non la balise de table
système EFI. Pour vous simplifier la vie, **Easyboot** ne génère pas non plus de balise de carte mémoire EFI (type 17), il fournit
uniquement la balise [Carte mémoire](#carte-mémoire) (type 6) sans discernement sur toutes les plateformes (sur les systèmes UEFI
également, là, la carte mémoire est simplement convertie pour vous, votre noyau ne doit donc gérer qu'un seul type de balise de
liste mémoire). Les balises anciennes et obsolètes sont également omises et jamais générées par ce gestionnaire de démarrage.

Le noyau s'exécute au niveau superviseur (ring 0 sur x86, EL1 sur ARM).

GDT non spécifié, mais valide. La pile est configurée dans les premiers 640k et augmente vers le bas (mais vous devriez la changer
dès que possible pour la pile que vous jugez digne).

Vous devez considérer IDT comme non spécifié ; IRQ, NMI et interruptions logicielles désactivées. Des gestionnaires d'exceptions
factices sont configurés pour afficher un vidage minimal et arrêter la machine. Ceux-ci ne doivent être utilisés que pour signaler
si votre noyau fait des ravages avant que vous puissiez configurer votre propre IDT et vos propres gestionnaires, de préférence
dès que possible. Sur ARM, vbar_el1 est configuré pour appeler les mêmes gestionnaires d'exceptions factices (bien qu'ils vident
bien sûr des registres différents).

Framebuffer est également défini par défaut. Vous pouvez modifier la résolution dans la configuration, mais si elle n'est pas
indiquée, le framebuffer est toujours configuré.

Il est important de ne jamais revenir de votre noyau. Vous êtes libre d'écraser n'importe quelle partie du chargeur en mémoire
(dès que vous avez terminé avec les balises MBI), il n'y a donc tout simplement nulle part où revenir. "Der Mohr hat seine
Schuldigkeit getan, der Mohr kann gehen."

Informations de démarrage transmises à votre noyau (MBI)
--------------------------------------------------------

Ce n'est pas évident au premier abord, mais la spécification Multiboot2 définit en réalité deux ensembles de balises totalement
indépendants:

- Le premier ensemble est censé être intégré dans un noyau compatible Multiboot2, appelé en-tête Multiboot2 de l'image du système
  d'exploitation (section 3.1.2), donc *fourni par le noyau*. **Easyboot** ne se soucie pas de ces balises et ne les analyse pas
  non plus dans votre noyau. Vous n'avez tout simplement pas besoin de données magiques spéciales intégrées dans votre fichier
  noyau, les en-têtes **Easyboot**, ELF et PE suffisent.

- Le deuxième ensemble est *transmis au noyau* dynamiquement au démarrage, **Easyboot** utilise uniquement ces balises. Cependant,
  il ne génère pas tout ce que Multiboot2 spécifie (il omet simplement les anciens, obsolètes ou hérités). Ces balises sont appelées
  balises MBI, voir [Informations de démarrage](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information-format)
  (section 3.6).

REMARQUE : la spécification Multiboot2 sur les balises MBI est complètement boguée. Vous pouvez trouver une version corrigée
ci-dessous, qui correspond au fichier d'en-tête multiboot2.h que vous pouvez trouver dans le référentiel source de GRUB.

Le premier paramètre de votre noyau est le magique 0x36d76289 (dans `rax`, `rcx` et `rdi`). Vous pouvez localiser les balises MBI
en utilisant le deuxième paramètre (dans `rbx`, `rdx` et `rsi`). Sur la plate-forme ARM, la magie est dans `x0` et l'adresse est
dans `x1`. Sur RISC-V et MIPS `a0` et `a1` utilisés respectivement. Si et quand ce chargeur est porté sur une autre architecture,
alors toujours les registres spécifiés par SysV ABI pour les arguments de fonction doivent toujours être utilisés. S'il existe
d'autres ABI courants sur la plate-forme qui n'interfèrent pas avec SysV ABI, alors les valeurs doivent également être dupliquées
dans les registres de ces ABI (ou en haut de la pile).

### En-têtes

L'adresse transmise est toujours alignée sur 8 octets et commence par un en-tête MBI:

```
        +-------------------+
u32     | total_size        |
u32     | reserved          |
        +-------------------+
```

Ceci est suivi d'une série de balises également alignées sur 8 octets. Chaque balise commence par les champs d'en-tête de balise
suivants:

```
        +-------------------+
u32     | type              |
u32     | size              |
        +-------------------+
```

`type` contient un identifiant du contenu du reste de la balise. `size` contient la taille de la balise, y compris les champs
d'en-tête mais sans le remplissage. Les balises se succèdent, complétées si nécessaire afin que chaque balise commence à une
adresse alignée sur 8 octets.

### Terminateur

```
        +-------------------+
u32     | type = 0          |
u32     | size = 8          |
        +-------------------+
```

Les balises se terminent par une balise de type `0` et de taille `8`.

### Ligne de commande de démarrage

```
        +-------------------+
u32     | type = 1          |
u32     | size              |
u8[n]   | string            |
        +-------------------+
```

`string` contient la ligne de commande spécifiée dans la ligne `kernel` de *menuentry* (sans le chemin et le nom de fichier du
noyau). La ligne de commande est une chaîne UTF-8 normale de style C terminée par zéro.

### Nom du chargeur de démarrage

```
        +----------------------+
u32     | type = 2             |
u32     | size = 17            |
u8[n]   | string "Easyboot"    |
        +----------------------+
```

`string` contient le nom d'un chargeur de démarrage démarrant le noyau. Le nom est une chaîne normale de style C UTF-8 terminée par
un zéro.

### Modules

```
        +-------------------+
u32     | type = 3          |
u32     | size              |
u32     | mod_start         |
u32     | mod_end           |
u8[n]   | string            |
        +-------------------+
```

Cette balise indique au noyau quel module de démarrage a été chargé avec l'image du noyau et où il peut être trouvé. Les éléments
`mod_start` et `mod_end` contiennent les adresses physiques de début et de fin du module de démarrage lui-même. Vous n'obtiendrez
jamais de tampon compressé gzip, car **Easyboot** les décompresse de manière transparente pour vous (et si vous fournissez un plugin,
il fonctionne également avec des données compressées autres que gzip). Le champ « string » fournit une chaîne arbitraire à associer
à ce module de démarrage particulier; il s'agit d'une chaîne UTF-8 normale de style C terminée par zéro. Spécifié dans la ligne
`module` de *menuentry* et son utilisation exacte est spécifique au système d'exploitation. Contrairement à la balise de ligne de
commande de démarrage, les balises de module *incluent également* le chemin et le nom de fichier du module.

Une balise apparaît par module. Ce type de balise peut apparaître plusieurs fois. Si un disque virtuel initial a été chargé avec
votre noyau, il apparaîtra comme premier module.

Il existe un cas particulier, si le fichier est une table DSDT ACPI, un blob FDT (dtb) ou GUDT, alors il n'apparaîtra pas en tant
que module, mais plutôt l'ancien RSDP ACPI (type 14) ou le nouveau RSDP ACPI (type 15) sera corrigé. et leur DSDT remplacé par le
contenu de ce fichier.

### Carte mémoire

Cette balise fournit une carte mémoire.

```
        +-------------------+
u32     | type = 6          |
u32     | size              |
u32     | entry_size = 24   |
u32     | entry_version = 0 |
varies  | entries           |
        +-------------------+
```

`size` contient la taille de toutes les entrées, y compris ce champ lui-même. `entry_size` est toujours 24. `entry_version` est
défini sur `0`. Chaque entrée a la structure suivante:

```
        +-------------------+
u64     | base_addr         |
u64     | length            |
u32     | type              |
u32     | reserved          |
        +-------------------+
```

`base_addr` est l'adresse physique de départ. `length` est la taille de la région mémoire en octets. `type` est la variété de plage
d'adresses représentée, où une valeur de `1` indique la RAM disponible, la valeur de `3` indique la mémoire utilisable contenant des
informations ACPI, la valeur de `4` indique la mémoire réservée qui doit être préservée en veille prolongée, la valeur `5` indique
une mémoire occupée par des modules RAM défectueux et toutes les autres valeurs indiquent actuellement une zone réservée. `reserved`
est défini sur `0` au démarrage du BIOS.

Lorsque le MBI est généré sur une machine UEFI, diverses entrées de carte mémoire EFI sont stockées sous le type `1` (RAM disponible)
ou `2` (RAM réservée), et si vous en avez besoin, le type de mémoire EFI d'origine est placé dans le champ `reserved`.

La carte fournie est garantie de répertorier toute la RAM standard qui devrait être disponible pour une utilisation normale, et elle
est toujours classée par `base_addr` croissant. Ce type de RAM disponible inclut cependant les régions occupées par le noyau, le mbi,
les segments et les modules. Le noyau doit veiller à ne pas écraser ces régions (**Easyboot** pourrait facilement exclure ces
régions, mais cela romprait la compatibilité Multiboot2).

### Informations sur le framebuffer

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

Le champ `framebuffer_addr` contient l'adresse physique du framebuffer. Le champ `framebuffer_pitch` contient la longueur d'une
ligne en octets. Les champs `framebuffer_width`, `framebuffer_height` contiennent les dimensions du framebuffer en pixels. Le champ
`framebuffer_bpp` contient le nombre de bits par pixel. `framebuffer_type` est toujours défini sur 1 et `reserved` contient toujours
0 dans la version actuelle de la spécification et doit être ignoré par l'image du système d'exploitation. Les champs restants
décrivent le format de pixel compressé, la position et la taille des canaux en bits. Vous pouvez utiliser l'expression
`((~(0xffffffff << size)) << position) & 0xffffffff` pour obtenir un masque de canal de type UEFI GOP.

### Pointeur de table système EFI 64 bits

Cette balise n'existe que si **Easyboot** est exécuté sur une machine UEFI. Sur une machine BIOS, cette balise n'a jamais été
générée.

```
        +-------------------+
u32     | type = 12         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Cette balise contient un pointeur vers la table système EFI.

### Pointeur de poignée d'image EFI 64 bits

Cette balise n'existe que si **Easyboot** est exécuté sur une machine UEFI. Sur une machine BIOS, cette balise n'a jamais été
générée.

```
        +-------------------+
u32     | type = 20         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Cette balise contient un pointeur vers le handle d’image EFI. Il s’agit généralement du handle de l’image du chargeur de démarrage.

### Tableaux SMBIOS

```
        +-------------------+
u32     | type = 13         |
u32     | size              |
u8      | major             |
u8      | minor             |
u8[6]   | reserved          |
        | smbios tables     |
        +-------------------+
```

Cette balise contient une copie des tables SMBIOS ainsi que leur version.

### L'ancien ACPI RSDP

```
        +-------------------+
u32     | type = 14         |
u32     | size              |
        | copy of RSDPv1    |
        +-------------------+
```

Cette balise contient une copie de RSDP tel que défini par la spécification ACPI 1.0. (Avec une adresse 32 bits.)

### Le nouveau ACPI RSDP

```
        +-------------------+
u32     | type = 15         |
u32     | size              |
        | copy of RSDPv2    |
        +-------------------+
```

Cette balise contient une copie de RSDP telle que définie par la spécification ACPI 2.0 ou ultérieure. (Avec une adresse 64 bits
probablement.)

Ceux-ci (types 14 et 15) pointent vers une table `RSDT` ou `XSDT` avec un pointeur vers une table `FACP`, qui à son tour contient
deux pointeurs vers une table `DSDT`, qui décrit la machine. **Easyboot** simule ces tables sur des machines qui ne prennent pas
en charge ACPI autrement. De plus, si vous fournissez une table DSDT, un blob FDT (dtb) ou GUDT en tant que module, **Easyboot**
corrigera les pointeurs pour pointer vers cette table fournie par l'utilisateur. Pour analyser ces tables, vous pouvez utiliser ma
bibliothèque à en-tête unique sans dépendance [hwdet](https://gitlab.com/bztsrc/hwdet) (ou la bibliothèque gonflée
[apcica](https://github.com/acpica/ acpica) et [libfdt](https://github.com/dgibson/dtc)).

Balises spécifiques au noyau
----------------------------

Les balises avec un `type` supérieur ou égal à 256 ne font pas partie de la spécification Multiboot2. Ceux-ci peuvent être ajoutés
à la liste par des [plugins](plugins.md) facultatifs, si un noyau en a besoin.

### EDID

```
        +-------------------+
u32     | type = 256        |
u32     | size              |
        | copy of EDID      |
        +-------------------+
```

Cette balise contient une copie de la liste des résolutions de moniteur prises en charge selon la spécification EDID.

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

Cette balise existe si la directive `multicore` a été donnée. `numcores` contient le nombre de cœurs de processeur dans le système,
`running` est le nombre de cœurs qui ont réussi à initialiser et à exécuter le même noyau en parallèle. Le `bspid` contient
l'identifiant du noyau BSP (sur l'identifiant lAPIC x86), afin que les noyaux puissent distinguer les AP et exécuter un code
différent sur ceux-ci. Tous les AP ont leur propre pile, et au-dessus de la pile se trouve l'identifiant du noyau actuel.

Disposition de la mémoire
-------------------------

### Machines BIOS

| Décalage | Fin     | Description                                                                       |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x400 | Interrupt Vector Table (utilisable, IDT en mode réel)                             |
|    0x400 |   0x4FF | BIOS Data Area (utilisable)                                                       |
|    0x4FF |   0x500 | Code du lecteur de démarrage du BIOS (très probablement 0x80, utilisable)         |
|    0x500 |  0x1000 | pile de gestionnaires d'exceptions (utilisable après avoir configuré votre IDT)   |
|   0x1000 |  0x8000 | tables de pagination (utilisable après avoir configuré vos tables de pagination)  |
|   0x8000 | 0x20000 | code et données du chargeur (utilisable après avoir configuré votre IDT)          |
|  0x20000 | 0x40000 | configuration + balises (utilisable après analyse MBI)                            |
|  0x40000 | 0x90000 | plugin ids; de haut en bas: la pile du noyau                                      |
|  0x90000 | 0x9A000 | Noyau Linux uniquement: zero page + cmdline                                       |
|  0x9A000 | 0xA0000 | Extended BIOS Data Area (mieux vaut ne pas toucher)                               |
|  0xA0000 | 0xFFFFF | VRAM et BIOS ROM (pas utilisable)                                                 |
| 0x100000 |       x | segments du noyau, suivis des modules, chaque page alignée                        |

### Machines UEFI

Personne ne sait. L'UEFI alloue la mémoire à sa guise. Attendez-vous à tout et à tout, mais très probablement placé en dessous de
64M. Toutes les zones seront sûrement répertoriées dans la carte mémoire comme type = 1 (`MULTIBOOT_MEMORY_AVAILABLE`) et reserved = 2
(`EfiLoaderData`), mais ce n'est pas exclusif, d'autres types de mémoire peuvent également être répertoriés comme ça (bss du
gestionnaire de démarrage rubrique par exemple).

### Raspberry Pi

| Décalage | Fin     | Description                                                                       |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x100 | réservé par firmware (mieux vaut ne pas toucher)                                  |
|    0x100 |  0x1000 | pile de gestionnaires d'exceptions (utilisable après avoir configuré votre VBAR)  |
|   0x1000 |  0x9000 | tables de pagination (utilisable après avoir configuré vos tables de pagination)  |
|   0x9000 | 0x20000 | code et données du chargeur (utilisable après avoir configuré votre VBAR)         |
|  0x20000 | 0x40000 | configuration + balises (utilisable après analyse MBI)                            |
|  0x40000 | 0x80000 | firmware fourni FDT (dtb); de haut en bas: la pile du noyau                       |
| 0x100000 |       x | segments du noyau, suivis des modules, chaque page alignée                        |

Les premiers octets sont réservés à [armstub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S). Seul le noyau 0
a démarré, donc pour démarrer les processeurs d'application, écrivez l'adresse d'une fonction dans 0xE0 (noyau 1), 0xE8 (noyau 2),
0xF0 (noyau 3), dont les adresses se trouvent dans cette zone.

Bien que non pris en charge nativement sur le RPi, vous obtenez toujours une ancienne balise RSDP (type 14) ACPI, avec de fausses
tables. La table `APIC` est utilisée pour communiquer le nombre de cœurs de processeur disponibles au noyau. L'adresse de la fonction
de démarrage est stockée dans le champ RSD PTR-> RSDT -> APIC -> cpu\[x].apic_id (et l'identifiant du noyau dans cpu\[x].acpi_id, où
BSP est toujours cpu\[0].acpi_id = 0 et cpu\[0].apic_id = 0xD8. Attention, "acpi" et "apic" se ressemblent beaucoup).

Si un blob FDT valide est transmis par le micrologiciel, ou si l'un des modules est un fichier .dtb, .gud ou .aml, alors une table
FADT (avec `FACP` magique) est également ajoutée. Dans ce tableau, le pointeur DSDT (32 bits, au décalage 40) pointe vers l’objet
blob aplati de l’arborescence des périphériques fourni.

Bien qu'aucune fonctionnalité de carte mémoire ne soit fournie par le micrologiciel, vous obtiendrez toujours une balise Carte
mémoire (type 6), répertoriant la RAM détectée et la région MMIO. Vous pouvez l'utiliser pour détecter l'adresse de base du MMIO,
qui est différente sur RPi3 et RPi4.
